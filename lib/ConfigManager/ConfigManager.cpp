#include "ConfigManager.h"

bool ConfigManager::begin() { return prefs_.begin("ct100", false); }

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
    config_.tcp.autoReconnect = prefs_.getBool("tcp_arec", config_.tcp.autoReconnect);
    config_.tcp.reconnectIntervalMs = prefs_.getUShort("tcp_rint", config_.tcp.reconnectIntervalMs);
    config_.tcp.connectTimeoutMs = prefs_.getUShort("tcp_ctmo", config_.tcp.connectTimeoutMs);

    config_.scaleTcp.enabled = prefs_.getBool("sc_en", config_.scaleTcp.enabled);
    config_.scaleTcp.mode = static_cast<TcpMode>(prefs_.getUChar("sc_mode", static_cast<uint8_t>(config_.scaleTcp.mode)));
    config_.scaleTcp.serverIp = prefs_.getString("sc_ip", config_.scaleTcp.serverIp);
    config_.scaleTcp.serverPort = prefs_.getUShort("sc_sport", config_.scaleTcp.serverPort);
    config_.scaleTcp.listenPort = prefs_.getUShort("sc_lport", config_.scaleTcp.listenPort);
    config_.scaleTcp.autoReconnect = prefs_.getBool("sc_arec", config_.scaleTcp.autoReconnect);
    config_.scaleTcp.reconnectIntervalMs = prefs_.getUShort("sc_rint", config_.scaleTcp.reconnectIntervalMs);
    config_.scaleTcp.connectTimeoutMs = prefs_.getUShort("sc_ctmo", config_.scaleTcp.connectTimeoutMs);

    config_.security.adminUser = prefs_.getString("adm_user", prefs_.getString("web_user", config_.security.adminUser));
    config_.security.adminPassword = prefs_.getString("adm_pass", prefs_.getString("web_pass", config_.security.adminPassword));
    config_.security.serviceUser = prefs_.getString("svc_user", config_.security.serviceUser);
    config_.security.servicePassword = prefs_.getString("svc_pass", config_.security.servicePassword);
    config_.security.otaPassword = prefs_.getString("ota_pass", config_.security.otaPassword);

    config_.rfid.encoding = static_cast<RfidEncoding>(prefs_.getUChar("rfid_enc", static_cast<uint8_t>(config_.rfid.encoding)));
    config_.rfid.baudRate = prefs_.getULong("rfid_baud", config_.rfid.baudRate);
    config_.rfid.enabled = prefs_.getBool("rfid_en", config_.rfid.enabled);

    config_.qr.enabled = prefs_.getBool("qr_en", config_.qr.enabled);
    config_.qr.baudRate = prefs_.getULong("qr_baud", config_.qr.baudRate);
    config_.qr.sendToTcp = prefs_.getBool("qr_tcp", config_.qr.sendToTcp);
    config_.qr.publishToWeb = prefs_.getBool("qr_web", config_.qr.publishToWeb);
    config_.qr.applyStartupCommands = prefs_.getBool("qr_apply", config_.qr.applyStartupCommands);
    config_.qr.saveToFlashAfterApply = prefs_.getBool("qr_save", config_.qr.saveToFlashAfterApply);
    config_.qr.startupCommandDelayMs = prefs_.getUShort("qr_sdelay", config_.qr.startupCommandDelayMs);
    config_.qr.interCommandDelayMs = prefs_.getUShort("qr_idelay", config_.qr.interCommandDelayMs);
    config_.qr.maxFrameLength = prefs_.getUShort("qr_maxlen", config_.qr.maxFrameLength);
    config_.qr.frameIdleTimeoutMs = prefs_.getUShort("qr_idle", config_.qr.frameIdleTimeoutMs);
    config_.qr.linePrefix = prefs_.getString("qr_prefix", config_.qr.linePrefix);
    config_.qr.startupCommandsHex = prefs_.getString("qr_starthex", config_.qr.startupCommandsHex);
    config_.qr.tcpBridgeEnabled = prefs_.getBool("qr_br_en", config_.qr.tcpBridgeEnabled);
    config_.qr.tcpBridgePort = prefs_.getUShort("qr_br_pt", config_.qr.tcpBridgePort);

    config_.display.enabled = prefs_.getBool("disp_en", config_.display.enabled);
    config_.display.contrast = prefs_.getUChar("disp_ctr", config_.display.contrast);

    config_.display.flow.enabled = prefs_.getBool("flow_en", config_.display.flow.enabled);
    config_.display.flow.remoteTriggerEnabled = prefs_.getBool("flow_rmt", config_.display.flow.remoteTriggerEnabled);
    config_.display.flow.weightTriggerEnabled = prefs_.getBool("flow_wgh", config_.display.flow.weightTriggerEnabled);
    config_.display.flow.weightThresholdKg = prefs_.getUShort("flow_thr", config_.display.flow.weightThresholdKg);
    config_.display.flow.summaryScreenMs = prefs_.getUShort("flow_sum", config_.display.flow.summaryScreenMs);
    config_.display.flow.resultScreenMs = prefs_.getUShort("flow_res", config_.display.flow.resultScreenMs);

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
    prefs_.putBool("tcp_arec", config.tcp.autoReconnect);
    prefs_.putUShort("tcp_rint", config.tcp.reconnectIntervalMs);
    prefs_.putUShort("tcp_ctmo", config.tcp.connectTimeoutMs);

    prefs_.putBool("sc_en", config.scaleTcp.enabled);
    prefs_.putUChar("sc_mode", static_cast<uint8_t>(config.scaleTcp.mode));
    prefs_.putString("sc_ip", config.scaleTcp.serverIp);
    prefs_.putUShort("sc_sport", config.scaleTcp.serverPort);
    prefs_.putUShort("sc_lport", config.scaleTcp.listenPort);
    prefs_.putBool("sc_arec", config.scaleTcp.autoReconnect);
    prefs_.putUShort("sc_rint", config.scaleTcp.reconnectIntervalMs);
    prefs_.putUShort("sc_ctmo", config.scaleTcp.connectTimeoutMs);

    prefs_.putString("adm_user", config.security.adminUser);
    prefs_.putString("adm_pass", config.security.adminPassword);
    prefs_.putString("svc_user", config.security.serviceUser);
    prefs_.putString("svc_pass", config.security.servicePassword);
    prefs_.putString("ota_pass", config.security.otaPassword);
    prefs_.putString("web_user", config.security.adminUser);
    prefs_.putString("web_pass", config.security.adminPassword);

    prefs_.putUChar("rfid_enc", static_cast<uint8_t>(config.rfid.encoding));
    prefs_.putULong("rfid_baud", config.rfid.baudRate);
    prefs_.putBool("rfid_en", config.rfid.enabled);

    prefs_.putBool("qr_en", config.qr.enabled);
    prefs_.putULong("qr_baud", config.qr.baudRate);
    prefs_.putBool("qr_tcp", config.qr.sendToTcp);
    prefs_.putBool("qr_web", config.qr.publishToWeb);
    prefs_.putBool("qr_apply", config.qr.applyStartupCommands);
    prefs_.putBool("qr_save", config.qr.saveToFlashAfterApply);
    prefs_.putUShort("qr_sdelay", config.qr.startupCommandDelayMs);
    prefs_.putUShort("qr_idelay", config.qr.interCommandDelayMs);
    prefs_.putUShort("qr_maxlen", config.qr.maxFrameLength);
    prefs_.putUShort("qr_idle", config.qr.frameIdleTimeoutMs);
    prefs_.putString("qr_prefix", config.qr.linePrefix);
    prefs_.putString("qr_starthex", config.qr.startupCommandsHex);
    prefs_.putBool("qr_br_en", config.qr.tcpBridgeEnabled);
    prefs_.putUShort("qr_br_pt", config.qr.tcpBridgePort);

    prefs_.putBool("disp_en", config.display.enabled);
    prefs_.putUChar("disp_ctr", config.display.contrast);

    prefs_.putBool("flow_en", config.display.flow.enabled);
    prefs_.putBool("flow_rmt", config.display.flow.remoteTriggerEnabled);
    prefs_.putBool("flow_wgh", config.display.flow.weightTriggerEnabled);
    prefs_.putUShort("flow_thr", config.display.flow.weightThresholdKg);
    prefs_.putUShort("flow_sum", config.display.flow.summaryScreenMs);
    prefs_.putUShort("flow_res", config.display.flow.resultScreenMs);

    prefs_.putBool("key_en", config.keypad.enabled);
    prefs_.putUChar("key_addr", config.keypad.pcf8574Address);

    prefs_.putBool("disc_en", config.discovery.enabled);
    prefs_.putUShort("disc_port", config.discovery.udpPort);
    return true;
}

String ConfigManager::ipToString(const IPAddress& ip) { return ip.toString(); }
IPAddress ConfigManager::stringToIp(const String& ipStr) { IPAddress ip; if (!ip.fromString(ipStr)) return IPAddress(0, 0, 0, 0); return ip; }
String ConfigManager::networkModeToString(NetworkMode mode) { return mode == NetworkMode::STATIC ? "static" : "dhcp"; }
NetworkMode ConfigManager::networkModeFromString(const String& value) { return value == "static" ? NetworkMode::STATIC : NetworkMode::DHCP; }
String ConfigManager::tcpModeToString(TcpMode mode) { switch (mode) { case TcpMode::HOST: return "host"; case TcpMode::SERVER: return "server"; default: return "client"; } }
TcpMode ConfigManager::tcpModeFromString(const String& value) { if (value == "host") return TcpMode::HOST; if (value == "server") return TcpMode::SERVER; return TcpMode::CLIENT; }
String ConfigManager::rfidEncodingToString(RfidEncoding encoding) { switch (encoding) { case RfidEncoding::DEC_MODE: return "dec"; case RfidEncoding::RAW_MODE: return "raw"; case RfidEncoding::SCALE_FRAME_MODE: return "scale_frame"; default: return "hex"; } }
RfidEncoding ConfigManager::rfidEncodingFromString(const String& value) { if (value == "dec") return RfidEncoding::DEC_MODE; if (value == "raw") return RfidEncoding::RAW_MODE; if (value == "scale_frame") return RfidEncoding::SCALE_FRAME_MODE; return RfidEncoding::HEX_MODE; }
