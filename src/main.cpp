#include <ArduinoOTA.h>
#include <ETH.h>
#include <vector>
#include <ctype.h>

#include "FirmwareInfo.h"
#include "FlowManager.h"
#include "AppTypes.h"
#include "ConfigManager.h"
#include "DiscoveryService.h"
#include "DisplayController.h"
#include "DisplayManager.h"
#include "KeypadManager.h"
#include "OutputManager.h"
#include "LogManager.h"
#include "NetManager.h"
#include "Pins.h"
#include "QrCamGm805.h"
#include "QrTcpBridge.h"
#include "Rfid125kHzUart.h"
#include "RfidFrameEncoder.h"
#include "RuntimeState.h"
#include "RuntimeStatusBuilder.h"
#include "ShiftRegister74HC595.h"
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
TcpManager keypadTcpManager(logger);
WebConfigServer webServer(logger);
DiscoveryService discoveryService;
ShiftRegister74HC595 outputs595(Pins::SHIFT595_DATA, Pins::SHIFT595_CLOCK, Pins::SHIFT595_LATCH);
QrCamGm805 qrCam(logger);
QrTcpBridge qrTcpBridge(logger);

DeviceConfig cfg;
RuntimeState rt;
OutputManager outputManager(logger, outputs595, rt);
FlowManager flowManager(logger, rt, cfg);
RuntimeStatusBuilder runtimeStatus(cfg, rt, netManager, tcpManager, scaleTcpManager, keypadTcpManager, qrTcpBridge, flowManager);
DisplayController displayController(display, cfg, rt, netManager, scaleTcpManager, flowManager);

static String trimCopy(const String& value) {
    String out = value;
    out.trim();
    return out;
}

static String normalizeQrLinePrefix(const String& value) {
    String out = trimCopy(value);
    if (out.isEmpty()) out = "QR:";
    return out;
}

static String sanitizeWeightDigits(const String& raw) {
    String digits;
    digits.reserve(raw.length());
    for (size_t i = 0; i < raw.length(); ++i) {
        if (isDigit(raw[i])) digits += raw[i];
    }
    while (digits.length() > 1 && digits[0] == '0') digits.remove(0, 1);
    return digits;
}

static void refreshQrDiagnostics() {
    const auto d = qrCam.diagnostics();
    rt.qrLastPublished = d.lastDecoded;
    rt.qrLastRawAscii = d.lastRawAscii;
    rt.qrLastRawHex = d.lastRawHex;
    rt.qrLastCommandHex = d.lastCommandHex;
    rt.qrLastCommandStatus = d.lastCommandStatus;
    rt.qrLastFinalizeReason = d.lastFinalizeReason;
    rt.qrRuntimeStatus = d.runtimeStatus;
    rt.qrLastByteAt = d.lastByteAt;
    rt.qrLastFrameAt = d.lastFrameAt;
    rt.qrCurrentBytesCount = d.currentBytesCount;
    rt.qrLastFrameBytesCount = d.lastFrameBytesCount;
    rt.qrFrameOpen = d.frameOpen;
}

static void applyRuntimeConfig() {
    ArduinoOTA.setHostname(cfg.network.deviceName.c_str());
    ArduinoOTA.setPassword(cfg.security.otaPassword.c_str());
    ArduinoOTA.setPort(3232);
    ArduinoOTA.begin();

    outputManager.begin();
    displayController.begin();

    if (cfg.rfid.enabled) {
        rfid.begin(cfg.rfid.baudRate, Pins::RFID_RX, Pins::RFID_TX, cfg.rfid.encoding);
        rt.rfidStatus = "ON(UART)";
    } else {
        rt.rfidStatus = "OFF";
    }

    qrCam.begin(cfg.qr, Pins::QR_RX, Pins::QR_TX);
    if (cfg.qr.tcpBridgeEnabled) {
        qrTcpBridge.begin(cfg.qr.tcpBridgePort);
    } else {
        qrTcpBridge.stop();
        qrCam.applyStartupCommands();
    }
    refreshQrDiagnostics();

    rt.keypadDetected = cfg.keypad.enabled ? keypad.begin(cfg.keypad.pcf8574Address, Pins::I2C_SDA, Pins::I2C_SCL) : false;
    if (cfg.keypad.enabled && cfg.keypad.tcpEnabled) {
        TcpSettings keyTcp;
        keyTcp.mode = TcpMode::SERVER;
        keyTcp.listenPort = cfg.keypad.tcpPort;
        keyTcp.autoReconnect = false;
        keypadTcpManager.begin(keyTcp);
    }

    tcpManager.begin(cfg.tcp);
    if (cfg.scaleTcp.enabled) {
        TcpSettings s;
        s.mode = cfg.scaleTcp.mode;
        s.serverIp = cfg.scaleTcp.serverIp;
        s.serverPort = cfg.scaleTcp.serverPort;
        s.listenPort = cfg.scaleTcp.listenPort;
        s.autoReconnect = cfg.scaleTcp.autoReconnect;
        s.reconnectIntervalMs = cfg.scaleTcp.reconnectIntervalMs;
        s.connectTimeoutMs = cfg.scaleTcp.connectTimeoutMs;
        scaleTcpManager.begin(s);
    }

    webServer.begin(cfg);

    if (cfg.discovery.enabled) {
        DiscoveryInfo info;
        info.deviceId = String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
        info.deviceName = cfg.network.deviceName;
        info.fwVersion = FirmwareInfo::VERSION;
        info.tcpPort = cfg.tcp.listenPort;
        info.httpPort = 80;
        info.configApiEnabled = true;
        info.rfidEnabled = cfg.rfid.enabled;
        info.keypadEnabled = cfg.keypad.enabled;
        discoveryService.begin(info, cfg.discovery.udpPort);
    }
}

static void handleVirtualKeyFromWeb(const String& key) {
    rt.lastKey = key;
    outputManager.beep(40);

    const String payload = "KEY:" + key;
    rt.lastOutboundFrame = payload;
    tcpManager.sendLine(payload);

    if (cfg.display.enabled) {
        display.showInputScreen("KLAWISZ WWW", key, "Wyslano z panelu");
        rt.lcdCustomText = key;
        rt.lcdCustomUntil = millis() + 2500UL;
    }

    if (rt.flow.active && rt.flow.currentStep == "KEYPAD") {
        rt.lastWebCode = key;
        flowManager.markKeypadDone();
    }

    logger.info("Virtual web key: " + key);
}

static void handleVirtualCodeFromWeb(const String& code) {
    rt.lastWebCode = code;
    outputManager.beep(60);

    const String payload = "CODE:" + code;
    rt.lastOutboundFrame = payload;
    tcpManager.sendLine(payload);

    if (cfg.display.enabled) {
        display.showInputScreen("KOD Z WWW", code, "Wyslano z panelu");
        rt.lcdCustomText = code;
        rt.lcdCustomUntil = millis() + 3000UL;
    }

    if (rt.flow.active && rt.flow.currentStep == "KEYPAD") {
        flowManager.markKeypadDone();
    }

    logger.info("Virtual web code: " + code);
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
        rt.lastInboundFrame = line;

        if (line.startsWith("LCD:")) {
            rt.lcdCustomText = line.substring(4);
            rt.lcdCustomText.trim();
            rt.lcdCustomUntil = millis() + 5000UL;
            if (cfg.display.enabled) display.showTcp(rt.lcdCustomText);
            return;
        }

        if (line.startsWith("STATUS:")) {
            rt.remoteStatusText = line.substring(7);
            rt.remoteStatusText.trim();
            if (rt.remoteStatusText.isEmpty()) rt.remoteStatusText = "Oczekuje na wazenie";
            rt.remoteStatusUntil = millis() + 5000UL;
            return;
        }

        if (line == "FLOW:START") {
            flowManager.start();
            return;
        }

        if (line == "FLOW:CANCEL") {
            flowManager.cancel();
            return;
        }

        outputManager.handleCommand(line, "(TCP)");
    });

    scaleTcpManager.onLineReceived([](const String& line) {
        String weight = sanitizeWeightDigits(line);
        if (weight.isEmpty()) return;

        rt.currentWeight = weight;
        rt.lastWeightAt = millis();
        rt.lastInboundFrame = "SCALE:" + weight;

        logger.info("Scale ASCII: " + rt.currentWeight);
    });

    rfid.onCard([](const String& raw, const String& encoded) {
        rt.lastCard = encoded;
        outputManager.beep(80);

        if (cfg.display.enabled) display.showCard(encoded);

        if (rt.flow.active && rt.flow.currentStep == "RFID") {
            flowManager.markRfidDone();
        }

        if (cfg.rfid.encoding == RfidEncoding::SCALE_FRAME_MODE) {
            const String frame = RfidFrameEncoder::encode(raw, RfidFrameEncoder::Mode::CT100_FRAME);
            if (frame.isEmpty()) {
                logger.warn("RFID CT100 frame build failed");
                return;
            }
            rt.lastOutboundFrame = frame;
            tcpManager.sendLine(frame);
            return;
        }

        const String payload = "RFID:" + encoded + ";RAW:" + raw;
        rt.lastOutboundFrame = payload;
        tcpManager.sendLine(payload);
    });

    keypad.onKey([](char key) {
        rt.lastKey = String(key);
        outputManager.beep(40);

        const String payload = "KEY:" + String(key);
        rt.lastOutboundFrame = payload;

        if (cfg.keypad.sendToMainTcp) {
            tcpManager.sendLine(payload);
        }

        if (cfg.keypad.tcpEnabled) {
            const bool sent = keypadTcpManager.sendLine(payload);
            if (!sent) {
                logger.warn("KEY TCP 4012 no client: " + payload);
            }
        }

        if (cfg.display.enabled) {
            display.showInputScreen("KLAWISZ", String(key), "TCP 4012");
            rt.lcdCustomText = String(key);
            rt.lcdCustomUntil = millis() + 1200UL;
        }

        if (rt.flow.active && rt.flow.currentStep == "KEYPAD") {
            flowManager.markKeypadDone();
        }
    });

    qrCam.onDecoded([](const String& value) {
        refreshQrDiagnostics();
        rt.qrLastPublished = value;

        const String prefix = normalizeQrLinePrefix(cfg.qr.linePrefix);
        const String payload = prefix + value;
        rt.lastOutboundFrame = payload;

        if (cfg.qr.sendToTcp) tcpManager.sendLine(payload);

        outputManager.beep(60);
        logger.info("QR: " + value);

        if (rt.flow.active && rt.flow.currentStep == "QR") {
            flowManager.markQrDone();
        }
    });

    webServer.setConfigProvider([]() { return cfg; });

    webServer.setStatusProvider([]() {
        refreshQrDiagnostics();
        return runtimeStatus.buildText();
    });

    webServer.setRuntimeJsonProvider([]() {
        refreshQrDiagnostics();
        return runtimeStatus.buildJson();
    });

    webServer.onSave([](DeviceConfig newCfg) {
        configManager.save(newCfg);
        cfg = newCfg;
        logger.warn("Config persisted. Restart required.");
    });

    webServer.onOutputCommand([](const String& cmd) { outputManager.handleCommand(cmd, "(WEB)"); });
    webServer.onVirtualKey([](const String& key) { handleVirtualKeyFromWeb(key); });
    webServer.onVirtualCode([](const String& code) { handleVirtualCodeFromWeb(code); });
    webServer.onFlowStart([]() { flowManager.start(); });
    webServer.onFlowCancel([]() { flowManager.cancel(); });

    webServer.onQrCommand([](const String& cmd) {
        if (cfg.qr.tcpBridgeEnabled) {
            logger.warn("QR command ignored from web: TCP bridge active");
            return;
        }

        String value = trimCopy(cmd);

        if (value.equalsIgnoreCase("APPLY_STARTUP")) {
            qrCam.applyStartupCommands();
            refreshQrDiagnostics();
            return;
        }

        if (value.equalsIgnoreCase("SAVE_FLASH")) {
            qrCam.sendHexCommand("7E 00 09 01 00 00 00 DE C8", "web_save_flash");
            refreshQrDiagnostics();
            return;
        }

        if (value.startsWith("HEX:")) {
            qrCam.sendHexCommand(value.substring(4), "web_hex");
            refreshQrDiagnostics();
            return;
        }

        qrCam.sendHexCommand(value, "web_raw");
        refreshQrDiagnostics();
    });

    webServer.onReboot([]() { ESP.restart(); });
}

void loop() {
    ArduinoOTA.handle();
    netManager.loop();
    webServer.loop();
    tcpManager.loop();

    if (cfg.scaleTcp.enabled) scaleTcpManager.loop();
    if (cfg.keypad.enabled && cfg.keypad.tcpEnabled) keypadTcpManager.loop();
    if (cfg.discovery.enabled) discoveryService.loop();
    if (cfg.rfid.enabled) rfid.loop();
    if (cfg.keypad.enabled) keypad.loop();

    if (cfg.qr.enabled) {
        if (cfg.qr.tcpBridgeEnabled) {
            qrTcpBridge.loop(Serial1);
        } else {
            qrCam.loop();
        }
    }

    refreshQrDiagnostics();
    outputManager.loop();
    displayController.loop();
}
