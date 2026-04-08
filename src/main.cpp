#include <ArduinoOTA.h>
#include <ETH.h>
#include "AppTypes.h"
#include "ConfigManager.h"
#include "DiscoveryService.h"
#include "DisplayManager.h"
#include "KeypadManager.h"
#include "LogManager.h"
#include "NetManager.h"
#include "Pins.h"
#include "Rfid125kHzUart.h"
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

String lcdCustomText;
unsigned long lcdCustomUntil = 0;

String remoteStatusText = "Oczekuje na wazenie";
unsigned long remoteStatusUntil = 0;

String currentWeight = "---";

static String activeHeaderText() {
    if (remoteStatusUntil > millis() && !remoteStatusText.isEmpty()) {
        return remoteStatusText;
    }
    return "Oczekuje na wazenie";
}

static String buildStatusText() {
    String out;
    out += "device=" + cfg.network.deviceName + "\n";
    out += "ip=" + netManager.localIP().toString() + "\n";
    out += "rfid_last=" + lastCard + "\n";
    out += "key_last=" + lastKey + "\n";
    out += "cmd_tcp_mode=" + ConfigManager::tcpModeToString(cfg.tcp.mode) + "\n";
    out += "cmd_tcp_last=" + tcpManager.lastMessage() + "\n";
    out += "scale_enabled=" + String(cfg.scaleTcp.enabled ? "on" : "off") + "\n";
    out += "scale_tcp_mode=" + ConfigManager::tcpModeToString(cfg.scaleTcp.mode) + "\n";
    out += "weight=" + currentWeight + "\n";
    out += "header=" + activeHeaderText() + "\n";
    return out;
}

static void applyRuntimeConfig() {
    ArduinoOTA.setHostname(cfg.network.deviceName.c_str());
    ArduinoOTA.setPassword(cfg.security.otaPassword.c_str());
    ArduinoOTA.setPort(3232);
    ArduinoOTA.begin();

    if (cfg.display.enabled) {
        display.begin(cfg.display.contrast);
        display.showBoot(cfg.network.deviceName);
    }

    if (cfg.rfid.enabled) {
        rfid.begin(cfg.rfid.baudRate, Pins::RFID_RX, Pins::RFID_TX, cfg.rfid.encoding);
    }

    if (cfg.keypad.enabled) {
        keypad.begin(cfg.keypad.pcf8574Address, Pins::I2C_SDA, Pins::I2C_SCL);
    }

    tcpManager.begin(cfg.tcp);

    if (cfg.scaleTcp.enabled) {
        TcpSettings scaleSettings;
        scaleSettings.mode = cfg.scaleTcp.mode;
        scaleSettings.serverIp = cfg.scaleTcp.serverIp;
        scaleSettings.serverPort = cfg.scaleTcp.serverPort;
        scaleSettings.listenPort = cfg.scaleTcp.listenPort;
        scaleTcpManager.begin(scaleSettings);
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
    ArduinoOTA.onError([](ota_error_t error) {
        logger.error("OTA error=" + String(static_cast<int>(error)));
    });

    applyRuntimeConfig();

    tcpManager.onLineReceived([](const String& line) {
        if (line.startsWith("LCD:")) {
            lcdCustomText = line.substring(4);
            lcdCustomText.trim();
            lcdCustomUntil = millis() + 5000UL;

            logger.info("LCD custom text: " + lcdCustomText);

            if (cfg.display.enabled) {
                display.showTcp(lcdCustomText);
            }
            return;
        }

        if (line.startsWith("STATUS:")) {
            remoteStatusText = line.substring(7);
            remoteStatusText.trim();
            if (remoteStatusText.isEmpty()) {
                remoteStatusText = "Oczekuje na wazenie";
            }
            remoteStatusUntil = millis() + 5000UL;
            logger.info("Remote status text: " + remoteStatusText);
            return;
        }
    });

    scaleTcpManager.onLineReceived([](const String& line) {
        String weight = line;
        weight.trim();
        if (weight.isEmpty()) return;

        currentWeight = weight;
        logger.info("Scale ASCII: " + currentWeight);
    });

    rfid.onCard([](const String& raw, const String& encoded) {
        lastCard = encoded;
        if (cfg.display.enabled) {
            display.showCard(encoded);
        }
        const String payload = "RFID:" + encoded + ";RAW:" + raw;
        tcpManager.sendLine(payload);
    });

    keypad.onKey([](char key) {
        lastKey = String(key);
        logger.info("Key action registered: " + String(key));
        tcpManager.sendLine("KEY:" + String(key));
    });

    webServer.setConfigProvider([]() { return cfg; });
    webServer.setStatusProvider([]() { return buildStatusText(); });
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

    if (cfg.scaleTcp.enabled) {
        scaleTcpManager.loop();
    }

    if (cfg.discovery.enabled) {
        discoveryService.loop();
    }

    if (cfg.rfid.enabled) rfid.loop();
    if (cfg.keypad.enabled) keypad.loop();

    if (cfg.display.enabled && millis() - lastStatusRefresh > 500UL) {
        lastStatusRefresh = millis();

        if (lcdCustomUntil > millis()) {
            display.showTcp(lcdCustomText);
        } else {
            display.showIdleWeight(
                activeHeaderText(),
                currentWeight.isEmpty() ? String("---") : currentWeight,
                "Zbliz karte RFID"
            );
        }
    }
}