#include "Rfid125kHzUart.h"

Rfid125kHzUart::Rfid125kHzUart(LogManager& logger) : serial_(2), logger_(logger) {}

void Rfid125kHzUart::begin(uint32_t baudRate, int rxPin, int txPin, RfidEncoding encoding) {
  encoding_ = encoding;
  serial_.begin(baudRate, SERIAL_8N1, rxPin, txPin);
  logger_.info("RFID UART started on Serial2");
}

void Rfid125kHzUart::setEncoding(RfidEncoding encoding) { encoding_ = encoding; }

void Rfid125kHzUart::onCard(std::function<void(const String&, const String&)> callback) { callback_ = callback; }

void Rfid125kHzUart::loop() {
  while (serial_.available()) {
    const char c = static_cast<char>(serial_.read());
    if (c == '\r' || c == '\n') {
      if (!buffer_.isEmpty()) {
        processFrame(buffer_);
        buffer_.clear();
      }
    } else if (isPrintable(static_cast<unsigned char>(c))) {
      buffer_ += c;
      if (buffer_.length() > 32) {
        processFrame(buffer_);
        buffer_.clear();
      }
    }
  }
}

void Rfid125kHzUart::processFrame(const String& raw) {
  const String normalized = normalizeFrame(raw);
  if (normalized.isEmpty()) return;

  const String encoded = encodeTag(normalized);
  logger_.info("RFID card: raw=" + normalized + " encoded=" + encoded);
  if (callback_) callback_(normalized, encoded);
}

String Rfid125kHzUart::normalizeFrame(const String& raw) const {
  String out;
  for (size_t i = 0; i < raw.length(); ++i) {
    const char c = raw[i];
    if (isHexadecimalDigit(c)) {
      out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
  }
  if (out.length() >= 8 && isHexString(out)) return out;
  return raw;
}

String Rfid125kHzUart::encodeTag(const String& normalized) const {
  if (encoding_ == RfidEncoding::RAW_MODE || !isHexString(normalized)) {
    return normalized;
  }
  if (encoding_ == RfidEncoding::HEX_MODE) {
    return normalized;
  }

  unsigned long long value = 0;
  for (size_t i = 0; i < normalized.length(); ++i) {
    const char c = normalized[i];
    value <<= 4U;
    value += (c >= '0' && c <= '9') ? static_cast<unsigned long long>(c - '0')
                                     : static_cast<unsigned long long>(10 + (c - 'A'));
  }
  return String(value);
}

bool Rfid125kHzUart::isHexString(const String& value) {
  if (value.isEmpty()) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isHexadecimalDigit(value[i])) return false;
  }
  return true;
}
