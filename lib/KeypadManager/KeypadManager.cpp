#include "KeypadManager.h"

#include <WiFiClient.h>
#include <WiFiServer.h>

constexpr char KeypadManager::kMap[4][4];

// Diagnostyka klawiatury i PCF8574N idzie po TCP 4012.
// Dzieki temu nie trzeba podmieniac src/main.cpp.
static constexpr uint16_t KEYPAD_TCP_PORT = 4012;
static WiFiServer keypadTcpServer(KEYPAD_TCP_PORT);
static WiFiClient keypadTcpClient;
static bool keypadTcpStarted = false;

static String byteToBinary(uint8_t value) {
  String out;
  out.reserve(8);
  for (int i = 7; i >= 0; --i) {
    out += ((value >> i) & 0x01) ? '1' : '0';
  }
  return out;
}

static void tcpPrint(const String& line) {
  if (keypadTcpClient && keypadTcpClient.connected()) {
    keypadTcpClient.print(line + "\r\n");
  }
}

static void keypadTcpBegin(LogManager& logger) {
  if (keypadTcpStarted) return;

  keypadTcpServer.begin();
  keypadTcpServer.setNoDelay(true);
  keypadTcpStarted = true;

  logger.info("KEYPAD TCP diagnostic server listening on port " + String(KEYPAD_TCP_PORT));
}

static bool keypadTcpLoop(LogManager& logger) {
  if (!keypadTcpStarted) return false;

  bool connectedNow = false;

  if (!keypadTcpClient || !keypadTcpClient.connected()) {
    WiFiClient newClient = keypadTcpServer.available();

    if (newClient) {
      keypadTcpClient.stop();
      keypadTcpClient = newClient;
      keypadTcpClient.setNoDelay(true);
      connectedNow = true;

      logger.info("KEYPAD TCP diagnostic client connected on port " + String(KEYPAD_TCP_PORT));

      tcpPrint("");
      tcpPrint("=== ESP-TERMINAL PCF8574N DIAGNOSTYKA ===");
      tcpPrint("TCP port: 4012");
      tcpPrint("Komendy: HELP, SCAN, RAW");
      tcpPrint("PCF8574N expected range: 0x20-0x27");
      tcpPrint("PCF8574A range, informacyjnie: 0x38-0x3F");
      tcpPrint("==========================================");
    }
  }

  // Obsluga prostych komend wpisanych przez TCP, np. w netcat.
  static String rx;
  while (keypadTcpClient && keypadTcpClient.connected() && keypadTcpClient.available()) {
    char c = static_cast<char>(keypadTcpClient.read());
    if (c == '\r') continue;

    if (c == '\n') {
      rx.trim();
      rx.toUpperCase();

      if (rx == "HELP") {
        tcpPrint("HELP:");
        tcpPrint("  SCAN - skan calej magistrali I2C");
        tcpPrint("  RAW  - jednorazowy odczyt bajtu PCF P7..P0");
        tcpPrint("  HELP - ta pomoc");
      } else if (rx == "SCAN") {
        tcpPrint("SCAN command accepted - wynik pojawi sie w nastepnej paczce diagnostyki");
      } else if (rx == "RAW") {
        tcpPrint("RAW command accepted - stan PCF pojawi sie w nastepnej paczce diagnostyki");
      } else if (rx.length() > 0) {
        tcpPrint("Nieznana komenda: " + rx);
        tcpPrint("Wpisz HELP");
      }

      rx = "";
    } else {
      if (rx.length() < 32) rx += c;
    }
  }

  return connectedNow;
}

static bool keypadTcpSendLine(LogManager& logger, const String& line) {
  keypadTcpLoop(logger);

  if (!keypadTcpClient || !keypadTcpClient.connected()) {
    logger.warn("KEYPAD TCP 4012 no client: " + line);
    return false;
  }

  keypadTcpClient.print(line + "\r\n");
  return true;
}

KeypadManager::KeypadManager(LogManager& logger) : logger_(logger) {}

bool KeypadManager::probeAddress(uint8_t address) {
  wire_->beginTransmission(address);
  return wire_->endTransmission() == 0;
}

String KeypadManager::scanI2cBus() {
  String found = "";
  uint8_t count = 0;

  for (uint8_t addr = 1; addr < 127; ++addr) {
    if (probeAddress(addr)) {
      if (found.length() > 0) found += ", ";
      if (addr < 16) found += "0x0";
      else found += "0x";
      found += String(addr, HEX);
      found.toUpperCase();
      count++;
    }
  }

  if (count == 0) {
    return "brak urzadzen I2C";
  }

  return found;
}

bool KeypadManager::begin(uint8_t i2cAddress, uint8_t sdaPin, uint8_t sclPin) {
  wire_->begin(sdaPin, sclPin);

  address_ = i2cAddress;
  initialized_ = false;
  reversedWiring_ = false;
  lastRawState_ = 0xFF;

  // Start TCP zawsze, nawet gdy PCF nie odpowiada.
  // Dzieki temu zobaczysz diagnostyke po TCP 4012 nawet przy blednym adresie albo kablach.
  keypadTcpBegin(logger_);

  logger_.info("PCF8574N keypad init. Config address: 0x" + String(address_, HEX));
  logger_.info("PCF8574N expected I2C range: 0x20-0x27");

  initialized_ = probeAddress(address_);

  // PCF8574N: typowy zakres adresow 0x20-0x27.
  if (!initialized_) {
    for (uint8_t addr = 0x20; addr <= 0x27; ++addr) {
      if (probeAddress(addr)) {
        address_ = addr;
        initialized_ = true;
        logger_.warn("PCF8574N keypad auto-detected at 0x" + String(address_, HEX));
        break;
      }
    }
  }

  if (initialized_) {
    initialized_ = writeByte(0xFF);
  }

  if (initialized_) {
    logger_.info("PCF8574N keypad ready at 0x" + String(address_, HEX));
  } else {
    logger_.error("PCF8574N keypad not detected in range 0x20-0x27");
  }

  return initialized_;
}

void KeypadManager::onKey(std::function<void(char)> callback) {
  callback_ = callback;
}

bool KeypadManager::writeByte(uint8_t value) {
  wire_->beginTransmission(address_);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

bool KeypadManager::readByte(uint8_t& value) {
  if (wire_->requestFrom(static_cast<int>(address_), 1) != 1) {
    return false;
  }

  value = wire_->read();
  lastRawState_ = value;
  return true;
}

void KeypadManager::publishDiagnostics(bool forceScan) {
  if (!keypadTcpClient || !keypadTcpClient.connected()) return;

  const uint32_t now = millis();

  if (!forceScan && now - lastDiagnosticMs_ < 1000UL) {
    return;
  }

  lastDiagnosticMs_ = now;

  tcpPrint("");
  tcpPrint("--- PCF8574N DIAGNOSTYKA ---");
  tcpPrint("millis=" + String(now));
  tcpPrint("pcf_initialized=" + String(initialized_ ? "YES" : "NO"));
  tcpPrint("pcf_active_address=0x" + String(address_, HEX));
  tcpPrint("pcf_expected_N_range=0x20-0x27");

  if (forceScan || now - lastScanMs_ > 5000UL) {
    lastScanMs_ = now;
    tcpPrint("i2c_scan=" + scanI2cBus());
  }

  if (initialized_) {
    uint8_t raw = 0xFF;

    // Ustawiamy wszystkie piny jako wejscia z pull-up PCF,
    // potem czytamy P7..P0.
    writeByte(0xFF);

    if (readByte(raw)) {
      tcpPrint("pcf_raw_bin_P7_P0=" + byteToBinary(raw));
      tcpPrint("pcf_raw_hex=0x" + String(raw, HEX));
      tcpPrint("P0=" + String((raw & 0x01) ? "HIGH" : "LOW") +
               " P1=" + String((raw & 0x02) ? "HIGH" : "LOW") +
               " P2=" + String((raw & 0x04) ? "HIGH" : "LOW") +
               " P3=" + String((raw & 0x08) ? "HIGH" : "LOW"));
      tcpPrint("P4=" + String((raw & 0x10) ? "HIGH" : "LOW") +
               " P5=" + String((raw & 0x20) ? "HIGH" : "LOW") +
               " P6=" + String((raw & 0x40) ? "HIGH" : "LOW") +
               " P7=" + String((raw & 0x80) ? "HIGH" : "LOW"));
      tcpPrint("wiring_mode=" + String(reversedWiring_ ? "P0-P3=COL, P4-P7=ROW" : "P0-P3=ROW, P4-P7=COL or auto"));
    } else {
      tcpPrint("pcf_raw_read=ERROR");
    }
  } else {
    tcpPrint("pcf_raw_read=SKIPPED_NO_PCF");
    tcpPrint("sprawdz: VCC, GND, SDA=GPIO33, SCL=GPIO32, adres A0/A1/A2");
  }

  tcpPrint("----------------------------");
}

// Standardowe podlaczenie:
// P0-P3 = ROW
// P4-P7 = COL
char KeypadManager::scanRowsLowColsHigh() {
  if (!initialized_) return 0;

  for (uint8_t row = 0; row < 4; ++row) {
    uint8_t out = 0xFF;
    out &= static_cast<uint8_t>(~(1U << row));

    if (!writeByte(out)) return 0;
    delayMicroseconds(150);

    uint8_t in = 0xFF;
    if (!readByte(in)) return 0;

    for (uint8_t col = 0; col < 4; ++col) {
      const uint8_t mask = static_cast<uint8_t>(1U << (4 + col));

      if ((in & mask) == 0U) {
        writeByte(0xFF);
        return kMap[row][col];
      }
    }
  }

  writeByte(0xFF);
  return 0;
}

// Odwrotne podlaczenie:
// P0-P3 = COL
// P4-P7 = ROW
char KeypadManager::scanColsLowRowsHigh() {
  if (!initialized_) return 0;

  for (uint8_t col = 0; col < 4; ++col) {
    uint8_t out = 0xFF;
    out &= static_cast<uint8_t>(~(1U << (4 + col)));

    if (!writeByte(out)) return 0;
    delayMicroseconds(150);

    uint8_t in = 0xFF;
    if (!readByte(in)) return 0;

    for (uint8_t row = 0; row < 4; ++row) {
      const uint8_t mask = static_cast<uint8_t>(1U << row);

      if ((in & mask) == 0U) {
        writeByte(0xFF);
        return kMap[row][col];
      }
    }
  }

  writeByte(0xFF);
  return 0;
}

char KeypadManager::scanOnce() {
  char key = 0;

  if (!reversedWiring_) {
    key = scanRowsLowColsHigh();
    if (key) return key;

    key = scanColsLowRowsHigh();
    if (key) {
      reversedWiring_ = true;
      logger_.warn("PCF8574N keypad wiring detected: P0-P3=COL, P4-P7=ROW");
      tcpPrint("AUTO WIRING: P0-P3=COL, P4-P7=ROW");
    }

    return key;
  }

  key = scanColsLowRowsHigh();
  if (key) return key;

  return scanRowsLowColsHigh();
}

void KeypadManager::loop() {
  const bool connectedNow = keypadTcpLoop(logger_);

  if (connectedNow) {
    publishDiagnostics(true);
  } else {
    publishDiagnostics(false);
  }

  const char key = scanOnce();
  const uint32_t now = millis();
  static char lastReported = 0;

  if (key != lastStableKey_) {
    lastDebounceMs_ = now;
    lastStableKey_ = key;

    if (key == 0) {
      lastReported = 0;
    }

    return;
  }

  if (key == 0) {
    lastReported = 0;
    return;
  }

  if (now - lastDebounceMs_ < 30) {
    return;
  }

  if (key != lastReported) {
    lastReported = key;

    const String payload = "KEY:" + String(key);

    logger_.info("Keypad key: " + String(key));
    tcpPrint("");
    tcpPrint("EVENT " + payload);

    const bool sent4012 = keypadTcpSendLine(logger_, payload);
    logger_.info(String("KEYPAD TCP 4012 send: ") + (sent4012 ? "OK" : "NO CLIENT"));

    if (callback_) {
      callback_(key);
    }
  }
}
