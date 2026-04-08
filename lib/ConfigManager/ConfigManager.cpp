#include "ConfigManager.h"

bool ConfigManager::begin() {
    return prefs_.begin("ct100", false);
}

DeviceConfig ConfigManager::load() {
    config_.network.deviceName = prefs_.getString("dev_name", config_.network.deviceName);
    config_.network.mode = static_cast<NetworkMode>(prefs_.getUChar("net_mode", static_cast<uint8_t>(config_.network.mode)));
    config_.network.ip = stringToIp(prefs_.getString("ip", ipToString(config_.network.ip)));
    config_.network.gateway = stringToIp(prefs_.getString("gw", ipToString(config_.network.gateway)));
    config_.network.subnet = stringToIp(prefs_.getString("mask", ipToString(config_.network.subnet)));
    config_.network.dns1 = stringToIp(prefs_.getString("dns1", ipToString(config_.network.dns1)));
    config_.network.dns2 = stringToIp(prefs_.getString("dns2", ipToString(config_.network.dns2)));

    config_.tcp.mode = static_cast<TcpMode>(prefs_.getUChar("tcp_mode", static_cast<uint8_t>(config_.tcp.mode)));
    config_.tcp.serverIp = prefs_.getString("tcp_ip", config_.tcp.serverIp);
    config_.tcp.serverPort = prefs_.getUShort("tcp_sport", config_.tcp.serverPort);
    config_.tcp.listenPort = prefs_.getUShort("tcp_lport", config_.tcp.listenPort);

    config_.scaleTcp.enabled = prefs_.getBool("sc_en", config_.scaleTcp.enabled);
    config_.scaleTcp.mode = static_cast<TcpMode>(prefs_.getUChar("sc_mode", static_cast<uint8_t>(config_.scaleTcp.mode)));
    config_.scaleTcp.serverIp = prefs_.getString("sc_ip", config_.scaleTcp.serverIp);
    config_.scaleTcp.serverPort = prefs_.getUShort("sc_sport", config_.scaleTcp.serverPort);
    config_.scaleTcp.listenPort = prefs_.getUShort("sc_lport", config_.scaleTcp.listenPort);

    config_.security.webUser = prefs_.getString("web_user", config_.security.webUser);
    config_.security.webPassword = prefs_.getString("web_pass", config_.security.webPassword);
    config_.security.otaPassword = prefs_.getString("ota_pass", config_.security.otaPassword);

    config_.rfid.encoding = static_cast<RfidEncoding>(prefs_.getUChar("rfid_enc", static_cast<uint8_t>(config_.rfid.encoding)));
    config_.rfid.baudRate = prefs_.getULong("rfid_baud", config_.rfid.baudRate);
    config_.rfid.enabled = prefs_.getBool("rfid_en", config_.rfid.enabled);

    config_.display.enabled = prefs_.getBool("disp_en", config_.display.enabled);
    config_.display.contrast = prefs_.getUChar("disp_ctr", config_.display.contrast);

    config_.keypad.enabled = prefs_.getBool("key_en", config_.keypad.enabled);
    config_.keypad.pcf8574Address = prefs_.getUChar("key_addr", config_.keypad.pcf8574Address);

    config_.discovery.enabled = prefs_.getBool("disc_en", config_.discovery.enabled);
    config_.discovery.udpPort = prefs_.getUShort("disc_port", config_.discovery.udpPort);

    return config_;
}

bool ConfigManager::save(const DeviceConfig& config) {
    config_ = config;

    prefs_.putString("dev_name", config.network.deviceName);
    prefs_.putUChar("net_mode", static_cast<uint8_t>(config.network.mode));
    prefs_.putString("ip", ipToString(config.network.ip));
    prefs_.putString("gw", ipToString(config.network.gateway));
    prefs_.putString("mask", ipToString(config.network.subnet));
    prefs_.putString("dns1", ipToString(config.network.dns1));
    prefs_.putString("dns2", ipToString(config.network.dns2));

    prefs_.putUChar("tcp_mode", static_cast<uint8_t>(config.tcp.mode));
    prefs_.putString("tcp_ip", config.tcp.serverIp);
    prefs_.putUShort("tcp_sport", config.tcp.serverPort);
    prefs_.putUShort("tcp_lport", config.tcp.listenPort);

    prefs_.putBool("sc_en", config.scaleTcp.enabled);
    prefs_.putUChar("sc_mode", static_cast<uint8_t>(config.scaleTcp.mode));
    prefs_.putString("sc_ip", config.scaleTcp.serverIp);
    prefs_.putUShort("sc_sport", config.scaleTcp.serverPort);
    prefs_.putUShort("sc_lport", config.scaleTcp.listenPort);

    prefs_.putString("web_user", config.security.webUser);
    prefs_.putString("web_pass", config.security.webPassword);
    prefs_.putString("ota_pass", config.security.otaPassword);

    prefs_.putUChar("rfid_enc", static_cast<uint8_t>(config.rfid.encoding));
    prefs_.putULong("rfid_baud", config.rfid.baudRate);
    prefs_.putBool("rfid_en", config.rfid.enabled);

    prefs_.putBool("disp_en", config.display.enabled);
    prefs_.putUChar("disp_ctr", config.display.contrast);

    prefs_.putBool("key_en", config.keypad.enabled);
    prefs_.putUChar("key_addr", config.keypad.pcf8574Address);

    prefs_.putBool("disc_en", config.discovery.enabled);
    prefs_.putUShort("disc_port", config.discovery.udpPort);

    return true;
}

String ConfigManager::ipToString(const IPAddress& ip) {
    return ip.toString();
}

IPAddress ConfigManager::stringToIp(const String& ipStr) {
    IPAddress ip;
    if (!ip.fromString(ipStr)) {
        return IPAddress(0, 0, 0, 0);
    }
    return ip;
}

String ConfigManager::networkModeToString(NetworkMode mode) {
    return mode == NetworkMode::STATIC ? "static" : "dhcp";
}

NetworkMode ConfigManager::networkModeFromString(const String& value) {
    return value == "static" ? NetworkMode::STATIC : NetworkMode::DHCP;
}

String ConfigManager::tcpModeToString(TcpMode mode) {
    switch (mode) {
        case TcpMode::HOST: return "host";
        case TcpMode::SERVER: return "server";
        default: return "client";
    }
}

TcpMode ConfigManager::tcpModeFromString(const String& value) {
    if (value == "host") return TcpMode::HOST;
    if (value == "server") return TcpMode::SERVER;
    return TcpMode::CLIENT;
}

String ConfigManager::rfidEncodingToString(RfidEncoding encoding) {
    switch (encoding) {
        case RfidEncoding::DEC_MODE: return "dec";
        case RfidEncoding::RAW_MODE: return "raw";
        default: return "hex";
    }
}

RfidEncoding ConfigManager::rfidEncodingFromString(const String& value) {
    if (value == "dec") return RfidEncoding::DEC_MODE;
    if (value == "raw") return RfidEncoding::RAW_MODE;
    return RfidEncoding::HEX_MODE;
}