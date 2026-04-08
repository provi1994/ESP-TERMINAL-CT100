#include "DiscoveryService.h"

void DiscoveryService::begin(const DiscoveryInfo& info, uint16_t port) {
    info_ = info;
    port_ = port;
    udp_.begin(port_);
}

String DiscoveryService::buildResponseJson() const {
    const IPAddress ip = ETH.localIP();
    const String mac = ETH.macAddress();

    String resp;
    resp.reserve(384);
    resp += "{";
    resp += "\"proto\":\"ct100-discovery\",";
    resp += "\"ver\":1,";
    resp += "\"deviceId\":\"" + info_.deviceId + "\",";
    resp += "\"deviceName\":\"" + info_.deviceName + "\",";
    resp += "\"ip\":\"" + ip.toString() + "\",";
    resp += "\"mac\":\"" + mac + "\",";
    resp += "\"fw\":\"" + info_.fwVersion + "\",";
    resp += "\"model\":\"WT32-ETH01\",";
    resp += "\"tcpPort\":" + String(info_.tcpPort) + ",";
    resp += "\"httpPort\":" + String(info_.httpPort) + ",";
    resp += "\"config\":" + String(info_.configApiEnabled ? "true" : "false") + ",";
    resp += "\"rfid\":" + String(info_.rfidEnabled ? "true" : "false") + ",";
    resp += "\"keypad\":" + String(info_.keypadEnabled ? "true" : "false");
    resp += "}";
    return resp;
}

void DiscoveryService::loop() {
    const int packetSize = udp_.parsePacket();
    if (packetSize <= 0) {
        return;
    }

    String req;
    req.reserve(packetSize + 8);
    while (udp_.available()) {
        req += static_cast<char>(udp_.read());
    }

    if (req.indexOf("\"cmd\":\"discover\"") < 0) {
        return;
    }
    if (req.indexOf("\"proto\":\"ct100-discovery\"") < 0) {
        return;
    }

    const String resp = buildResponseJson();
    udp_.beginPacket(udp_.remoteIP(), udp_.remotePort());
    udp_.print(resp);
    udp_.endPacket();
}
