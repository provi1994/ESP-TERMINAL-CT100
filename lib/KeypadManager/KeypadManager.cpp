#include "KeypadManager.h"

constexpr char KeypadManager::kMap[4][4];

KeypadManager::KeypadManager(LogManager& logger) : logger_(logger) {}

bool KeypadManager::probeAddress(uint8_t address) {
  wire_->beginTransmission(address);
  return wire_->endTransmission() == 0;
}

String KeypadManager::scanI2cBus() {
  String found;
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

bool KeypadManager::autoDetectAddress() {
  // PCF8574A / PCF8574AN: 0x38-0x3F
  // Dla A0=A1=A2=GND typowy adres to 0x38.
  for (uint8_t addr = 0x38; addr <= 0x3F; ++addr) {
    if (probeAddress(addr)) {
      address_ = addr;
      logger_.warn("PCF8574A/AN keypad auto-detected at 0x" + String(address_, HEX));
      return true;
    }
  }

  // Fallback dla starszego PCF8574 / PCF8574N: 0x20-0x27.
  // Zostawione celowo, zeby ten sam firmware dzialal z obiema rodzinami ukladu.
  for (uint8_t addr = 0x20; addr <= 0x27; ++addr) {
    if (probeAddress(addr)) {
      address_ = addr;
      logger_.warn("PCF8574/N keypad auto-detected at 0x" + String(address_, HEX));
      return true;
    }
  }

  return false;
}

bool KeypadManager::begin(uint8_t i2cAddress, uint8_t sdaPin, uint8_t sclPin) {
  wire_->begin(sdaPin, sclPin);

  address_ = i2cAddress;
  initialized_ = false;
  reversedWiring_ = false;
  lastRawState_ = 0xFF;
  lastStableKey_ = 0;
  lastDebounceMs_ = 0;
  lastDiagnosticMs_ = 0;
  lastScanMs_ = 0;

  logger_.info("PCF8574 keypad init. Config address: 0x" + String(address_, HEX));
  logger_.info("PCF8574A/AN I2C range: 0x38-0x3F");
  logger_.info("PCF8574/N fallback I2C range: 0x20-0x27");

  initialized_ = probeAddress(address_);

  if (!initialized_) {
    initialized_ = autoDetectAddress();
  }

  if (initialized_) {
    initialized_ = writeByte(0xFF);
  }

  if (initialized_) {
    logger_.info("PCF8574 keypad ready at 0x" + String(address_, HEX));
    logger_.info("I2C scan: " + scanI2cBus());
  } else {
    logger_.error("PCF8574 keypad not detected. Checked 0x38-0x3F and 0x20-0x27");
    logger_.error("Check: VCC, GND, SDA=GPIO33, SCL=GPIO32, address pins A0/A1/A2");
    logger_.info("I2C scan: " + scanI2cBus());
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
  const uint32_t now = millis();

  if (!forceScan && now - lastDiagnosticMs_ < 5000UL) {
    return;
  }

  lastDiagnosticMs_ = now;

  logger_.info("KEYPAD diag: initialized=" + String(initialized_ ? "YES" : "NO") +
               ", address=0x" + String(address_, HEX) +
               ", raw=0x" + String(lastRawState_, HEX) +
               ", wiring=" + String(reversedWiring_ ? "P0-P3=COL,P4-P7=ROW" : "P0-P3=ROW,P4-P7=COL/auto"));

  if (forceScan || now - lastScanMs_ > 30000UL) {
    lastScanMs_ = now;
    logger_.info("KEYPAD I2C scan: " + scanI2cBus());
  }
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
      logger_.warn("PCF8574 keypad wiring detected: P0-P3=COL, P4-P7=ROW");
    }

    return key;
  }

  key = scanColsLowRowsHigh();
  if (key) return key;

  return scanRowsLowColsHigh();
}

void KeypadManager::loop() {
  if (!initialized_) {
    publishDiagnostics(false);
    return;
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

    logger_.info("Keypad key: " + String(key));

    // Tylko callback. Wysylka TCP 4012 i glownego TCP jest obslugiwana w src/main.cpp
    // przez keypadTcpManager oraz tcpManager. Dzieki temu nie ma dwoch serwerow na porcie 4012.
    if (callback_) {
      callback_(key);
    }
  }
}
