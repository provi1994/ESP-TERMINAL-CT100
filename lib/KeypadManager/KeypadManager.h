#pragma once

#include <Wire.h>
#include <functional>

#include "LogManager.h"

class KeypadManager {
 public:
  explicit KeypadManager(LogManager& logger);

  bool begin(uint8_t i2cAddress, uint8_t sdaPin, uint8_t sclPin);
  void loop();
  void onKey(std::function<void(char)> callback);

 private:
  LogManager& logger_;
  TwoWire* wire_ = &Wire;
  uint8_t address_ = 0x20;
  bool initialized_ = false;
  bool reversedWiring_ = false;

  uint8_t lastRawState_ = 0xFF;
  char lastStableKey_ = 0;
  uint32_t lastDebounceMs_ = 0;
  uint32_t lastDiagnosticMs_ = 0;
  uint32_t lastScanMs_ = 0;

  std::function<void(char)> callback_;

  static constexpr char kMap[4][4] = {
      {'1', '2', '3', 'A'},
      {'4', '5', '6', 'B'},
      {'7', '8', '9', 'C'},
      {'*', '0', '#', 'D'},
  };

  bool probeAddress(uint8_t address);
  bool writeByte(uint8_t value);
  bool readByte(uint8_t& value);

  String scanI2cBus();
  void publishDiagnostics(bool forceScan);

  char scanOnce();
  char scanRowsLowColsHigh();
  char scanColsLowRowsHigh();
};
