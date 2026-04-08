#pragma once
#include <Arduino.h>
#include <ETH.h>
#include <WiFiUdp.h>

struct DiscoveryInfo {
    String deviceId;
    String deviceName;
    String fwVersion;
    uint16_t tcpPort = 7000;
    uint16_t httpPort = 80;
    bool configApiEnabled = true;
    bool rfidEnabled = true;
    bool keypadEnabled = true;
};

class DiscoveryService {
public:
    void begin(const DiscoveryInfo& info, uint16_t port = 40404);
    void loop();

private:
    String buildResponseJson() const;

    WiFiUDP udp_;
    DiscoveryInfo info_;
    uint16_t port_ = 40404;
};
