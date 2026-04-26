
#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>

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
        String runtimeStatus = "IDLE";
        uint32_t lastByteAt = 0;
        uint32_t lastFrameAt = 0;
        uint16_t currentBytesCount = 0;
        uint16_t lastFrameBytesCount = 0;
        bool frameOpen = false;
    };

    explicit QrCamGm805(LogManager& logger);

    void begin(const QrSettings& settings, int rxPin, int txPin);
    void applySettings(const QrSettings& settings);
    void loop();

    bool sendHexCommand(const String& inputHex, const char* tag);
    void applyStartupCommands();

    Diagnostics diagnostics() const;
    bool hasFreshDecode() const;
    String takeLastDecode();

    void onDecoded(std::function<void(const String& value)> cb);

private:
    void clearCurrentFrame();
    void finalizeFrame(const char* reason);
    bool isKnownControlFrame(const std::vector<uint8_t>& raw) const;
    String extractAsciiPayload(const std::vector<uint8_t>& raw) const;
    static String bytesToHex(const uint8_t* data, size_t len);

    LogManager& logger_;
    QrSettings settings_;
    int rxPin_ = -1;
    int txPin_ = -1;

    std::vector<uint8_t> currentRaw_;
    String currentAscii_;
    String pendingDecoded_;
    uint32_t lastByteAt_ = 0;
    Diagnostics diag_;
    std::function<void(const String&)> onDecoded_;
};
