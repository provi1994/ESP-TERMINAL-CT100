#pragma once
#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "LogManager.h"

class QrTcpBridge {
public:
    struct Stats {
        bool enabled = false;
        bool clientConnected = false;
        uint16_t port = 4010;
        uint32_t rxBytesFromQr = 0;
        uint32_t txBytesToQr = 0;
        uint32_t rxBytesFromTcp = 0;
        uint32_t txBytesToTcp = 0;
        String lastRxHex = "";
        String lastTxHex = "";
    };

    explicit QrTcpBridge(LogManager& logger);
    void begin(uint16_t port);
    void stop();
    void loop(HardwareSerial& serial);

    bool isEnabled() const { return enabled_; }
    bool hasClient() { return client_.connected(); }
    Stats stats() const { return stats_; }

private:
    static String toHexByte(uint8_t b);
    void pushHexSample(String& dst, uint8_t b);

    LogManager& logger_;
    bool enabled_ = false;
    WiFiServer* server_ = nullptr;
    WiFiClient client_;
    Stats stats_;
};
