#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <functional>
#include <vector>

#include "AppTypes.h"
#include "LogManager.h"

class Rfid125kHzUart {
public:
  explicit Rfid125kHzUart(LogManager& logger);

  void begin(uint32_t baudRate, int rxPin, int txPin, RfidEncoding encoding);
  void setEncoding(RfidEncoding encoding);
  void loop();
  void onCard(std::function<void(const String&, const String&)> callback);

  bool buildScaleFrame(const String& encoded, std::vector<uint8_t>& outFrame) const;

private:
  HardwareSerial serial_;
  LogManager& logger_;
  RfidEncoding encoding_ = RfidEncoding::HEX_MODE;
  std::function<void(const String&, const String&)> callback_;

  std::vector<uint8_t> frameBuffer_;
  bool inFrame_ = false;
  size_t expectedFrameSize_ = 0;

  void processAsciiFrame(const String& raw);
  void processBinaryFrame(const std::vector<uint8_t>& frame);

  String normalizeFrame(const String& raw) const;
  String encodeTag(const String& normalized) const;
  String hexToDecString(const String& normalized) const;

  static bool isHexString(const String& value);
  static String bytesToHex(const uint8_t* data, size_t len);
  static String bytesToHex(const std::vector<uint8_t>& data);
  static String decodeBinaryTag(const std::vector<uint8_t>& frame);
  static bool verifyBcc(const std::vector<uint8_t>& frame);
};
