#include <ArduinoOTA.h>
#include <ETH.h>
#include <vector>
#include <ctype.h>

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
WebConfigServer webServer(logger);
DiscoveryService discoveryService;
ShiftRegister74HC595 outputs595(Pins::SHIFT595_DATA, Pins::SHIFT595_CLOCK, Pins::SHIFT595_LATCH);

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
unsigned long lastWeightAt = 0;
String qrLastData;
String lastWebCode;
String qrLastPublished;
String qrLastCommandHex;
String qrLastCommandStatus;

bool out1State = false;
bool out2State = false;
bool buzzerState = false;
unsigned long buzzerOffAt = 0;

enum class BootScreenPhase : uint8_t { LOGO = 0, MODULES = 1, TCP = 2, DONE = 3 };
unsigned long bootSequenceStart = 0;
BootScreenPhase bootPhase = BootScreenPhase::DONE;
bool keypadDetected = false;
String rfidStatus = "OFF";

struct FlowRuntime {
    bool active = false;
    bool completed = false;
    bool rfidDone = false;
    bool keypadDone = false;
    bool qrDone = false;
    unsigned long startedAt = 0;
    unsigned long screenUntil = 0;
    String currentStep = "IDLE";
    String summary = "";
} flow;


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

static String cleanHexLine(String line) {
    line.trim();
    const int hashPos = line.indexOf('#');
    if (hashPos >= 0) line = line.substring(0, hashPos);
    String out;
    out.reserve(line.length());
    for (size_t i = 0; i < line.length(); ++i) {
        const char c = line[i];
        if (isxdigit((unsigned char)c)) out += (char)toupper((unsigned char)c);
    }
    return out;
}

static bool qrSendHexCommand(const String& inputHex, const char* tag) {
    String hex = cleanHexLine(inputHex);
    if (hex.isEmpty()) {
        qrLastCommandStatus = "EMPTY";
        return false;
    }
    if ((hex.length() % 2) != 0) {
        qrLastCommandHex = hex;
        qrLastCommandStatus = "HEX_LEN_ERR";
        logger.error(String("GM805 invalid hex len ") + tag + ": " + hex);
        return false;
    }
    qrLastCommandHex = hex;
    for (size_t i = 0; i < hex.length(); i += 2) {
        const String byteText = hex.substring(i, i + 2);
        const uint8_t value = (uint8_t)strtoul(byteText.c_str(), nullptr, 16);
        Serial1.write(value);
    }
    Serial1.flush();
    qrLastCommandStatus = "OK";
    logger.info(String("GM805 cmd ") + tag + ": " + hex);
    return true;
}

static void qrApplyStartupCommands() {
    if (!cfg.qr.enabled || !cfg.qr.applyStartupCommands) return;
    const String all = cfg.qr.startupCommandsHex;
    if (all.isEmpty()) return;
    delay(cfg.qr.startupCommandDelayMs);
    int start = 0;
    while (start <= (int)all.length()) {
        int end = all.indexOf('\n', start);
        if (end < 0) end = all.length();
        String line = all.substring(start, end);
        line.trim();
        if (!line.isEmpty()) qrSendHexCommand(line, "startup");
        start = end + 1;
        delay(cfg.qr.interCommandDelayMs);
    }
    if (cfg.qr.saveToFlashAfterApply) {
        qrSendHexCommand("7E 00 09 01 00 00 00 DE C8", "save_flash");
    }
}

static void beepBuzzer(unsigned long durationMs);
static void updateFlowStep();

static void qrPublishDecoded(const String& value) {
    qrLastPublished = value;
    const String prefix = normalizeQrLinePrefix(cfg.qr.linePrefix);
    const String payload = prefix + value;
    lastOutboundFrame = payload;
    if (cfg.qr.sendToTcp) tcpManager.sendLine(payload);
    beepBuzzer(60);
    logger.info("QR: " + value);
    if (flow.active && flow.currentStep == "QR") { flow.qrDone = true; updateFlowStep(); }
}

static String jsonEscapeLocal(const String& value) {
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: if ((uint8_t)c >= 32) out += c; break;
        }
    }
    return out;
}

static bool toPhysicalLevel(bool logicalState, bool activeLow) { return activeLow ? !logicalState : logicalState; }
static void setOut1(bool logicalState) { out1State = logicalState; outputs595.setBit(Pins::OUT1_BIT, toPhysicalLevel(logicalState, Pins::OUT1_ACTIVE_LOW)); }
static void setOut2(bool logicalState) { out2State = logicalState; outputs595.setBit(Pins::OUT2_BIT, toPhysicalLevel(logicalState, Pins::OUT2_ACTIVE_LOW)); }
static void setBuzzer(bool logicalState) { buzzerState = logicalState; outputs595.setBit(Pins::BUZZER_BIT, toPhysicalLevel(logicalState, Pins::BUZZER_ACTIVE_LOW)); }
static void beepBuzzer(unsigned long durationMs) { setBuzzer(true); buzzerOffAt = millis() + durationMs; }

static String activeHeaderText() {
    if (remoteStatusUntil > millis() && !remoteStatusText.isEmpty()) return remoteStatusText;
    return "Oczekuje na wazenie";
}

static String sanitizeWeightDigits(const String& raw) {
    String digits;
    digits.reserve(raw.length());
    for (size_t i = 0; i < raw.length(); ++i) if (isDigit(raw[i])) digits += raw[i];
    while (digits.length() > 1 && digits[0] == '0') digits.remove(0, 1);
    return digits;
}

static String buildWeightForDisplay() {
    const bool scaleConnected = cfg.scaleTcp.enabled && (scaleTcpManager.isConnected() || scaleTcpManager.hasClient());
    const bool freshWeight = (millis() - lastWeightAt) <= 5000UL;
    if (!cfg.scaleTcp.enabled) return "WAGA OFF";
    if (!scaleConnected || !freshWeight) return "Brak polaczenia z miernikiem";
    if (currentWeight.isEmpty()) return "Brak polaczenia z miernikiem";
    return currentWeight + " KG";
}

static String buildFlowModules() {
    String out;
    if (cfg.rfid.enabled) out += "RFID ";
    if (cfg.keypad.enabled) out += "KEYPAD ";
    if (cfg.qr.enabled) out += "QR ";
    out.trim();
    if (out.isEmpty()) out = "BRAK";
    return out;
}

static void updateFlowStep() {
    if (!flow.active) {
        flow.currentStep = "IDLE";
        return;
    }
    if (cfg.rfid.enabled && !flow.rfidDone) { flow.currentStep = "RFID"; return; }
    if (cfg.keypad.enabled && !flow.keypadDone) { flow.currentStep = "KEYPAD"; return; }
    if (cfg.qr.enabled && !flow.qrDone) { flow.currentStep = "QR"; return; }
    flow.currentStep = "SUMMARY";
    flow.completed = true;
}

static void startWeighFlow() {
    flow.active = true;
    flow.completed = false;
    flow.rfidDone = !cfg.rfid.enabled;
    flow.keypadDone = !cfg.keypad.enabled;
    flow.qrDone = !cfg.qr.enabled;
    flow.startedAt = millis();
    flow.screenUntil = 0;
    flow.summary = "";
    updateFlowStep();
    logger.info("Flow started from API/UI");
}

static void cancelWeighFlow() {
    flow.active = false;
    flow.completed = false;
    flow.currentStep = "IDLE";
    flow.summary = "anulowano";
    logger.warn("Flow cancelled from API/UI");
}

static String buildStatusText() {
    String out;
    out += "device=" + cfg.network.deviceName + "\n";
    out += "ip=" + netManager.localIP().toString() + "\n";
    out += "rfid_last=" + lastCard + "\n";
    out += "qr_last=" + qrLastPublished + "\n";
    out += "qr_prefix=" + normalizeQrLinePrefix(cfg.qr.linePrefix) + "\n";
    out += "qr_last_cmd=" + qrLastCommandHex + "\n";
    out += "qr_last_cmd_status=" + qrLastCommandStatus + "\n";
    out += "key_last=" + lastKey + "\n";
    out += "web_code_last=" + lastWebCode + "\n";
    out += "last_outbound=" + lastOutboundFrame + "\n";
    out += "last_inbound=" + lastInboundFrame + "\n";
    out += "cmd_tcp_mode=" + ConfigManager::tcpModeToString(cfg.tcp.mode) + "\n";
    out += "cmd_tcp_last=" + tcpManager.lastMessage() + "\n";
    out += "scale_enabled=" + String(cfg.scaleTcp.enabled ? "on" : "off") + "\n";
    out += "scale_tcp_mode=" + ConfigManager::tcpModeToString(cfg.scaleTcp.mode) + "\n";
    out += "scale_display=" + buildWeightForDisplay() + "\n";
    out += "flow_active=" + String(flow.active ? "on" : "off") + "\n";
    out += "flow_step=" + flow.currentStep + "\n";
    out += "flow_modules=" + buildFlowModules() + "\n";
    out += "out1=" + String(out1State ? "on" : "off") + "\n";
    out += "out2=" + String(out2State ? "on" : "off") + "\n";
    out += "buzzer=" + String(buzzerState ? "on" : "off") + "\n";
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
    if (elapsed < 3000UL) { bootPhase = BootScreenPhase::LOGO; display.showLogo(); return; }
    if (elapsed < 6500UL) {
        bootPhase = BootScreenPhase::MODULES;
        display.showInfo("START / MODULY", "RFID: " + rfidStatus,
            cfg.keypad.enabled ? ("KEYPAD: " + String(keypadDetected ? "OK" : "BRAK")) : "KEYPAD: OFF",
            String("QR: ") + (cfg.qr.enabled ? "ON" : "OFF"),
            "IP: " + ipText());
        return;
    }
    if (elapsed < 10000UL) {
        bootPhase = BootScreenPhase::TCP;
        display.showInfo("START / TCP",
            "CMD: " + tcpModeLabel(cfg.tcp.mode),
            "WAGA: " + String(cfg.scaleTcp.enabled ? tcpModeLabel(cfg.scaleTcp.mode) : "OFF"),
            "FLOW: " + String(cfg.display.flow.enabled ? "ON" : "OFF"),
            "MOD: " + buildFlowModules());
        return;
    }
    bootPhase = BootScreenPhase::DONE;
}

static void renderFlowScreen() {
    if (!cfg.display.enabled || !flow.active) return;
    if (flow.currentStep == "RFID") {
        display.showRfidPrompt("FLOW / RFID", "Zbliz karte", "Czekam na odczyt");
        return;
    }
    if (flow.currentStep == "KEYPAD") {
        display.showInputScreen("FLOW / KEY", lastWebCode.isEmpty() ? "----" : lastWebCode, "Wprowadz kod");
        return;
    }
    if (flow.currentStep == "QR") {
        display.showInputScreen("FLOW / QR", qrLastData.isEmpty() ? "SCAN" : qrLastData, "Zeskanuj kod QR");
        return;
    }
    if (flow.currentStep == "SUMMARY") {
        display.showSummaryScreen(
            String("RFID: ") + (lastCard.isEmpty() ? "-" : lastCard),
            String("KEY: ") + (lastWebCode.isEmpty() ? lastKey : lastWebCode),
            String("QR: ") + (qrLastData.isEmpty() ? "-" : qrLastData),
            buildWeightForDisplay());
        return;
    }
}

static void applyRuntimeConfig() {
    ArduinoOTA.setHostname(cfg.network.deviceName.c_str());
    ArduinoOTA.setPassword(cfg.security.otaPassword.c_str());
    ArduinoOTA.setPort(3232);
    ArduinoOTA.begin();

    outputs595.begin();
    setOut1(false);
    setOut2(false);
    setBuzzer(false);

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

    Serial1.begin(cfg.qr.baudRate, SERIAL_8N1, Pins::QR_RX, Pins::QR_TX);
    qrApplyStartupCommands();
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

static void handleOutputCommand(const String& cmd, const String& sourceTag) {
    if (cmd.equalsIgnoreCase("OUT1:ON")) { setOut1(true); logger.info("OUT1=ON " + sourceTag); return; }
    if (cmd.equalsIgnoreCase("OUT1:OFF")) { setOut1(false); logger.info("OUT1=OFF " + sourceTag); return; }
    if (cmd.equalsIgnoreCase("OUT2:ON")) { setOut2(true); logger.info("OUT2=ON " + sourceTag); return; }
    if (cmd.equalsIgnoreCase("OUT2:OFF")) { setOut2(false); logger.info("OUT2=OFF " + sourceTag); return; }
    if (cmd.startsWith("BUZZER:")) {
        String msText = cmd.substring(7); msText.trim();
        unsigned long duration = (unsigned long)msText.toInt();
        if (duration == 0) duration = 120;
        if (duration > 5000UL) duration = 5000UL;
        beepBuzzer(duration);
        logger.info("BUZZER=" + String(duration) + "ms " + sourceTag);
    }
}

static void handleVirtualKeyFromWeb(const String& key) {
    lastKey = key;
    beepBuzzer(40);
    const String payload = "KEY:" + key;
    lastOutboundFrame = payload;
    tcpManager.sendLine(payload);
    if (cfg.display.enabled) { display.showInputScreen("KLAWISZ WWW", key, "Wyslano z panelu"); lcdCustomText = key; lcdCustomUntil = millis() + 2500UL; }
    if (flow.active && flow.currentStep == "KEYPAD") { flow.keypadDone = true; lastWebCode = key; updateFlowStep(); }
    logger.info("Virtual web key: " + key);
}

static void handleVirtualCodeFromWeb(const String& code) {
    lastWebCode = code;
    beepBuzzer(60);
    const String payload = "CODE:" + code;
    lastOutboundFrame = payload;
    tcpManager.sendLine(payload);
    if (cfg.display.enabled) { display.showInputScreen("KOD Z WWW", code, "Wyslano z panelu"); lcdCustomText = code; lcdCustomUntil = millis() + 3000UL; }
    if (flow.active && flow.currentStep == "KEYPAD") { flow.keypadDone = true; updateFlowStep(); }
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
        lastInboundFrame = line;
        if (line.startsWith("LCD:")) {
            lcdCustomText = line.substring(4); lcdCustomText.trim(); lcdCustomUntil = millis() + 5000UL;
            if (cfg.display.enabled) display.showTcp(lcdCustomText);
            return;
        }
        if (line.startsWith("STATUS:")) {
            remoteStatusText = line.substring(7); remoteStatusText.trim(); if (remoteStatusText.isEmpty()) remoteStatusText = "Oczekuje na wazenie"; remoteStatusUntil = millis() + 5000UL; return;
        }
        if (line == "FLOW:START") { startWeighFlow(); return; }
        if (line == "FLOW:CANCEL") { cancelWeighFlow(); return; }
        handleOutputCommand(line, "(TCP)");
    });

    scaleTcpManager.onLineReceived([](const String& line) {
        String weight = sanitizeWeightDigits(line);
        if (weight.isEmpty()) return;
        currentWeight = weight;
        lastWeightAt = millis();
        lastInboundFrame = "SCALE:" + weight;
        logger.info("Scale ASCII: " + currentWeight);
    });

    rfid.onCard([](const String& raw, const String& encoded) {
        lastCard = encoded;
        beepBuzzer(80);
        if (cfg.display.enabled) display.showCard(encoded);
        if (flow.active && flow.currentStep == "RFID") { flow.rfidDone = true; updateFlowStep(); }
        if (cfg.rfid.encoding == RfidEncoding::SCALE_FRAME_MODE) {
            const String frame = RfidFrameEncoder::encode(raw, RfidFrameEncoder::Mode::CT100_FRAME);
            if (frame.isEmpty()) { logger.warn("RFID CT100 frame build failed"); return; }
            lastOutboundFrame = frame; tcpManager.sendLine(frame); return;
        }
        const String payload = "RFID:" + encoded + ";RAW:" + raw;
        lastOutboundFrame = payload;
        tcpManager.sendLine(payload);
    });

    keypad.onKey([](char key) {
        lastKey = String(key);
        beepBuzzer(40);
        const String payload = "KEY:" + String(key);
        lastOutboundFrame = payload;
        tcpManager.sendLine(payload);
        if (flow.active && flow.currentStep == "KEYPAD") { flow.keypadDone = true; updateFlowStep(); }
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
        out += "\"qrLast\":\"" + jsonEscapeLocal(qrLastPublished) + "\",";
        out += "\"qrPrefix\":\"" + jsonEscapeLocal(normalizeQrLinePrefix(cfg.qr.linePrefix)) + "\",";
        out += "\"qrLastCommandHex\":\"" + jsonEscapeLocal(qrLastCommandHex) + "\",";
        out += "\"qrLastCommandStatus\":\"" + jsonEscapeLocal(qrLastCommandStatus) + "\",";
        out += "\"keyLast\":\"" + jsonEscapeLocal(lastKey) + "\",";
        out += "\"webCodeLast\":\"" + jsonEscapeLocal(lastWebCode) + "\",";
        out += "\"lastOutbound\":\"" + jsonEscapeLocal(lastOutboundFrame) + "\",";
        out += "\"lastInbound\":\"" + jsonEscapeLocal(lastInboundFrame) + "\",";
        out += "\"cmdTcpConnected\":" + String(cmdTcpConnected ? "true" : "false") + ",";
        out += "\"cmdTcpLast\":\"" + jsonEscapeLocal(tcpManager.lastMessage()) + "\",";
        out += "\"scaleTcpConnected\":" + String(scaleConnected ? "true" : "false") + ",";
        out += "\"scaleTcpLast\":\"" + jsonEscapeLocal(buildWeightForDisplay()) + "\",";
        out += "\"keypadDetected\":" + String(keypadDetected ? "true" : "false") + ",";
        out += "\"rfidEnabled\":" + String(cfg.rfid.enabled ? "true" : "false") + ",";
        out += "\"qrEnabled\":" + String(cfg.qr.enabled ? "true" : "false") + ",";
        out += "\"displayEnabled\":" + String(cfg.display.enabled ? "true" : "false") + ",";
        out += "\"keypadEnabled\":" + String(cfg.keypad.enabled ? "true" : "false") + ",";
        out += "\"discoveryEnabled\":" + String(cfg.discovery.enabled ? "true" : "false") + ",";
        out += "\"out1\":" + String(out1State ? "true" : "false") + ",";
        out += "\"out2\":" + String(out2State ? "true" : "false") + ",";
        out += "\"buzzer\":" + String(buzzerState ? "true" : "false") + ",";
        out += "\"flowActive\":" + String(flow.active ? "true" : "false") + ",";
        out += "\"flowStep\":\"" + jsonEscapeLocal(flow.currentStep) + "\",";
        out += "\"flowModules\":\"" + jsonEscapeLocal(buildFlowModules()) + "\"";
        out += "}";
        return out;
    });

    webServer.onSave([](DeviceConfig newCfg) {
        configManager.save(newCfg);
        cfg = newCfg;
        logger.warn("Config persisted. Restart required.");
    });
    webServer.onOutputCommand([](const String& cmd) { handleOutputCommand(cmd, "(WEB)"); });
    webServer.onVirtualKey([](const String& key) { handleVirtualKeyFromWeb(key); });
    webServer.onVirtualCode([](const String& code) { handleVirtualCodeFromWeb(code); });
    webServer.onFlowStart([]() { startWeighFlow(); });
    webServer.onFlowCancel([]() { cancelWeighFlow(); });
    webServer.onQrCommand([](const String& cmd) {
        String value = trimCopy(cmd);
        if (value.equalsIgnoreCase("APPLY_STARTUP")) { qrApplyStartupCommands(); return; }
        if (value.equalsIgnoreCase("SAVE_FLASH")) { qrSendHexCommand("7E 00 09 01 00 00 00 DE C8", "web_save_flash"); return; }
        if (value.startsWith("HEX:")) { qrSendHexCommand(value.substring(4), "web_hex"); return; }
        qrSendHexCommand(value, "web_raw");
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

    if (cfg.qr.enabled) {
        while (Serial1.available()) {
            char c = (char)Serial1.read();
            if (c == '\n' || c == '\r') {
                if (!qrLastData.isEmpty()) {
                    qrLastData.trim();
                    if (!qrLastData.isEmpty()) qrPublishDecoded(qrLastData);
                    qrLastData = "";
                }
            } else if (isPrintable((int)c)) {
                qrLastData += c;
                const size_t maxLen = cfg.qr.maxFrameLength > 0 ? cfg.qr.maxFrameLength : 256;
                if (qrLastData.length() > maxLen) qrLastData.remove(0, qrLastData.length() - maxLen);
            }
        }
    }

    if (buzzerState && buzzerOffAt > 0 && millis() >= buzzerOffAt) {
        setBuzzer(false);
        buzzerOffAt = 0;
    }

    if (cfg.display.enabled && millis() - lastStatusRefresh > 250UL) {
        lastStatusRefresh = millis();
        if (bootPhase != BootScreenPhase::DONE) {
            renderBootSequence();
        } else if (flow.active) {
            renderFlowScreen();
        } else if (lcdCustomUntil > millis()) {
            display.showTcp(lcdCustomText);
        } else {
            display.showIdleWeight(activeHeaderText(), buildWeightForDisplay(), "Zbliz karte RFID");
        }
    }
}
