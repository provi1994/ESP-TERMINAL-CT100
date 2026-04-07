#pragma once

#include <HardwareSerial.h>
#include <functional>
#include <AppTypes.h>
#include "LogManager.h"

class Rfid125kHzUart {
 public:
  explicit Rfid125kHzUart(LogManager& logger);

  void begin(uint32_t baudRate, int rxPin, int txPin, RfidEncoding encoding);
  void setEncoding(RfidEncoding encoding);
  void loop();
  void onCard(std::function<void(const String&, const String&)> callback);

 private:
  HardwareSerial serial_;
  LogManager& logger_;
  RfidEncoding encoding_ = RfidEncoding::HEX_MODE;
  String buffer_;
  std::function<void(const String&, const String&)> callback_;

  void processFrame(String raw);
  String normalizeFrame(const String& raw) const;
  String encodeTag(const String& normalized) const;
  static bool isHexString(const String& value);
};
