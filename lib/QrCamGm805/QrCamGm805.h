#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <functional>

#include "AppTypes.h"
#include "LogManager.h"

class QrCamGm805 {
public:
    struct Diagnostics {
        String lastDecoded;
        String lastRawAscii;
        String lastRawHex;
        String lastCommandHex;
        String lastCommandStatus;
        String lastFinalizeReason;
        String runtimeStatus;
        uint32_t lastByteAt = 0;
        uint32_t lastFrameAt = 0;
        uint16_t currentBytesCount = 0;
        uint16_t lastFrameBytesCount = 0;
        bool frameOpen = false;
    };

    explicit QrCamGm805(LogManager& logger, HardwareSerial& serial = Serial1);

    void begin(const QrSettings& settings, int rxPin, int txPin);
    void applySettings(const QrSettings& settings);
    void loop();

    bool sendHexCommand(const String& hex, const char* tag);
    void applyStartupCommands();

    void onDecoded(std::function<void(const String&)> cb);
    Diagnostics diagnostics() const;

private:
    static String trimCopy(const String& value);
    static String cleanHexLine(const String& line);
    static String bytesToHex(const uint8_t* data, size_t len);
    void pushByte(uint8_t b);
    void finalizeFrame(const char* reason);
    void resetCurrentFrame();

    LogManager& logger_;
    HardwareSerial& serial_;
    QrSettings settings_;
    int rxPin_ = -1;
    int txPin_ = -1;

    String currentAscii_;
    uint8_t rawFrame_[512] = {0};
    size_t rawLen_ = 0;
    std::function<void(const String&)> onDecoded_;
    Diagnostics diag_;
};
