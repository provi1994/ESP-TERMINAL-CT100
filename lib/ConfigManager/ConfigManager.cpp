#include "ConfigManager.h"

/*
 * Jeden namespace Preferences dla całego terminala.
 * Dzięki temu po aktualizacji firmware zachowujemy ustawienia urządzenia.
 */
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

    /*
     * Zachowujemy kompatybilność wsteczną:
     * stare klucze web_user/web_pass nadal są czytane jako fallback.
     */
    config_.security.adminUser = prefs_.getString("adm_user", prefs_.getString("web_user", config_.security.adminUser));
    config_.security.adminPassword = prefs_.getString("adm_pass", prefs_.getString("web_pass", config_.security.adminPassword));
    config_.security.serviceUser = prefs_.getString("svc_user", config_.security.serviceUser);
    config_.security.servicePassword = prefs_.getString("svc_pass", config_.security.servicePassword);
    config_.security.otaPassword = prefs_.getString("ota_pass", config_.security.otaPassword);

    config_.rfid.encoding = static_cast<RfidEncoding>(prefs_.getUChar("rfid_enc", static_cast<uint8_t>(config_.rfid.encoding)));
    config_.rfid.baudRate = prefs_.getULong("rfid_baud", config_.rfid.baudRate);
    config_.rfid.enabled = prefs_.getBool("rfid_en", config_.rfid.enabled);

    config_.display.enabled = prefs_.getBool("disp_en", config_.display.enabled);
    config_.display.contrast = prefs_.getUChar("disp_ctr", config_.display.contrast);

    config_.display.flow.enabled = prefs_.getBool("flow_en", config_.display.flow.enabled);
    config_.display.flow.remoteTriggerEnabled = prefs_.getBool("flow_rmt", config_.display.flow.remoteTriggerEnabled);
    config_.display.flow.weightTriggerEnabled = prefs_.getBool("flow_wgh", config_.display.flow.weightTriggerEnabled);
    config_.display.flow.weightThresholdKg = prefs_.getUShort("flow_thr", config_.display.flow.weightThresholdKg);
    config_.display.flow.summaryScreenMs = prefs_.getUShort("flow_sum", config_.display.flow.summaryScreenMs);
    config_.display.flow.resultScreenMs = prefs_.getUShort("flow_res", config_.display.flow.resultScreenMs);

    config_.display.screen1.enabled = prefs_.getBool("sc1_en", config_.display.screen1.enabled);
    config_.display.screen1.order = prefs_.getUChar("sc1_ord", config_.display.screen1.order);
    config_.display.screen1.name = prefs_.getString("sc1_nm", config_.display.screen1.name);
    config_.display.screen1.title = prefs_.getString("sc1_ti", config_.display.screen1.title);
    config_.display.screen1.line1 = prefs_.getString("sc1_l1", config_.display.screen1.line1);
    config_.display.screen1.line2 = prefs_.getString("sc1_l2", config_.display.screen1.line2);
    config_.display.screen1.hint = prefs_.getString("sc1_hi", config_.display.screen1.hint);

    config_.display.screen2.enabled = prefs_.getBool("sc2_en", config_.display.screen2.enabled);
    config_.display.screen2.order = prefs_.getUChar("sc2_ord", config_.display.screen2.order);
    config_.display.screen2.name = prefs_.getString("sc2_nm", config_.display.screen2.name);
    config_.display.screen2.title = prefs_.getString("sc2_ti", config_.display.screen2.title);
    config_.display.screen2.line1 = prefs_.getString("sc2_l1", config_.display.screen2.line1);
    config_.display.screen2.line2 = prefs_.getString("sc2_l2", config_.display.screen2.line2);
    config_.display.screen2.hint = prefs_.getString("sc2_hi", config_.display.screen2.hint);

    config_.display.screen3.enabled = prefs_.getBool("sc3_en", config_.display.screen3.enabled);
    config_.display.screen3.order = prefs_.getUChar("sc3_ord", config_.display.screen3.order);
    config_.display.screen3.name = prefs_.getString("sc3_nm", config_.display.screen3.name);
    config_.display.screen3.title = prefs_.getString("sc3_ti", config_.display.screen3.title);
    config_.display.screen3.line1 = prefs_.getString("sc3_l1", config_.display.screen3.line1);
    config_.display.screen3.line2 = prefs_.getString("sc3_l2", config_.display.screen3.line2);
    config_.display.screen3.hint = prefs_.getString("sc3_hi", config_.display.screen3.hint);

    config_.display.screen4.enabled = prefs_.getBool("sc4_en", config_.display.screen4.enabled);
    config_.display.screen4.order = prefs_.getUChar("sc4_ord", config_.display.screen4.order);
    config_.display.screen4.name = prefs_.getString("sc4_nm", config_.display.screen4.name);
    config_.display.screen4.title = prefs_.getString("sc4_ti", config_.display.screen4.title);
    config_.display.screen4.line1 = prefs_.getString("sc4_l1", config_.display.screen4.line1);
    config_.display.screen4.line2 = prefs_.getString("sc4_l2", config_.display.screen4.line2);
    config_.display.screen4.hint = prefs_.getString("sc4_hi", config_.display.screen4.hint);

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

    prefs_.putBool("disp_en", config.display.enabled);
    prefs_.putUChar("disp_ctr", config.display.contrast);

    prefs_.putBool("flow_en", config.display.flow.enabled);
    prefs_.putBool("flow_rmt", config.display.flow.remoteTriggerEnabled);
    prefs_.putBool("flow_wgh", config.display.flow.weightTriggerEnabled);
    prefs_.putUShort("flow_thr", config.display.flow.weightThresholdKg);
    prefs_.putUShort("flow_sum", config.display.flow.summaryScreenMs);
    prefs_.putUShort("flow_res", config.display.flow.resultScreenMs);

    prefs_.putBool("sc1_en", config.display.screen1.enabled);
    prefs_.putUChar("sc1_ord", config.display.screen1.order);
    prefs_.putString("sc1_nm", config.display.screen1.name);
    prefs_.putString("sc1_ti", config.display.screen1.title);
    prefs_.putString("sc1_l1", config.display.screen1.line1);
    prefs_.putString("sc1_l2", config.display.screen1.line2);
    prefs_.putString("sc1_hi", config.display.screen1.hint);

    prefs_.putBool("sc2_en", config.display.screen2.enabled);
    prefs_.putUChar("sc2_ord", config.display.screen2.order);
    prefs_.putString("sc2_nm", config.display.screen2.name);
    prefs_.putString("sc2_ti", config.display.screen2.title);
    prefs_.putString("sc2_l1", config.display.screen2.line1);
    prefs_.putString("sc2_l2", config.display.screen2.line2);
    prefs_.putString("sc2_hi", config.display.screen2.hint);

    prefs_.putBool("sc3_en", config.display.screen3.enabled);
    prefs_.putUChar("sc3_ord", config.display.screen3.order);
    prefs_.putString("sc3_nm", config.display.screen3.name);
    prefs_.putString("sc3_ti", config.display.screen3.title);
    prefs_.putString("sc3_l1", config.display.screen3.line1);
    prefs_.putString("sc3_l2", config.display.screen3.line2);
    prefs_.putString("sc3_hi", config.display.screen3.hint);

    prefs_.putBool("sc4_en", config.display.screen4.enabled);
    prefs_.putUChar("sc4_ord", config.display.screen4.order);
    prefs_.putString("sc4_nm", config.display.screen4.name);
    prefs_.putString("sc4_ti", config.display.screen4.title);
    prefs_.putString("sc4_l1", config.display.screen4.line1);
    prefs_.putString("sc4_l2", config.display.screen4.line2);
    prefs_.putString("sc4_hi", config.display.screen4.hint);

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
        case RfidEncoding::SCALE_FRAME_MODE: return "scale_frame";
        default: return "hex";
    }
}

RfidEncoding ConfigManager::rfidEncodingFromString(const String& value) {
    if (value == "dec") return RfidEncoding::DEC_MODE;
    if (value == "raw") return RfidEncoding::RAW_MODE;
    if (value == "scale_frame") return RfidEncoding::SCALE_FRAME_MODE;
    return RfidEncoding::HEX_MODE;
}
