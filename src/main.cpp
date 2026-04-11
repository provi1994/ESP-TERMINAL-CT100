#include <ArduinoOTA.h>
#include <ETH.h>
#include <vector>

#include "AppTypes.h"
#include "ConfigManager.h"
#include "DiscoveryService.h"
#include "DisplayManager.h"
#include "KeypadManager.h"
#include "LogManager.h"
#include "NetManager.h"
#include "Pins.h"
#include "Rfid125kHzUart.h"
#include "RfidFrameEncoder.h"
#include "TcpManager.h"
#include "WebConfigServer.h"

LogManager logger(120);
ConfigManager configManager;
NetManager netManager(logger);
DisplayManager display(logger, Pins::LCD_CLK, Pins::LCD_MOSI, Pins::LCD_CS, Pins::LCD_RST);
Rfid125kHzUart rfid(logger);
KeypadManager keypad(logger);
TcpManager tcpManager(logger);
TcpManager scaleTcpManager(logger);
WebConfigServer webServer(logger);
DiscoveryService discoveryService;

DeviceConfig cfg;
unsigned long lastStatusRefresh = 0;
String lastCard;
String lastKey;
String lastOutboundFrame;
String lastInboundFrame;
String lcdCustomText;
unsigned long lcdCustomUntil = 0;
String remoteStatusText = "Oczekuje na wazenie";
unsigned long remoteStatusUntil = 0;
String currentWeight = "---";

enum class BootScreenPhase : uint8_t { LOGO = 0, MODULES = 1, TCP = 2, DONE = 3 };
unsigned long bootSequenceStart = 0;
BootScreenPhase bootPhase = BootScreenPhase::DONE;
bool keypadDetected = false;
String rfidStatus = "OFF";

static String jsonEscapeLocal(const String& value) {
    String out;
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

static String activeHeaderText() {
    if (remoteStatusUntil > millis() && !remoteStatusText.isEmpty()) return remoteStatusText;
    return "Oczekuje na wazenie";
}

static String buildStatusText() {
    String out;
    out += "device=" + cfg.network.deviceName + "\n";
    out += "ip=" + netManager.localIP().toString() + "\n";
    out += "rfid_last=" + lastCard + "\n";
    out += "key_last=" + lastKey + "\n";
    out += "last_outbound=" + lastOutboundFrame + "\n";
    out += "last_inbound=" + lastInboundFrame + "\n";
    out += "cmd_tcp_mode=" + ConfigManager::tcpModeToString(cfg.tcp.mode) + "\n";
    out += "cmd_tcp_last=" + tcpManager.lastMessage() + "\n";
    out += "scale_enabled=" + String(cfg.scaleTcp.enabled ? "on" : "off") + "\n";
    out += "scale_tcp_mode=" + ConfigManager::tcpModeToString(cfg.scaleTcp.mode) + "\n";
    return out;
}

static String tcpModeLabel(TcpMode mode) {
    switch (mode) {
        case TcpMode::CLIENT: return "CLIENT";
        case TcpMode::HOST: return "HOST";
        case TcpMode::SERVER: return "SERVER";
        default: return "?";
    }
}

static String ipText() {
    IPAddress ip = netManager.localIP();
    if (ip == IPAddress((uint32_t)0)) return "0.0.0.0";
    return ip.toString();
}

static void renderBootSequence() {
    if (!cfg.display.enabled) return;
    const unsigned long elapsed = millis() - bootSequenceStart;
    if (elapsed < 3000UL) {
        bootPhase = BootScreenPhase::LOGO;
        display.showLogo();
        return;
    }
    if (elapsed < 6500UL) {
        bootPhase = BootScreenPhase::MODULES;
        display.showInfo("START / MODULY", "RFID: " + rfidStatus, cfg.keypad.enabled ? ("KEYPAD: " + String(keypadDetected ? "OK" : "BRAK")) : "KEYPAD: OFF", "IP: " + ipText(), "NET: " + String(cfg.network.mode == NetworkMode::DHCP ? "DHCP" : "STATIC"));
        return;
    }
    if (elapsed < 10000UL) {
        bootPhase = BootScreenPhase::TCP;
        String cmdLine1 = "CMD: " + tcpModeLabel(cfg.tcp.mode);
        String cmdLine2 = (cfg.tcp.mode == TcpMode::CLIENT) ? ("-> " + cfg.tcp.serverIp + ":" + String(cfg.tcp.serverPort)) : ("LISTEN:" + String(cfg.tcp.listenPort));
        String scaleLine1 = "WAGA: " + String(cfg.scaleTcp.enabled ? tcpModeLabel(cfg.scaleTcp.mode) : "OFF");
        String scaleLine2 = !cfg.scaleTcp.enabled ? "-" : ((cfg.scaleTcp.mode == TcpMode::CLIENT) ? ("-> " + cfg.scaleTcp.serverIp + ":" + String(cfg.scaleTcp.serverPort)) : ("LISTEN:" + String(cfg.scaleTcp.listenPort)));
        display.showInfo("START / TCP", cmdLine1, cmdLine2, scaleLine1, scaleLine2);
        return;
    }
    bootPhase = BootScreenPhase::DONE;
}

static void applyRuntimeConfig() {
    ArduinoOTA.setHostname(cfg.network.deviceName.c_str());
    ArduinoOTA.setPassword(cfg.security.otaPassword.c_str());
    ArduinoOTA.setPort(3232);
    ArduinoOTA.begin();

    if (cfg.display.enabled) {
        display.begin(cfg.display.contrast);
        display.showLogo();
        bootSequenceStart = millis();
        bootPhase = BootScreenPhase::LOGO;
    }

    if (cfg.rfid.enabled) {
        rfid.begin(cfg.rfid.baudRate, Pins::RFID_RX, Pins::RFID_TX, cfg.rfid.encoding);
        rfidStatus = "ON(UART)";
    } else {
        rfidStatus = "OFF";
    }

    keypadDetected = cfg.keypad.enabled ? keypad.begin(cfg.keypad.pcf8574Address, Pins::I2C_SDA, Pins::I2C_SCL) : false;
    tcpManager.begin(cfg.tcp);
    if (cfg.scaleTcp.enabled) {
        TcpSettings s;
        s.mode = cfg.scaleTcp.mode;
        s.serverIp = cfg.scaleTcp.serverIp;
        s.serverPort = cfg.scaleTcp.serverPort;
        s.listenPort = cfg.scaleTcp.listenPort;
        scaleTcpManager.begin(s);
    }
    webServer.begin(cfg);
    if (cfg.discovery.enabled) {
        DiscoveryInfo info;
        info.deviceId = String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
        info.deviceName = cfg.network.deviceName;
        info.fwVersion = "0.1.0";
        info.tcpPort = cfg.tcp.listenPort;
        info.httpPort = 80;
        info.configApiEnabled = true;
        info.rfidEnabled = cfg.rfid.enabled;
        info.keypadEnabled = cfg.keypad.enabled;
        discoveryService.begin(info, cfg.discovery.udpPort);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    logger.info("Booting CT-100");
    configManager.begin();
    cfg = configManager.load();
    netManager.begin(cfg.network);
    ArduinoOTA.onStart([]() { logger.warn("OTA start"); });
    ArduinoOTA.onEnd([]() { logger.warn("OTA end"); });
    ArduinoOTA.onError([](ota_error_t error) { logger.error("OTA error=" + String((int)error)); });
    applyRuntimeConfig();

    tcpManager.onLineReceived([](const String& line) {
        lastInboundFrame = line;
        if (line.startsWith("LCD:")) {
            lcdCustomText = line.substring(4);
            lcdCustomText.trim();
            lcdCustomUntil = millis() + 5000UL;
            if (cfg.display.enabled) display.showTcp(lcdCustomText);
            return;
        }
        if (line.startsWith("STATUS:")) {
            remoteStatusText = line.substring(7);
            remoteStatusText.trim();
            if (remoteStatusText.isEmpty()) remoteStatusText = "Oczekuje na wazenie";
            remoteStatusUntil = millis() + 5000UL;
            return;
        }
    });

    scaleTcpManager.onLineReceived([](const String& line) {
        String weight = line;
        weight.trim();
        if (weight.isEmpty()) return;
        currentWeight = weight;
        lastInboundFrame = "SCALE:" + weight;
        logger.info("Scale ASCII: " + currentWeight);
    });

    rfid.onCard([](const String& raw, const String& encoded) {
        lastCard = encoded;
        if (cfg.display.enabled) display.showCard(encoded);
        if (cfg.rfid.encoding == RfidEncoding::SCALE_FRAME_MODE) {
            const String frame = RfidFrameEncoder::encode(raw, RfidFrameEncoder::Mode::CT100_FRAME);
            if (frame.isEmpty()) {
                logger.warn("RFID CT100 frame build failed");
                return;
            }
            lastOutboundFrame = frame;
            tcpManager.sendLine(frame);
            return;
        }
        const String payload = "RFID:" + encoded + ";RAW:" + raw;
        lastOutboundFrame = payload;
        tcpManager.sendLine(payload);
    });

    keypad.onKey([](char key) {
        lastKey = String(key);
        const String payload = "KEY:" + String(key);
        lastOutboundFrame = payload;
        tcpManager.sendLine(payload);
    });

    webServer.setConfigProvider([]() { return cfg; });
    webServer.setStatusProvider([]() { return buildStatusText(); });
    webServer.setRuntimeJsonProvider([]() {
        String out;
        const bool cmdTcpConnected = tcpManager.isConnected() || tcpManager.hasClient();
        const bool scaleConnected = cfg.scaleTcp.enabled && (scaleTcpManager.isConnected() || scaleTcpManager.hasClient());
        out += "{";
        out += "\"ip\":\"" + jsonEscapeLocal(netManager.localIP().toString()) + "\",";
        out += "\"uptimeMs\":" + String(millis()) + ",";
        out += "\"rfidLast\":\"" + jsonEscapeLocal(lastCard) + "\",";
        out += "\"keyLast\":\"" + jsonEscapeLocal(lastKey) + "\",";
        out += "\"lastOutbound\":\"" + jsonEscapeLocal(lastOutboundFrame) + "\",";
        out += "\"lastInbound\":\"" + jsonEscapeLocal(lastInboundFrame) + "\",";
        out += "\"cmdTcpConnected\":" + String(cmdTcpConnected ? "true" : "false") + ",";
        out += "\"cmdTcpLast\":\"" + jsonEscapeLocal(tcpManager.lastMessage()) + "\",";
        out += "\"scaleTcpConnected\":" + String(scaleConnected ? "true" : "false") + ",";
        out += "\"scaleTcpLast\":\"" + jsonEscapeLocal(scaleTcpManager.lastMessage()) + "\",";
        out += "\"keypadDetected\":" + String(keypadDetected ? "true" : "false") + ",";
        out += "\"rfidEnabled\":" + String(cfg.rfid.enabled ? "true" : "false") + ",";
        out += "\"displayEnabled\":" + String(cfg.display.enabled ? "true" : "false") + ",";
        out += "\"keypadEnabled\":" + String(cfg.keypad.enabled ? "true" : "false") + ",";
        out += "\"discoveryEnabled\":" + String(cfg.discovery.enabled ? "true" : "false");
        out += "}";
        return out;
    });
    webServer.onSave([](DeviceConfig newCfg) {
        configManager.save(newCfg);
        cfg = newCfg;
        logger.warn("Config persisted. Restart required.");
    });
    webServer.onReboot([]() { ESP.restart(); });
}

void loop() {
    ArduinoOTA.handle();
    netManager.loop();
    webServer.loop();
    tcpManager.loop();
    if (cfg.scaleTcp.enabled) scaleTcpManager.loop();
    if (cfg.discovery.enabled) discoveryService.loop();
    if (cfg.rfid.enabled) rfid.loop();
    if (cfg.keypad.enabled) keypad.loop();
    if (cfg.display.enabled && millis() - lastStatusRefresh > 250UL) {
        lastStatusRefresh = millis();
        if (bootPhase != BootScreenPhase::DONE) renderBootSequence();
        else if (lcdCustomUntil > millis()) display.showTcp(lcdCustomText);
        else display.showIdleWeight(activeHeaderText(), currentWeight.isEmpty() ? String("---") : currentWeight, "Zbliz karte RFID");
    }
}
