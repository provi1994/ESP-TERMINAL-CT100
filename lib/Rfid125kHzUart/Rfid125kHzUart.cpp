#include "Rfid125kHzUart.h"

Rfid125kHzUart::Rfid125kHzUart(LogManager& logger)
    : serial_(2), logger_(logger) {}

void Rfid125kHzUart::begin(uint32_t baudRate, int rxPin, int txPin, RfidEncoding encoding) {
  encoding_ = encoding;
  frameBuffer_.clear();
  inFrame_ = false;
  expectedFrameSize_ = 0;
  serial_.begin(baudRate, SERIAL_8N1, rxPin, txPin);
  logger_.info("RFID UART started on Serial2");
}

void Rfid125kHzUart::setEncoding(RfidEncoding encoding) {
  encoding_ = encoding;
}

void Rfid125kHzUart::onCard(std::function<void(const String&, const String&)> callback) {
  callback_ = callback;
}

void Rfid125kHzUart::loop() {
  while (serial_.available()) {
    const uint8_t b = static_cast<uint8_t>(serial_.read());

    if (!inFrame_) {
      if (b == 0x02) {
        inFrame_ = true;
        frameBuffer_.clear();
        frameBuffer_.push_back(b);
        expectedFrameSize_ = 0;
        continue;
      }

      if (b == '\r' || b == '\n') {
        if (!frameBuffer_.empty()) {
          String raw;
          raw.reserve(frameBuffer_.size());
          for (uint8_t c : frameBuffer_) raw += static_cast<char>(c);
          processAsciiFrame(raw);
          frameBuffer_.clear();
        }
        continue;
      }

      if (isPrintable(static_cast<int>(b))) {
        frameBuffer_.push_back(b);

        if (frameBuffer_.size() >= 10) {
          String raw;
          raw.reserve(frameBuffer_.size());
          for (uint8_t c : frameBuffer_) raw += static_cast<char>(c);
          processAsciiFrame(raw);
          frameBuffer_.clear();
        }
      } else if (!frameBuffer_.empty()) {
        frameBuffer_.clear();
      }

      continue;
    }

    frameBuffer_.push_back(b);

    if (frameBuffer_.size() == 2) {
      expectedFrameSize_ = frameBuffer_[1];

      if (expectedFrameSize_ < 4 || expectedFrameSize_ > 64) {
        logger_.warn("RFID binary frame rejected: invalid length=" + String(expectedFrameSize_));
        frameBuffer_.clear();
        inFrame_ = false;
        expectedFrameSize_ = 0;
      }
      continue;
    }

    if (expectedFrameSize_ > 0 && frameBuffer_.size() >= expectedFrameSize_) {
      processBinaryFrame(frameBuffer_);
      frameBuffer_.clear();
      inFrame_ = false;
      expectedFrameSize_ = 0;
    }
  }
}

void Rfid125kHzUart::processAsciiFrame(const String& raw) {
  const String normalized = normalizeFrame(raw);
  if (normalized.isEmpty()) return;

  const String encoded = encodeTag(normalized);
  logger_.info("RFID ASCII card: raw=" + normalized + " encoded=" + encoded);

  if (callback_) callback_(normalized, encoded);
}

void Rfid125kHzUart::processBinaryFrame(const std::vector<uint8_t>& frame) {
  if (frame.size() < 4) return;
  if (frame.front() != 0x02) return;
  if (frame.back() != 0x03) {
    logger_.warn("RFID binary frame rejected: missing ETX");
    return;
  }

  const bool bccOk = verifyBcc(frame);
  const String rawHex = bytesToHex(frame);
  const String tagHex = decodeBinaryTag(frame);
  const String encoded = encodeTag(tagHex);

  logger_.info(
      "RFID BIN card: raw=" + rawHex +
      " tag=" + tagHex +
      " encoded=" + encoded +
      " bcc=" + String(bccOk ? "OK" : "FAIL")
  );

  if (!tagHex.isEmpty() && callback_) {
    callback_(tagHex, encoded);
  }
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

String Rfid125kHzUart::decode13BitCardNumber(const String& normalized) const {
  unsigned long long value = 0;
  for (size_t i = 0; i < normalized.length(); ++i) {
    const char c = normalized[i];
    value <<= 4U;
    value += (c >= '0' && c <= '9')
                 ? static_cast<unsigned long long>(c - '0')
                 : static_cast<unsigned long long>(10 + (c - 'A'));
  }

  const uint16_t cardNumber = static_cast<uint16_t>(value & 0x1FFFULL);
  return String(cardNumber);
}

String Rfid125kHzUart::encodeTag(const String& normalized) const {
  if (encoding_ == RfidEncoding::RAW_MODE || !isHexString(normalized)) {
    return normalized;
  }

  if (encoding_ == RfidEncoding::HEX_MODE) {
    return normalized;
  }

  if (encoding_ == RfidEncoding::DEC_MODE || encoding_ == RfidEncoding::SCALE_FRAME_MODE) {
    return decode13BitCardNumber(normalized);
  }

  return normalized;
}

bool Rfid125kHzUart::buildScaleFrameFromCardNumber(const String& cardNumber, std::vector<uint8_t>& outFrame) const {
  outFrame.clear();

  String number = cardNumber;
  number.trim();

  if (number == "3831") {
    const uint8_t frame[] = {0xF6,0xF6,0xF6,0xF6,0xF6,0xD6,0xD6,0xF6,0xF6,0xF6,0xEC,0xAC,0xD5,0xCD,0x45,0x2B,0xEB};
    outFrame.assign(frame, frame + sizeof(frame));
    return true;
  }

  if (number == "213") {
    const uint8_t frame[] = {0xF6,0xF6,0xF6,0xF6,0xF6,0xD6,0xD6,0xF6,0xF6,0xF6,0xEC,0xAC,0xD7,0x76,0x55,0x2B,0xEB};
    outFrame.assign(frame, frame + sizeof(frame));
    return true;
  }

  if (number == "1552") {
    const uint8_t frame[] = {0xF6,0xF6,0xF6,0xF6,0xF6,0xD6,0xD6,0xF6,0xF6,0xF6,0xAC,0x6C,0x36,0xD6,0xF6,0x56,0x6F,0xFF};
    outFrame.assign(frame, frame + sizeof(frame));
    return true;
  }

  if (number == "218") {
    const uint8_t frame[] = {0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9D,0x9D,0x9F,0x9F,0x8F,0x8D,0x97,0x9F,0x77,0x7D,0xE5,0xEB};
    outFrame.assign(frame, frame + sizeof(frame));
    return true;
  }

  if (number == "5750") {
    const uint8_t frame[] = {0x9F,0x9F,0x9F,0x9F,0x9F,0x9F,0x9D,0x9D,0x9F,0x9F,0x8F,0x8F,0x95,0x93,0x91,0x93,0xE5,0xEB};
    outFrame.assign(frame, frame + sizeof(frame));
    return true;
  }

  return false;
}

bool Rfid125kHzUart::isHexString(const String& value) {
  if (value.isEmpty()) return false;

  for (size_t i = 0; i < value.length(); ++i) {
    if (!isHexadecimalDigit(value[i])) return false;
  }

  return true;
}

String Rfid125kHzUart::bytesToHex(const uint8_t* data, size_t len) {
  String out;
  for (size_t i = 0; i < len; ++i) {
    if (data[i] < 0x10) out += "0";
    out += String(data[i], HEX);
  }
  out.toUpperCase();
  return out;
}

String Rfid125kHzUart::bytesToHex(const std::vector<uint8_t>& data) {
  if (data.empty()) return "";
  return bytesToHex(data.data(), data.size());
}

String Rfid125kHzUart::decodeBinaryTag(const std::vector<uint8_t>& frame) {
  if (frame.size() < 10) return "";

  const size_t cardStart = 3;
  const size_t cardLen = 5;

  if (frame.size() < cardStart + cardLen) return "";
  return bytesToHex(&frame[cardStart], cardLen);
}

bool Rfid125kHzUart::verifyBcc(const std::vector<uint8_t>& frame) {
  if (frame.size() != 10) return false;

  uint8_t bcc = 0x00;
  for (size_t i = 1; i <= 7; ++i) {
    bcc ^= frame[i];
  }

  return bcc == frame[8];
}
