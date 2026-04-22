#include "QrCamGm805.h"

#include <ctype.h>
#include <stdlib.h>

QrCamGm805::QrCamGm805(LogManager& logger, HardwareSerial& serial)
    : logger_(logger), serial_(serial) {
    diag_.runtimeStatus = "OFF";
}

void QrCamGm805::begin(const QrSettings& settings, int rxPin, int txPin) {
    settings_ = settings;
    rxPin_ = rxPin;
    txPin_ = txPin;
    serial_.begin(settings_.baudRate, SERIAL_8N1, rxPin_, txPin_);
    diag_.runtimeStatus = settings_.enabled ? "READY" : "OFF";
    resetCurrentFrame();
}

void QrCamGm805::applySettings(const QrSettings& settings) {
    settings_ = settings;
    if (rxPin_ >= 0 || txPin_ >= 0) {
        serial_.updateBaudRate(settings_.baudRate);
    }
}

void QrCamGm805::onDecoded(std::function<void(const String&)> cb) {
    onDecoded_ = cb;
}

QrCamGm805::Diagnostics QrCamGm805::diagnostics() const {
    return diag_;
}

String QrCamGm805::trimCopy(const String& value) {
    String out = value;
    out.trim();
    return out;
}

String QrCamGm805::cleanHexLine(const String& input) {
    String line = input;
    line.trim();
    const int hashPos = line.indexOf('#');
    if (hashPos >= 0) line = line.substring(0, hashPos);
    String out;
    out.reserve(line.length());
    for (size_t i = 0; i < line.length(); ++i) {
        const char c = line[i];
        if (isxdigit((unsigned char)c)) out += (char)toupper((unsigned char)c);
    }
    return out;
}

String QrCamGm805::bytesToHex(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return "";

    static const char* HEX_DIGITS = "0123456789ABCDEF";
    String out;
    out.reserve(len * 3);

    for (size_t i = 0; i < len; ++i) {
        if (i) out += ' ';
        out += HEX_DIGITS[(data[i] >> 4) & 0x0F];
        out += HEX_DIGITS[data[i] & 0x0F];
    }

    return out;
}

bool QrCamGm805::sendHexCommand(const String& inputHex, const char* tag) {
    String hex = cleanHexLine(inputHex);
    if (hex.isEmpty()) {
        diag_.lastCommandStatus = "EMPTY";
        return false;
    }
    if ((hex.length() % 2) != 0) {
        diag_.lastCommandHex = hex;
        diag_.lastCommandStatus = "HEX_LEN_ERR";
        logger_.error(String("GM805 invalid hex len ") + tag + ": " + hex);
        return false;
    }

    diag_.lastCommandHex = hex;
    for (size_t i = 0; i < hex.length(); i += 2) {
        const String byteText = hex.substring(i, i + 2);
        const uint8_t value = (uint8_t)strtoul(byteText.c_str(), nullptr, 16);
        serial_.write(value);
    }
    serial_.flush();
    diag_.lastCommandStatus = "OK";
    logger_.info(String("GM805 cmd ") + tag + ": " + hex);
    return true;
}

void QrCamGm805::applyStartupCommands() {
    if (!settings_.enabled || !settings_.applyStartupCommands) return;
    if (settings_.startupCommandsHex.isEmpty()) return;

    delay(settings_.startupCommandDelayMs);
    int start = 0;
    const String all = settings_.startupCommandsHex;
    while (start <= (int)all.length()) {
        int end = all.indexOf('\n', start);
        if (end < 0) end = all.length();
        String line = all.substring(start, end);
        line.trim();
        if (!line.isEmpty()) sendHexCommand(line, "startup");
        start = end + 1;
        delay(settings_.interCommandDelayMs);
    }
    if (settings_.saveToFlashAfterApply) {
        sendHexCommand("7E 00 09 01 00 00 00 DE C8", "save_flash");
    }
}

void QrCamGm805::resetCurrentFrame() {
    currentAscii_ = "";
    rawLen_ = 0;
    diag_.currentBytesCount = 0;
    diag_.frameOpen = false;
}

void QrCamGm805::pushByte(uint8_t b) {
    if (rawLen_ < sizeof(rawFrame_)) {
        rawFrame_[rawLen_++] = b;
    }
    if (currentAscii_.length() < settings_.maxFrameLength && isPrintable((int)b)) {
        currentAscii_ += (char)b;
    }
    diag_.lastByteAt = millis();
    diag_.currentBytesCount = (uint16_t)rawLen_;
    diag_.frameOpen = true;
    diag_.runtimeStatus = "RX_BYTES";
}

void QrCamGm805::finalizeFrame(const char* reason) {
    if (rawLen_ == 0 && currentAscii_.isEmpty()) return;

    String ascii = trimCopy(currentAscii_);
    const String hex = bytesToHex(rawFrame_, rawLen_);

    diag_.lastRawAscii = ascii;
    diag_.lastRawHex = hex;
    diag_.lastFinalizeReason = reason;
    diag_.lastFrameAt = millis();
    diag_.lastFrameBytesCount = (uint16_t)rawLen_;

    if (!ascii.isEmpty()) {
        diag_.lastDecoded = ascii;
        diag_.runtimeStatus = "FRAME_PUBLISHED";
        if (onDecoded_) onDecoded_(ascii);
        logger_.info("QR decoded: " + ascii + " [" + String(reason) + "]");
    } else if (settings_.publishHexOnlyFrames && !hex.isEmpty()) {
        diag_.lastDecoded = hex;
        diag_.runtimeStatus = "HEX_ONLY";
        if (onDecoded_) onDecoded_(hex);
        logger_.warn("QR hex-only frame published");
    } else {
        diag_.runtimeStatus = "EMPTY_FRAME";
    }

    resetCurrentFrame();
}

void QrCamGm805::loop() {
    if (!settings_.enabled) {
        diag_.runtimeStatus = "OFF";
        return;
    }

    while (serial_.available()) {
        const int value = serial_.read();
        if (value < 0) break;
        const uint8_t b = (uint8_t)value;
        if ((settings_.acceptCr && b == '\r') || (settings_.acceptLf && b == '\n')) {
            finalizeFrame(b == '\r' ? "CR" : "LF");
            continue;
        }
        pushByte(b);
    }

    if (diag_.frameOpen && diag_.lastByteAt > 0) {
        const unsigned long idleFor = millis() - diag_.lastByteAt;
        if (idleFor >= settings_.frameIdleTimeoutMs) {
            finalizeFrame("IDLE_TIMEOUT");
        }
    }
}
