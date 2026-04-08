#include "KeypadManager.h"

constexpr char KeypadManager::kMap[4][4];

KeypadManager::KeypadManager(LogManager& logger) : logger_(logger) {}

bool KeypadManager::begin(uint8_t i2cAddress, uint8_t sdaPin, uint8_t sclPin) {
  address_ = i2cAddress;
  wire_->begin(sdaPin, sclPin);
  state_ = 0xFF;
  initialized_ = writeByte(state_);
  if (initialized_) {
    logger_.info("PCF8574 keypad ready at 0x" + String(address_, HEX));
  } else {
    logger_.error("PCF8574 keypad not detected at 0x" + String(address_, HEX));
  }
  return initialized_;
}

void KeypadManager::onKey(std::function<void(char)> callback) { callback_ = callback; }

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
  return true;
}

char KeypadManager::scanOnce() {
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

void KeypadManager::loop() {
  const char key = scanOnce();
  const uint32_t now = millis();
  static char lastReported = 0;

  if (key != lastStableKey_) {
    lastDebounceMs_ = now;
    lastStableKey_ = key;
    if (key == 0) lastReported = 0;
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
    logger_.info(String("Keypad key: ") + key);
    if (callback_) callback_(key);
  }
}
