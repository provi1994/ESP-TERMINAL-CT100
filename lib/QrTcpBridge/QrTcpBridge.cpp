#include "QrTcpBridge.h"

QrTcpBridge::QrTcpBridge(LogManager& logger) : logger_(logger) {}

void QrTcpBridge::begin(uint16_t port) {
    stop();
    server_ = new WiFiServer(port);
    server_->begin();
    server_->setNoDelay(true);
    enabled_ = true;
    stats_ = Stats{};
    stats_.enabled = true;
    stats_.port = port;
    logger_.warn("QR TCP bridge started on port " + String(port));
}

void QrTcpBridge::stop() {
    if (client_.connected()) client_.stop();
    if (server_ != nullptr) {
        server_->stop();
        delete server_;
        server_ = nullptr;
    }
    enabled_ = false;
    stats_ = Stats{};
}

String QrTcpBridge::toHexByte(uint8_t b) {
    static const char* HEX_DIGITS = "0123456789ABCDEF";
    String out;
    out += HEX_DIGITS[(b >> 4) & 0x0F];
    out += HEX_DIGITS[b & 0x0F];
    return out;
}

void QrTcpBridge::pushHexSample(String& dst, uint8_t b) {
    if (!dst.isEmpty()) dst += ' ';
    dst += toHexByte(b);
    if (dst.length() > 96) dst.remove(0, dst.length() - 96);
}

void QrTcpBridge::loop(HardwareSerial& serial) {
    if (!enabled_ || server_ == nullptr) return;

    if (!client_.connected()) {
        WiFiClient incoming = server_->available();
        if (incoming) {
            client_ = incoming;
            client_.setNoDelay(true);
            logger_.warn("QR TCP bridge client connected");
        }
    }

    stats_.clientConnected = client_.connected();

    if (client_.connected()) {
        while (client_.available()) {
            uint8_t b = static_cast<uint8_t>(client_.read());
            serial.write(b);
            stats_.rxBytesFromTcp++;
            stats_.txBytesToQr++;
            pushHexSample(stats_.lastTxHex, b);
        }
        serial.flush();
    }

    while (serial.available()) {
        uint8_t b = static_cast<uint8_t>(serial.read());
        stats_.rxBytesFromQr++;
        pushHexSample(stats_.lastRxHex, b);
        if (client_.connected()) {
            client_.write(&b, 1);
            stats_.txBytesToTcp++;
        }
    }
}
