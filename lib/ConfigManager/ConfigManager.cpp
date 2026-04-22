#include "ConfigManager.h"

bool ConfigManager::begin() { return prefs_.begin("ct100", false); }

DeviceConfig ConfigManager::load() {
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
    config_.qr.acceptCr = prefs_.getBool("qr_cr", config_.qr.acceptCr);
    config_.qr.acceptLf = prefs_.getBool("qr_lf", config_.qr.acceptLf);
    config_.qr.publishHexOnlyFrames = prefs_.getBool("qr_hexonly", config_.qr.publishHexOnlyFrames);
    config_.qr.linePrefix = prefs_.getString("qr_prefix", config_.qr.linePrefix);
    config_.qr.startupCommandsHex = prefs_.getString("qr_starthex", config_.qr.startupCommandsHex);
    return config_;
}

bool ConfigManager::save(const DeviceConfig& config) {
    config_ = config;
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
    prefs_.putBool("qr_cr", config.qr.acceptCr);
    prefs_.putBool("qr_lf", config.qr.acceptLf);
    prefs_.putBool("qr_hexonly", config.qr.publishHexOnlyFrames);
    prefs_.putString("qr_prefix", config.qr.linePrefix);
    prefs_.putString("qr_starthex", config.qr.startupCommandsHex);
    return true;
}

String ConfigManager::ipToString(const IPAddress& ip) { return ip.toString(); }
IPAddress ConfigManager::stringToIp(const String& ipStr) { IPAddress ip; if (!ip.fromString(ipStr)) return IPAddress(0,0,0,0); return ip; }
String ConfigManager::networkModeToString(NetworkMode mode) { return mode == NetworkMode::STATIC ? "static" : "dhcp"; }
NetworkMode ConfigManager::networkModeFromString(const String& value) { return value == "static" ? NetworkMode::STATIC : NetworkMode::DHCP; }
String ConfigManager::tcpModeToString(TcpMode mode) { switch (mode) { case TcpMode::HOST: return "host"; case TcpMode::SERVER: return "server"; default: return "client"; } }
TcpMode ConfigManager::tcpModeFromString(const String& value) { if (value == "host") return TcpMode::HOST; if (value == "server") return TcpMode::SERVER; return TcpMode::CLIENT; }
String ConfigManager::rfidEncodingToString(RfidEncoding encoding) { switch (encoding) { case RfidEncoding::DEC_MODE: return "dec"; case RfidEncoding::RAW_MODE: return "raw"; case RfidEncoding::SCALE_FRAME_MODE: return "scale_frame"; default: return "hex"; } }
RfidEncoding ConfigManager::rfidEncodingFromString(const String& value) { if (value == "dec") return RfidEncoding::DEC_MODE; if (value == "raw") return RfidEncoding::RAW_MODE; if (value == "scale_frame") return RfidEncoding::SCALE_FRAME_MODE; return RfidEncoding::HEX_MODE; }
