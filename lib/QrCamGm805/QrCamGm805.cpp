
#include "QrCamGm805.h"

#include <HardwareSerial.h>

namespace {
HardwareSerial& qrSerial() { return Serial1; }

static bool isPrintableAscii(uint8_t b) {
    return b >= 32 && b <= 126;
}

static String trimCopyLocal(const String& in) {
    String out = in;
    out.trim();
    return out;
}
}

QrCamGm805::QrCamGm805(LogManager& logger) : logger_(logger) {}

void QrCamGm805::begin(const QrSettings& settings, int rxPin, int txPin) {
    settings_ = settings;
    rxPin_ = rxPin;
    txPin_ = txPin;

    qrSerial().begin(settings_.baudRate, SERIAL_8N1, rxPin_, txPin_);
    clearCurrentFrame();
    diag_ = Diagnostics();
    diag_.runtimeStatus = settings_.enabled ? "READY" : "DISABLED";
}

void QrCamGm805::applySettings(const QrSettings& settings) {
    settings_ = settings;
    qrSerial().updateBaudRate(settings_.baudRate);
}

void QrCamGm805::onDecoded(std::function<void(const String& value)> cb) {
    onDecoded_ = cb;
}

void QrCamGm805::clearCurrentFrame() {
    currentRaw_.clear();
    currentAscii_.clear();
    diag_.currentBytesCount = 0;
    diag_.frameOpen = false;
}

String QrCamGm805::bytesToHex(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return "";
    static const char* HEX_DIGITS = "0123456789ABCDEF";
    String out;
    out.reserve(len * 3);
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) out += ' ';
        out += HEX_DIGITS[(data[i] >> 4) & 0x0F];
        out += HEX_DIGITS[data[i] & 0x0F];
    }
    return out;
}

bool QrCamGm805::sendHexCommand(const String& inputHex, const char* tag) {
    String hex;
    hex.reserve(inputHex.length());
    for (size_t i = 0; i < inputHex.length(); ++i) {
        char c = inputHex[i];
        if (isxdigit((unsigned char)c)) hex += (char)toupper((unsigned char)c);
    }

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
        qrSerial().write(value);
    }
    qrSerial().flush();
    diag_.lastCommandStatus = "OK";
    logger_.info(String("GM805 cmd ") + tag + ": " + hex);
    return true;
}

void QrCamGm805::applyStartupCommands() {
    if (!settings_.enabled || !settings_.applyStartupCommands) return;
    const String all = settings_.startupCommandsHex;
    if (all.isEmpty()) return;

    delay(settings_.startupCommandDelayMs);
    int start = 0;
    while (start <= (int)all.length()) {
        int end = all.indexOf('\n', start);
        if (end < 0) end = all.length();
        String line = all.substring(start, end);
        line.trim();
        int hashPos = line.indexOf('#');
        if (hashPos >= 0) line = line.substring(0, hashPos);
        line.trim();
        if (!line.isEmpty()) sendHexCommand(line, "startup");
        start = end + 1;
        delay(settings_.interCommandDelayMs);
    }
}

bool QrCamGm805::isKnownControlFrame(const std::vector<uint8_t>& raw) const {
    if (raw.empty()) return false;

    const String hex = bytesToHex(raw.data(), raw.size());

    // ACK / trigger-related frame seen from GM805
    if (hex == "02 00 00 01 00 33 31") return true;

    // find-baud / control response family seen from GM805
    if (raw.size() >= 8 &&
        raw[0] == 0x02 && raw[1] == 0x00 && raw[2] == 0x00 &&
        raw[3] == 0x02) return true;

    // Short binary control packets starting with STX and no meaningful ASCII payload
    size_t printable = 0;
    for (uint8_t b : raw) if (isPrintableAscii(b)) ++printable;
    if (raw[0] == 0x02 && printable <= 2) return true;

    return false;
}

String QrCamGm805::extractAsciiPayload(const std::vector<uint8_t>& raw) const {
    String out;
    out.reserve(raw.size());

    // Prefer contiguous ASCII runs of length >= 3
    String run;
    for (uint8_t b : raw) {
        if (isPrintableAscii(b)) {
            run += (char)b;
        } else {
            if (run.length() >= 3) out += run;
            run = "";
        }
    }
    if (run.length() >= 3) out += run;

    out.trim();
    return out;
}

void QrCamGm805::finalizeFrame(const char* reason) {
    diag_.lastFinalizeReason = reason ? String(reason) : "";
    diag_.lastByteAt = lastByteAt_;
    diag_.lastFrameAt = millis();
    diag_.lastFrameBytesCount = (uint16_t)currentRaw_.size();

    if (currentRaw_.empty()) {
        diag_.runtimeStatus = "EMPTY_FRAME";
        clearCurrentFrame();
        return;
    }

    diag_.lastRawHex = bytesToHex(currentRaw_.data(), currentRaw_.size());
    diag_.lastRawAscii = extractAsciiPayload(currentRaw_);

    if (isKnownControlFrame(currentRaw_)) {
        diag_.runtimeStatus = "CONTROL_FRAME";
        logger_.info("GM805 control frame ignored: " + diag_.lastRawHex);
        clearCurrentFrame();
        return;
    }

    String decoded = trimCopyLocal(diag_.lastRawAscii);
    if (decoded.isEmpty()) {
        diag_.runtimeStatus = "HEX_ONLY_FRAME";
        clearCurrentFrame();
        return;
    }

    diag_.lastDecoded = decoded;
    pendingDecoded_ = decoded;
    diag_.runtimeStatus = "FRAME_PUBLISHED";
    logger_.info("GM805 decoded: " + decoded);

    if (onDecoded_) onDecoded_(decoded);
    clearCurrentFrame();
}

void QrCamGm805::loop() {
    if (!settings_.enabled) return;

    while (qrSerial().available()) {
        int v = qrSerial().read();
        if (v < 0) break;
        uint8_t b = (uint8_t)v;

        lastByteAt_ = millis();
        diag_.lastByteAt = lastByteAt_;
        diag_.frameOpen = true;
        currentRaw_.push_back(b);
        diag_.currentBytesCount = (uint16_t)currentRaw_.size();

        if (isPrintableAscii(b)) currentAscii_ += (char)b;

        if (currentRaw_.size() >= settings_.maxFrameLength) {
            finalizeFrame("MAX_LEN");
            return;
        }

        if ((settings_.acceptCr && b == '\r') || (settings_.acceptLf && b == '\n')) {
            finalizeFrame("CRLF");
            return;
        }
    }

    if (!currentRaw_.empty()) {
        const uint32_t now = millis();
        if ((now - lastByteAt_) >= settings_.frameIdleTimeoutMs) {
            finalizeFrame("IDLE_TIMEOUT");
        }
    }
}

QrCamGm805::Diagnostics QrCamGm805::diagnostics() const {
    return diag_;
}

bool QrCamGm805::hasFreshDecode() const {
    return !pendingDecoded_.isEmpty();
}

String QrCamGm805::takeLastDecode() {
    String out = pendingDecoded_;
    pendingDecoded_.clear();
    return out;
}
