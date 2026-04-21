#include "WebConfigServer.h"

#include <ETH.h>
#include <Update.h>
#include <base64.h>
#include <ctype.h>
#include <stdlib.h>

WebConfigServer::WebConfigServer(LogManager& logger) : logger_(logger), server_(80) {}

void WebConfigServer::begin(const DeviceConfig& config) {
    config_ = config;
    const char* headerKeys[] = {"Authorization"};
    server_.collectHeaders(headerKeys, 1);

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/logs", HTTP_GET, [this]() { handleLogs(); });
    server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/reboot", HTTP_POST, [this]() { handleReboot(); });
    server_.on("/logout", HTTP_GET, [this]() { handleLogout(); });

    server_.on("/api/device/info", HTTP_GET, [this]() { handleApiDeviceInfo(); });
    server_.on("/api/config", HTTP_GET, [this]() { handleApiConfigGet(); });
    server_.on("/api/config", HTTP_POST, [this]() { handleApiConfigPost(); });
    server_.on("/api/runtime", HTTP_GET, [this]() { handleApiRuntimeGet(); });
    server_.on("/api/flow/start", HTTP_POST, [this]() { handleApiFlowStart(); });
    server_.on("/api/flow/cancel", HTTP_POST, [this]() { handleApiFlowCancel(); });

    server_.on("/api/output/out1/on", HTTP_POST, [this]() { handleApiOutputOut1On(); });
    server_.on("/api/output/out1/off", HTTP_POST, [this]() { handleApiOutputOut1Off(); });
    server_.on("/api/output/out2/on", HTTP_POST, [this]() { handleApiOutputOut2On(); });
    server_.on("/api/output/out2/off", HTTP_POST, [this]() { handleApiOutputOut2Off(); });
    server_.on("/api/output/buzzer", HTTP_POST, [this]() { handleApiOutputBuzzer(); });

    server_.on("/api/keypad/key", HTTP_POST, [this]() { handleApiVirtualKey(); });
    server_.on("/api/keypad/code", HTTP_POST, [this]() { handleApiVirtualCode(); });
    server_.on("/api/qr/command", HTTP_POST, [this]() { handleApiQrCommand(); });
    server_.on("/api/qr/apply-startup", HTTP_POST, [this]() { handleApiQrApplyStartup(); });
    server_.on("/api/qr/save-flash", HTTP_POST, [this]() { handleApiQrSaveFlash(); });

    server_.on("/firmware", HTTP_GET, [this]() { handleFirmwarePage(); });
    server_.on(
        "/firmware/upload",
        HTTP_POST,
        [this]() {
            if (!authenticate()) return;
            const bool ok = !Update.hasError();
            server_.send(
                200,
                "text/html; charset=utf-8",
                ok ? "<h3>Firmware upload OK</h3><p>Urzadzenie uruchomi sie ponownie.</p><p><a href='/'>Powrot</a></p>"
                   : "<h3>Firmware upload blad</h3><p>Sprawdz logi urzadzenia i sprobuj ponownie.</p><p><a href='/firmware'>Powrot</a></p>");
            if (ok) {
                delay(800);
                if (onReboot_) onReboot_();
            }
        },
        [this]() { handleFirmwareUpload(); });

    server_.begin();
    logger_.info("Web config server started on port 80");
}

void WebConfigServer::loop() { server_.handleClient(); }

void WebConfigServer::onSave(std::function<void(DeviceConfig)> callback) { onSave_ = callback; }
void WebConfigServer::onReboot(std::function<void()> callback) { onReboot_ = callback; }
void WebConfigServer::onOutputCommand(std::function<void(const String&)> callback) { onOutputCommand_ = callback; }
void WebConfigServer::onVirtualKey(std::function<void(const String&)> callback) { onVirtualKey_ = callback; }
void WebConfigServer::onVirtualCode(std::function<void(const String&)> callback) { onVirtualCode_ = callback; }
void WebConfigServer::onFlowStart(std::function<void()> callback) { onFlowStart_ = callback; }
void WebConfigServer::onFlowCancel(std::function<void()> callback) { onFlowCancel_ = callback; }
void WebConfigServer::onQrCommand(std::function<void(const String&)> callback) { onQrCommand_ = callback; }
void WebConfigServer::setConfigProvider(std::function<DeviceConfig()> provider) { configProvider_ = provider; }
void WebConfigServer::setStatusProvider(std::function<String()> provider) { statusProvider_ = provider; }
void WebConfigServer::setRuntimeJsonProvider(std::function<String()> provider) { runtimeJsonProvider_ = provider; }

DeviceConfig WebConfigServer::activeConfig() const { return configProvider_ ? configProvider_() : config_; }

WebUserRole WebConfigServer::detectRole() {
    if (!server_.hasHeader("Authorization")) return WebUserRole::NONE;
    const String auth = server_.header("Authorization");
    const DeviceConfig cfg = activeConfig();
    const String adminCreds = cfg.security.adminUser + ":" + cfg.security.adminPassword;
    const String serviceCreds = cfg.security.serviceUser + ":" + cfg.security.servicePassword;
    if (auth == "Basic " + base64::encode(adminCreds)) return WebUserRole::ADMIN;
    if (auth == "Basic " + base64::encode(serviceCreds)) return WebUserRole::SERVICE;
    return WebUserRole::NONE;
}

String WebConfigServer::currentUserName() {
    const DeviceConfig cfg = activeConfig();
    const WebUserRole role = detectRole();
    if (role == WebUserRole::ADMIN) return cfg.security.adminUser;
    if (role == WebUserRole::SERVICE) return cfg.security.serviceUser;
    return "-";
}

String WebConfigServer::currentRoleName() {
    const WebUserRole role = detectRole();
    if (role == WebUserRole::ADMIN) return "admin";
    if (role == WebUserRole::SERVICE) return "service";
    return "none";
}

bool WebConfigServer::isAdmin() { return detectRole() == WebUserRole::ADMIN; }

bool WebConfigServer::authenticate() {
    const DeviceConfig cfg = activeConfig();
    if (server_.authenticate(cfg.security.adminUser.c_str(), cfg.security.adminPassword.c_str())) return true;
    if (server_.authenticate(cfg.security.serviceUser.c_str(), cfg.security.servicePassword.c_str())) return true;
    server_.requestAuthentication(BASIC_AUTH, "CT-100", "Logowanie wymagane");
    return false;
}

void WebConfigServer::handleRoot() {
    if (!authenticate()) return;
    server_.send(200, "text/html; charset=utf-8", buildPage(activeConfig()));
}

void WebConfigServer::handleLogs() {
    if (!authenticate()) return;
    String html;
    html.reserve(1200 + logger_.toHtml().length());
    html += "<!doctype html><html lang='pl'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>CT-100 logi</title>";
    html += "<style>body{font-family:Arial,sans-serif;max-width:1100px;margin:24px auto;padding:0 16px;background:#eef2f5;color:#13202b}a{color:#0077C0}pre{white-space:pre-wrap;background:#0f172a;color:#dbe4ee;padding:16px;border-radius:16px;overflow:auto}.top{display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin-bottom:16px}.btn{background:#0077C0;color:#fff;padding:10px 14px;border-radius:10px;text-decoration:none}</style>";
    html += "</head><body><div class='top'><a class='btn' href='/'>Panel</a><a class='btn' href='/status'>Status TXT</a><a class='btn' href='/logout'>Wyloguj</a></div><h1>Logi i diagnostyka</h1><pre>";
    html += logger_.toHtml();
    html += "</pre></body></html>";
    server_.send(200, "text/html; charset=utf-8", html);
}

void WebConfigServer::handleStatus() {
    if (!authenticate()) return;
    server_.send(200, "text/plain; charset=utf-8", statusProvider_ ? statusProvider_() : String("Brak danych"));
}

void WebConfigServer::handleReboot() {
    if (!authenticate()) return;
    server_.send(200, "text/html; charset=utf-8", "<h3>Restart</h3><p>Urzadzenie uruchamia sie ponownie.</p><p><a href='/'>Powrot</a></p>");
    delay(300);
    if (onReboot_) onReboot_();
}

void WebConfigServer::handleLogout() {
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server_.sendHeader("Pragma", "no-cache");
    server_.sendHeader("Expires", "0");
    server_.sendHeader("WWW-Authenticate", "Basic realm=\"CT-100-logout\"");
    server_.send(401, "text/html; charset=utf-8", "<!doctype html><html><body><h2>Sesja zamknieta</h2><p><a href='/'>Zaloguj ponownie</a></p></body></html>");
}

void WebConfigServer::handleApiDeviceInfo() {
    if (!authenticate()) return;
    server_.send(200, "application/json; charset=utf-8", buildDeviceInfoJson(activeConfig()));
}

void WebConfigServer::handleApiConfigGet() {
    if (!authenticate()) return;
    server_.send(200, "application/json; charset=utf-8", buildConfigJson(activeConfig()));
}

void WebConfigServer::handleApiRuntimeGet() {
    if (!authenticate()) return;
    server_.send(200, "application/json; charset=utf-8", runtimeJsonProvider_ ? runtimeJsonProvider_() : String("{}"));
}

void WebConfigServer::handleApiConfigPost() {
    if (!authenticate()) return;
    DeviceConfig cfg = activeConfig();
    const bool allowSecurity = isAdmin();
    applyConfigFromJson(cfg, server_.arg("plain"), allowSecurity);
    if (onSave_) onSave_(cfg);
    logger_.warn("Configuration saved from REST API");
    server_.send(200, "application/json; charset=utf-8", String("{\"ok\":true,\"restartRequired\":true,\"securityEditable\":") + (allowSecurity ? "true" : "false") + "}");
}

void WebConfigServer::handleApiOutputOut1On() { if (!authenticate()) return; if (onOutputCommand_) onOutputCommand_("OUT1:ON"); server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT1:ON\"}"); }
void WebConfigServer::handleApiOutputOut1Off() { if (!authenticate()) return; if (onOutputCommand_) onOutputCommand_("OUT1:OFF"); server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT1:OFF\"}"); }
void WebConfigServer::handleApiOutputOut2On() { if (!authenticate()) return; if (onOutputCommand_) onOutputCommand_("OUT2:ON"); server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT2:ON\"}"); }
void WebConfigServer::handleApiOutputOut2Off() { if (!authenticate()) return; if (onOutputCommand_) onOutputCommand_("OUT2:OFF"); server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT2:OFF\"}"); }

void WebConfigServer::handleApiOutputBuzzer() {
    if (!authenticate()) return;
    String ms = server_.arg("ms");
    ms.trim();
    if (ms.isEmpty()) ms = "120";
    if (onOutputCommand_) onOutputCommand_("BUZZER:" + ms);
    server_.send(200, "application/json; charset=utf-8", String("{\"ok\":true,\"cmd\":\"BUZZER:") + ms + "\"}");
}

void WebConfigServer::handleApiVirtualKey() {
    if (!authenticate()) return;
    String key = server_.arg("plain");
    if (key.isEmpty()) key = server_.arg("key");
    key.trim();
    if (key.isEmpty()) {
        server_.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"missing_key\"}");
        return;
    }
    if (onVirtualKey_) onVirtualKey_(key);
    server_.send(200, "application/json; charset=utf-8", String("{\"ok\":true,\"key\":\"") + jsonEscape(key) + "\"}");
}

void WebConfigServer::handleApiVirtualCode() {
    if (!authenticate()) return;
    String code = server_.arg("plain");
    if (code.isEmpty()) code = server_.arg("code");
    code.trim();
    if (code.isEmpty()) {
        server_.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"missing_code\"}");
        return;
    }
    if (onVirtualCode_) onVirtualCode_(code);
    server_.send(200, "application/json; charset=utf-8", String("{\"ok\":true,\"code\":\"") + jsonEscape(code) + "\"}");
}

void WebConfigServer::handleApiFlowStart() {
    if (!authenticate()) return;
    if (onFlowStart_) onFlowStart_();
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"flow\":\"started\"}");
}

void WebConfigServer::handleApiFlowCancel() {
    if (!authenticate()) return;
    if (onFlowCancel_) onFlowCancel_();
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"flow\":\"cancelled\"}");
}

void WebConfigServer::handleApiQrCommand() {
    if (!authenticate()) return;
    String cmd = server_.arg("plain");
    if (cmd.isEmpty()) cmd = server_.arg("cmd");
    cmd.trim();
    if (cmd.isEmpty()) {
        server_.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"missing_cmd\"}");
        return;
    }
    if (onQrCommand_) onQrCommand_(cmd);
    server_.send(200, "application/json; charset=utf-8", String("{\"ok\":true,\"cmd\":\"") + jsonEscape(cmd) + "\"}");
}

void WebConfigServer::handleApiQrApplyStartup() {
    if (!authenticate()) return;
    if (onQrCommand_) onQrCommand_("APPLY_STARTUP");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"action\":\"apply_startup\"}");
}

void WebConfigServer::handleApiQrSaveFlash() {
    if (!authenticate()) return;
    if (onQrCommand_) onQrCommand_("SAVE_FLASH");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"action\":\"save_flash\"}");
}

void WebConfigServer::handleFirmwarePage() {
    if (!authenticate()) return;
    String html;
    html += "<!doctype html><html><body style='font-family:Arial,sans-serif;max-width:820px;margin:24px auto'>";
    html += "<h1>Firmware / OTA</h1><p>Rola: <strong>" + currentRoleName() + "</strong></p>";
    html += "<form method='POST' action='/firmware/upload' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button type='submit'>Wgraj firmware</button></form>";
    html += "<p><a href='/'>Powrot do panelu</a></p></body></html>";
    server_.send(200, "text/html; charset=utf-8", html);
}

void WebConfigServer::handleFirmwareUpload() {
    if (!authenticate()) return;
    HTTPUpload& upload = server_.upload();
    if (upload.status == UPLOAD_FILE_START) {
        logger_.warn("Firmware upload start: " + upload.filename);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            logger_.error("Update.begin failed");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            logger_.error("Firmware write failed");
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) logger_.warn("Firmware upload success");
        else {
            Update.printError(Serial);
            logger_.error("Firmware upload failed");
        }
    }
}

String WebConfigServer::jsonEscape(const String& value) {
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
            default:
                if ((uint8_t)c >= 32) out += c;
                break;
        }
    }
    return out;
}

String WebConfigServer::jsonUnescape(const String& value) {
    String out;
    out.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c == '\\' && (i + 1) < value.length()) {
            const char n = value[i + 1];
            if (n == 'n') { out += '\n'; ++i; continue; }
            if (n == 'r') { out += '\r'; ++i; continue; }
            if (n == 't') { out += '\t'; ++i; continue; }
            if (n == '"') { out += '"'; ++i; continue; }
            if (n == '\\') { out += '\\'; ++i; continue; }
        }
        out += c;
    }
    return out;
}

String WebConfigServer::buildDeviceInfoJson(const DeviceConfig& cfg) {
    String out = "{";
    out += "\"deviceId\":\"" + jsonEscape(String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX)) + "\",";
    out += "\"deviceName\":\"" + jsonEscape(cfg.network.deviceName) + "\",";
    out += "\"fw\":\"0.1.0\",";
    out += "\"ip\":\"" + ETH.localIP().toString() + "\",";
    out += "\"mac\":\"" + ETH.macAddress() + "\",";
    out += "\"tcpListenPort\":" + String(cfg.tcp.listenPort) + ",";
    out += "\"scaleListenPort\":" + String(cfg.scaleTcp.listenPort) + ",";
    out += "\"discoveryPort\":" + String(cfg.discovery.udpPort) + ",";
    out += "\"currentRole\":\"" + currentRoleName() + "\"";
    out += "}";
    return out;
}

String WebConfigServer::buildConfigJson(const DeviceConfig& cfg) {
    const bool admin = isAdmin();
    String out = "{";
    out += "\"network\":{";
    out += "\"deviceName\":\"" + jsonEscape(cfg.network.deviceName) + "\",";
    out += "\"mode\":\"" + ConfigManager::networkModeToString(cfg.network.mode) + "\",";
    out += "\"ip\":\"" + cfg.network.ip.toString() + "\",";
    out += "\"gateway\":\"" + cfg.network.gateway.toString() + "\",";
    out += "\"subnet\":\"" + cfg.network.subnet.toString() + "\",";
    out += "\"dns1\":\"" + cfg.network.dns1.toString() + "\",";
    out += "\"dns2\":\"" + cfg.network.dns2.toString() + "\"},";

    out += "\"tcp\":{";
    out += "\"mode\":\"" + ConfigManager::tcpModeToString(cfg.tcp.mode) + "\",";
    out += "\"serverIp\":\"" + jsonEscape(cfg.tcp.serverIp) + "\",";
    out += "\"serverPort\":" + String(cfg.tcp.serverPort) + ",";
    out += "\"listenPort\":" + String(cfg.tcp.listenPort) + ",";
    out += "\"autoReconnect\":" + String(cfg.tcp.autoReconnect ? "true" : "false") + ",";
    out += "\"reconnectIntervalMs\":" + String(cfg.tcp.reconnectIntervalMs) + ",";
    out += "\"connectTimeoutMs\":" + String(cfg.tcp.connectTimeoutMs) + "},";

    out += "\"scaleTcp\":{";
    out += "\"enabled\":" + String(cfg.scaleTcp.enabled ? "true" : "false") + ",";
    out += "\"mode\":\"" + ConfigManager::tcpModeToString(cfg.scaleTcp.mode) + "\",";
    out += "\"serverIp\":\"" + jsonEscape(cfg.scaleTcp.serverIp) + "\",";
    out += "\"serverPort\":" + String(cfg.scaleTcp.serverPort) + ",";
    out += "\"listenPort\":" + String(cfg.scaleTcp.listenPort) + ",";
    out += "\"autoReconnect\":" + String(cfg.scaleTcp.autoReconnect ? "true" : "false") + ",";
    out += "\"reconnectIntervalMs\":" + String(cfg.scaleTcp.reconnectIntervalMs) + ",";
    out += "\"connectTimeoutMs\":" + String(cfg.scaleTcp.connectTimeoutMs) + "},";

    out += "\"rfid\":{";
    out += "\"enabled\":" + String(cfg.rfid.enabled ? "true" : "false") + ",";
    out += "\"baudRate\":" + String(cfg.rfid.baudRate) + ",";
    out += "\"encoding\":\"" + ConfigManager::rfidEncodingToString(cfg.rfid.encoding) + "\"},";

    out += "\"qr\":{";
    out += "\"enabled\":" + String(cfg.qr.enabled ? "true" : "false") + ",";
    out += "\"baudRate\":" + String(cfg.qr.baudRate) + ",";
    out += "\"sendToTcp\":" + String(cfg.qr.sendToTcp ? "true" : "false") + ",";
    out += "\"publishToWeb\":" + String(cfg.qr.publishToWeb ? "true" : "false") + ",";
    out += "\"applyStartupCommands\":" + String(cfg.qr.applyStartupCommands ? "true" : "false") + ",";
    out += "\"saveToFlashAfterApply\":" + String(cfg.qr.saveToFlashAfterApply ? "true" : "false") + ",";
    out += "\"startupCommandDelayMs\":" + String(cfg.qr.startupCommandDelayMs) + ",";
    out += "\"interCommandDelayMs\":" + String(cfg.qr.interCommandDelayMs) + ",";
    out += "\"maxFrameLength\":" + String(cfg.qr.maxFrameLength) + ",";
    out += "\"linePrefix\":\"" + jsonEscape(cfg.qr.linePrefix) + "\",";
    out += "\"startupCommandsHex\":\"" + jsonEscape(cfg.qr.startupCommandsHex) + "\"},";

    out += "\"display\":{";
    out += "\"enabled\":" + String(cfg.display.enabled ? "true" : "false") + ",";
    out += "\"contrast\":" + String(cfg.display.contrast) + ",";
    out += "\"flowEnabled\":" + String(cfg.display.flow.enabled ? "true" : "false") + ",";
    out += "\"flowRemoteTriggerEnabled\":" + String(cfg.display.flow.remoteTriggerEnabled ? "true" : "false") + ",";
    out += "\"flowWeightTriggerEnabled\":" + String(cfg.display.flow.weightTriggerEnabled ? "true" : "false") + ",";
    out += "\"flowWeightThresholdKg\":" + String(cfg.display.flow.weightThresholdKg) + ",";
    out += "\"flowSummaryScreenMs\":" + String(cfg.display.flow.summaryScreenMs) + ",";
    out += "\"flowResultScreenMs\":" + String(cfg.display.flow.resultScreenMs) + "},";

    out += "\"keypad\":{";
    out += "\"enabled\":" + String(cfg.keypad.enabled ? "true" : "false") + ",";
    out += "\"pcf8574Address\":" + String(cfg.keypad.pcf8574Address) + "},";

    out += "\"discovery\":{";
    out += "\"enabled\":" + String(cfg.discovery.enabled ? "true" : "false") + ",";
    out += "\"udpPort\":" + String(cfg.discovery.udpPort) + "},";

    out += "\"security\":{";
    out += "\"serviceUser\":\"" + jsonEscape(cfg.security.serviceUser) + "\",";
    out += "\"servicePassword\":\"" + (admin ? jsonEscape(cfg.security.servicePassword) : String("")) + "\",";
    out += "\"otaPassword\":\"" + (admin ? jsonEscape(cfg.security.otaPassword) : String("")) + "\"";
    if (admin) {
        out += ",\"adminUser\":\"" + jsonEscape(cfg.security.adminUser) + "\",";
        out += "\"adminPassword\":\"" + jsonEscape(cfg.security.adminPassword) + "\"";
    }
    out += "},";

    out += "\"permissions\":{\"canEditSecurity\":" + String(admin ? "true" : "false") + "}";
    out += "}";
    return out;
}

String WebConfigServer::buildPage(const DeviceConfig& cfg) {
    (void)cfg;
    return R"HTML(
<!doctype html><html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CT-100 panel</title>
<style>
:root{--bg:#eef2f5;--card:#ffffff;--line:#d9e0e6;--text:#13202b;--muted:#5b6570;--blue:#0077C0;--green:#119700;--red:#A60000;--gray:#4B4D4F;--orange:#F18A00;}
*{box-sizing:border-box}body{font-family:Arial,sans-serif;background:var(--bg);color:var(--text);margin:0;padding:18px}.wrap{max-width:1500px;margin:0 auto}
.top{display:flex;justify-content:space-between;gap:16px;align-items:center;flex-wrap:wrap;margin-bottom:18px}.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:18px;box-shadow:0 8px 24px rgba(0,0,0,.05)}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.grid3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px}.grid4{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px}.grid5{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:16px}.row{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.btn{border:0;border-radius:12px;padding:12px 16px;font-weight:700;cursor:pointer}.blue{background:var(--blue);color:#fff}.green{background:var(--green);color:#fff}.red{background:var(--red);color:#fff}.gray{background:var(--gray);color:#fff}.orange{background:var(--orange);color:#fff}.light{background:#fff;color:var(--text);border:1px solid var(--line)}
label{display:block;font-size:12px;font-weight:700;color:var(--muted);margin-bottom:6px;text-transform:uppercase}input,select,textarea{width:100%;padding:11px 12px;border:1px solid #cfd8e3;border-radius:12px;background:#fff}textarea{min-height:140px}h1,h2,h3{margin:0 0 8px 0}.actions{display:flex;gap:10px;flex-wrap:wrap}.muted{color:var(--muted)}.mono{font-family:monospace}.pill{display:inline-block;padding:6px 10px;background:#e9f4fb;border-radius:999px;font-size:12px;font-weight:700;margin:0 6px 6px 0}
#errorBox{display:none;background:#fff2f2;border:1px solid #ffcccc;color:#8a0000;padding:12px;border-radius:12px;margin-bottom:12px;white-space:pre-wrap}.okBox{display:none;background:#f2fff2;border:1px solid #c8eec8;color:#0d6d0d;padding:12px;border-radius:12px;margin-bottom:12px}
.tableLike{display:grid;grid-template-columns:180px 1fr;gap:6px 12px;font-size:14px}.tableLike div:nth-child(odd){font-weight:700;color:var(--muted)}
.lcdBox{margin-top:12px;border:2px solid #1f2937;border-radius:14px;background:#d7e3c0;padding:14px;min-height:250px}.lcdTitle{font-family:monospace;font-size:13px;color:#0b1f11;font-weight:700;margin-bottom:8px}.lcdLine1{font-family:monospace;font-size:20px;color:#102417;font-weight:700;margin:10px 0}.lcdLine{font-family:monospace;font-size:16px;color:#102417;margin:10px 0}
@media(max-width:1100px){.grid,.grid3,.grid4,.grid5,.row{grid-template-columns:1fr}}
</style></head><body><div class="wrap">
<div class="top"><div><h1>CT-100 · panel konfiguracyjny</h1><div class="muted">Pełny webserver: RFID, QR / kamera GM805-L, flow, LCD, wyjścia, TCP komend, TCP wagi, runtime i diagnostyka.</div></div><div class="actions"><button class="btn light" onclick="openUrl('/status')">Status</button><button class="btn light" onclick="openUrl('/logs')">Logi</button><button class="btn light" onclick="openUrl('/firmware')">Firmware</button><button class="btn red" onclick="reboot()">Restart</button></div></div>
<div id="errorBox"></div><div id="okBox" class="okBox"></div>

<div class="grid">
<div class="card"><h2>Podgląd runtime</h2><div id="runtimePills"></div><div class="tableLike" style="margin-top:10px"><div>IP</div><div id="rt_ip">-</div><div>RFID</div><div id="rt_rfid">-</div><div>QR</div><div id="rt_qr">-</div><div>Klawisz</div><div id="rt_key">-</div><div>Kod WWW</div><div id="rt_code">-</div><div>TCP cmd</div><div id="rt_cmd_tcp">-</div><div>TCP waga</div><div id="rt_scale_tcp">-</div><div>OUT1/OUT2/Buzzer</div><div id="rt_outputs">-</div><div>Flow</div><div id="rt_flow">-</div></div></div>
<div class="card"><h2>Podgląd LCD live</h2><div class="muted">Podgląd budowany z runtime. Gdy firmware wystawia dokładne pola LCD, będą użyte. W przeciwnym razie panel pokazuje sensowną symulację aktualnego ekranu.</div><div class="lcdBox"><div id="lcd_title" class="lcdTitle">LCD OFF</div><div id="lcd_line1" class="lcdLine1">-</div><div id="lcd_line2" class="lcdLine">-</div><div id="lcd_line3" class="lcdLine">-</div><div id="lcd_line4" class="lcdLine">-</div></div></div>
</div>

<div class="grid" style="margin-top:16px">
<div class="card"><h2>Szybkie sterowanie</h2><div class="actions"><button class="btn green" onclick="postSimple('/api/output/out1/on')">OUT1 ON</button><button class="btn gray" onclick="postSimple('/api/output/out1/off')">OUT1 OFF</button><button class="btn green" onclick="postSimple('/api/output/out2/on')">OUT2 ON</button><button class="btn gray" onclick="postSimple('/api/output/out2/off')">OUT2 OFF</button><button class="btn orange" onclick="buzz()">Buzzer</button></div><div class="actions" style="margin-top:12px"><button class="btn green" onclick="postSimple('/api/flow/start')">Start flow</button><button class="btn red" onclick="postSimple('/api/flow/cancel')">Anuluj flow</button></div><div class="row" style="margin-top:14px"><div><label>Wirtualny klawisz</label><input id="virtKey" value="F1"></div><div><label>Wirtualny kod</label><input id="virtCode" placeholder="np. 12345"></div></div><div class="actions" style="margin-top:10px"><button class="btn blue" onclick="sendKey()">Wyślij klawisz</button><button class="btn blue" onclick="sendCode()">Wyślij kod</button></div></div>
<div class="card"><h2>Runtime / status</h2><div class="row"><div><label>Runtime JSON</label><textarea id="runtimeBox" class="mono" readonly></textarea></div><div><label>Status TXT</label><textarea id="statusBox" class="mono" readonly></textarea></div></div></div>
</div>

<div class="card" style="margin-top:16px"><h2>Sieć Ethernet</h2><div class="grid5"><div><label>Nazwa urządzenia</label><input id="deviceName"></div><div><label>Tryb sieci</label><select id="networkMode"><option value="dhcp">DHCP</option><option value="static">STATIC</option></select></div><div><label>IP</label><input id="networkIp"></div><div><label>Gateway</label><input id="networkGateway"></div><div><label>Subnet</label><input id="networkSubnet"></div><div><label>DNS1</label><input id="networkDns1"></div><div><label>DNS2</label><input id="networkDns2"></div></div></div>

<div class="grid" style="margin-top:16px">
<div class="card"><h2>TCP komend</h2><div class="grid4"><div><label>Tryb</label><select id="tcpMode"><option value="client">CLIENT</option><option value="host">HOST</option><option value="server">SERVER</option></select></div><div><label>Server IP</label><input id="tcpServerIp"></div><div><label>Server Port</label><input id="tcpServerPort" type="number"></div><div><label>Listen Port</label><input id="tcpListenPort" type="number"></div><div><label>Auto reconnect</label><select id="tcpAutoReconnect"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Reconnect interval [ms]</label><input id="tcpReconnectIntervalMs" type="number"></div><div><label>Connect timeout [ms]</label><input id="tcpConnectTimeoutMs" type="number"></div></div></div>
<div class="card"><h2>TCP wagi</h2><div class="grid4"><div><label>Włączone</label><select id="scaleEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Tryb</label><select id="scaleMode"><option value="client">CLIENT</option><option value="host">HOST</option><option value="server">SERVER</option></select></div><div><label>Server IP</label><input id="scaleServerIp"></div><div><label>Server Port</label><input id="scaleServerPort" type="number"></div><div><label>Listen Port</label><input id="scaleListenPort" type="number"></div><div><label>Auto reconnect</label><select id="scaleAutoReconnect"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Reconnect interval [ms]</label><input id="scaleReconnectIntervalMs" type="number"></div><div><label>Connect timeout [ms]</label><input id="scaleConnectTimeoutMs" type="number"></div></div></div>
</div>

<div class="grid" style="margin-top:16px">
<div class="card"><h2>RFID</h2><div class="grid3"><div><label>Włączone</label><select id="rfidEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Baud</label><input id="rfidBaudRate" type="number"></div><div><label>Encoding</label><select id="rfidEncoding"><option value="hex">HEX</option><option value="dec">DEC</option><option value="raw">RAW</option><option value="scale_frame">SCALE FRAME</option></select></div></div></div>
<div class="card"><h2>Klawiatura / discovery</h2><div class="grid4"><div><label>Klawiatura</label><select id="keypadEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>PCF8574 address</label><input id="pcf8574Address" type="number"></div><div><label>Discovery</label><select id="discoveryEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>UDP port</label><input id="discoveryPort" type="number"></div></div></div>
</div>

<div class="card" style="margin-top:16px"><h2>QR / kamera GM805-L</h2><div class="muted">Panel zachowuje sekcję konfiguracji QR, komendy startup i ręczne wysyłanie HEX. Uwaga sprzętowa: w tej paczce pin QR_RX został przeniesiony na GPIO36.</div><div class="grid5" style="margin-top:12px"><div><label>QR enabled</label><select id="qrEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>QR baud</label><input id="qrBaudRate" type="number"></div><div><label>QR -&gt; TCP</label><select id="qrSendToTcp"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>QR publish web</label><select id="qrPublishToWeb"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Prefix linii</label><input id="qrLinePrefix"></div><div><label>Apply startup</label><select id="qrApplyStartupCommands"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Save to flash</label><select id="qrSaveToFlashAfterApply"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Startup delay [ms]</label><input id="qrStartupCommandDelayMs" type="number"></div><div><label>Inter-command delay [ms]</label><input id="qrInterCommandDelayMs" type="number"></div><div><label>Max frame len</label><input id="qrMaxFrameLength" type="number"></div></div><label style="margin-top:12px">Startup commands HEX</label><textarea id="qrStartupCommandsHex" class="mono" placeholder="7E 00 08 02 00 2A 39 01 A7 EA # baud 9600&#10;7E 00 08 01 00 02 01 AB CD # trigger mode"></textarea><div class="actions" style="margin-top:10px"><button class="btn light" onclick="appendPreset('Baud 9600')">Baud 9600</button><button class="btn light" onclick="appendPreset('Find baud')">Find baud</button><button class="btn light" onclick="appendPreset('Continuous profile')">Continuous</button><button class="btn light" onclick="appendPreset('Trigger mode')">Trigger mode</button><button class="btn light" onclick="appendPreset('Full area + all barcodes')">Full area</button><button class="btn light" onclick="appendPreset('Allow Code39')">Code39</button><button class="btn light" onclick="appendPreset('AIM ID on')">AIM ID on</button><button class="btn light" onclick="appendPreset('AIM ID off')">AIM ID off</button><button class="btn light" onclick="appendPreset('Save Flash')">Save Flash</button></div><div class="actions" style="margin-top:10px"><button class="btn blue" onclick="applyQrStartupNow()">Wyślij startup teraz</button><button class="btn orange" onclick="saveQrFlashNow()">Save to flash teraz</button><button class="btn gray" onclick="clearQrCommands()">Wyczyść</button></div><div class="row" style="margin-top:12px"><div><label>Jednorazowa komenda HEX</label><input id="qrHexNow" class="mono" placeholder="7E 00 08 01 00 D0 80 AB CD"></div><div><label>&nbsp;</label><button class="btn blue" onclick="sendQrHexNow()">Wyślij teraz</button></div></div><div class="row" style="margin-top:12px"><div><label>Ostatnia komenda HEX</label><input id="qrLastCommandHex" readonly class="mono"></div><div><label>Status komendy</label><input id="qrLastCommandStatus" readonly></div></div></div>

<div class="grid" style="margin-top:16px">
<div class="card"><h2>LCD i flow</h2><div class="grid4"><div><label>LCD enabled</label><select id="displayEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Contrast</label><input id="contrast" type="number"></div><div><label>Flow enabled</label><select id="flowEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Remote trigger</label><select id="flowRemoteTriggerEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Weight trigger</label><select id="flowWeightTriggerEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Weight threshold [kg]</label><input id="flowWeightThresholdKg" type="number"></div><div><label>Summary screen [ms]</label><input id="flowSummaryScreenMs" type="number"></div><div><label>Result screen [ms]</label><input id="flowResultScreenMs" type="number"></div></div></div>
<div class="card"><h2>Bezpieczeństwo</h2><div class="grid3"><div><label>Service user</label><input id="serviceUser"></div><div><label>Service password</label><input id="servicePassword" type="password"></div><div><label>OTA password</label><input id="otaPassword" type="password"></div><div><label>Admin user</label><input id="adminUser"></div><div><label>Admin password</label><input id="adminPassword" type="password"></div></div></div>
</div>

<div class="actions" style="margin-top:16px"><button class="btn blue" onclick="saveConfig()">Zapisz konfigurację</button></div>

<script>
const GM805_PRESETS={
 'Baud 9600':'7E 00 08 02 00 2A 39 01 A7 EA',
 'Save Flash':'7E 00 09 01 00 00 00 DE C8',
 'Find baud':'7E 00 07 01 00 2A 02 D8 0F',
 'Continuous profile':'7E 00 08 01 00 00 D6 AB CD',
 'Trigger mode':'7E 00 08 01 00 02 01 AB CD',
 'Full area + all barcodes':'7E 00 08 01 00 2C 02 AB CD',
 'Allow Code39':'7E 00 08 01 00 36 01 AB CD',
 'AIM ID on':'7E 00 08 01 00 D0 80 AB CD',
 'AIM ID off':'7E 00 08 01 00 D0 00 AB CD'
};
const $=(id)=>document.getElementById(id);
function showError(err){$('errorBox').style.display='block';$('errorBox').textContent=String(err&&err.message?err.message:err);}
function clearError(){$('errorBox').style.display='none';$('errorBox').textContent='';}
function showOk(msg){$('okBox').style.display='block';$('okBox').textContent=msg; setTimeout(()=>{$('okBox').style.display='none';},2500);}
function openUrl(url){ window.open(url,'_blank'); }
async function getJson(url){ const r=await fetch(url,{cache:'no-store'}); if(!r.ok) throw new Error(url+' '+r.status); return r.json(); }
async function getText(url){ const r=await fetch(url,{cache:'no-store'}); if(!r.ok) throw new Error(url+' '+r.status); return r.text(); }
async function postSimple(url, body){ const opt={method:'POST'}; if(body!==undefined){ opt.headers={'Content-Type':'text/plain;charset=utf-8'}; opt.body=body; } const r=await fetch(url,opt); if(!r.ok) throw new Error(url+' '+r.status); await refreshAll(); }
async function reboot(){ if(confirm('Uruchomić urządzenie ponownie?')) await fetch('/reboot',{method:'POST'}); }
async function buzz(){ await fetch('/api/output/buzzer?ms=120',{method:'POST'}); await refreshAll(); }
async function sendKey(){ await postSimple('/api/keypad/key',$('virtKey').value.trim()); }
async function sendCode(){ await postSimple('/api/keypad/code',$('virtCode').value.trim()); }
function appendPreset(name){ const area=$('qrStartupCommandsHex'); const line=GM805_PRESETS[name] + ' # ' + name; area.value=(area.value.trim()?area.value.trim()+'\n':'')+line; }
function clearQrCommands(){ $('qrStartupCommandsHex').value=''; }
async function sendQrHexNow(){ const value=$('qrHexNow').value.trim(); if(!value) return; await postSimple('/api/qr/command','HEX:'+value); }
async function applyQrStartupNow(){ await postSimple('/api/qr/apply-startup'); }
async function saveQrFlashNow(){ await postSimple('/api/qr/save-flash'); }
function boolVal(id){ return $(id).value==='true'; }
function numVal(id,def){ const v=Number($(id).value||def); return Number.isFinite(v)?v:def; }
function strVal(id){ return $(id).value||''; }

function fillConfig(cfg){
 $('deviceName').value=cfg.network.deviceName||'';
 $('networkMode').value=cfg.network.mode||'dhcp';
 $('networkIp').value=cfg.network.ip||'';
 $('networkGateway').value=cfg.network.gateway||'';
 $('networkSubnet').value=cfg.network.subnet||'';
 $('networkDns1').value=cfg.network.dns1||'';
 $('networkDns2').value=cfg.network.dns2||'';
 $('tcpMode').value=cfg.tcp.mode||'client';
 $('tcpServerIp').value=cfg.tcp.serverIp||'';
 $('tcpServerPort').value=cfg.tcp.serverPort||7000;
 $('tcpListenPort').value=cfg.tcp.listenPort||7000;
 $('tcpAutoReconnect').value=String(cfg.tcp.autoReconnect);
 $('tcpReconnectIntervalMs').value=cfg.tcp.reconnectIntervalMs||5000;
 $('tcpConnectTimeoutMs').value=cfg.tcp.connectTimeoutMs||350;
 $('scaleEnabled').value=String(cfg.scaleTcp.enabled);
 $('scaleMode').value=cfg.scaleTcp.mode||'client';
 $('scaleServerIp').value=cfg.scaleTcp.serverIp||'';
 $('scaleServerPort').value=cfg.scaleTcp.serverPort||4001;
 $('scaleListenPort').value=cfg.scaleTcp.listenPort||4001;
 $('scaleAutoReconnect').value=String(cfg.scaleTcp.autoReconnect);
 $('scaleReconnectIntervalMs').value=cfg.scaleTcp.reconnectIntervalMs||5000;
 $('scaleConnectTimeoutMs').value=cfg.scaleTcp.connectTimeoutMs||350;
 $('rfidEnabled').value=String(cfg.rfid.enabled);
 $('rfidBaudRate').value=cfg.rfid.baudRate||9600;
 $('rfidEncoding').value=cfg.rfid.encoding||'hex';
 $('qrEnabled').value=String(cfg.qr.enabled);
 $('qrBaudRate').value=cfg.qr.baudRate||9600;
 $('qrSendToTcp').value=String(cfg.qr.sendToTcp);
 $('qrPublishToWeb').value=String(cfg.qr.publishToWeb);
 $('qrApplyStartupCommands').value=String(cfg.qr.applyStartupCommands);
 $('qrSaveToFlashAfterApply').value=String(cfg.qr.saveToFlashAfterApply);
 $('qrStartupCommandDelayMs').value=cfg.qr.startupCommandDelayMs||120;
 $('qrInterCommandDelayMs').value=cfg.qr.interCommandDelayMs||80;
 $('qrMaxFrameLength').value=cfg.qr.maxFrameLength||256;
 $('qrLinePrefix').value=cfg.qr.linePrefix||'QR:';
 $('qrStartupCommandsHex').value=cfg.qr.startupCommandsHex||'';
 $('displayEnabled').value=String(cfg.display.enabled);
 $('contrast').value=cfg.display.contrast||180;
 $('flowEnabled').value=String(cfg.display.flowEnabled);
 $('flowRemoteTriggerEnabled').value=String(cfg.display.flowRemoteTriggerEnabled);
 $('flowWeightTriggerEnabled').value=String(cfg.display.flowWeightTriggerEnabled);
 $('flowWeightThresholdKg').value=cfg.display.flowWeightThresholdKg||500;
 $('flowSummaryScreenMs').value=cfg.display.flowSummaryScreenMs||2500;
 $('flowResultScreenMs').value=cfg.display.flowResultScreenMs||2500;
 $('keypadEnabled').value=String(cfg.keypad.enabled);
 $('pcf8574Address').value=cfg.keypad.pcf8574Address||32;
 $('discoveryEnabled').value=String(cfg.discovery.enabled);
 $('discoveryPort').value=cfg.discovery.udpPort||40404;
 $('serviceUser').value=(cfg.security&&cfg.security.serviceUser)||'';
 $('servicePassword').value=(cfg.security&&cfg.security.servicePassword)||'';
 $('otaPassword').value=(cfg.security&&cfg.security.otaPassword)||'';
 $('adminUser').value=(cfg.security&&cfg.security.adminUser)||'';
 $('adminPassword').value=(cfg.security&&cfg.security.adminPassword)||'';
}

function lcdPreviewFromRuntime(rt){
 if(rt.lcdTitle){ return {title:rt.lcdTitle,l1:rt.lcdLine1||'-',l2:rt.lcdLine2||'-',l3:rt.lcdLine3||'-',l4:rt.lcdLine4||'-'}; }
 if(rt.displayEnabled===false){ return {title:'LCD OFF',l1:'-',l2:'-',l3:'-',l4:'-'}; }
 if(rt.flowActive){
   if(rt.flowStep==='RFID') return {title:'FLOW / RFID',l1:'Zbliz karte',l2:'Czekam na odczyt',l3:rt.rfidLast||'-',l4:''};
   if(rt.flowStep==='KEYPAD') return {title:'FLOW / KEY',l1:rt.webCodeLast||rt.keyLast||'----',l2:'Wprowadz kod',l3:'',l4:''};
   if(rt.flowStep==='QR') return {title:'FLOW / QR',l1:rt.qrLast||'SCAN',l2:'Zeskanuj kod QR',l3:'',l4:''};
   if(rt.flowStep==='SUMMARY') return {title:'FLOW / SUMMARY',l1:'RFID: '+(rt.rfidLast||'-'),l2:'KEY: '+(rt.webCodeLast||rt.keyLast||'-'),l3:'QR: '+(rt.qrLast||'-'),l4:rt.scaleTcpLast||'-'};
 }
 return {title:'OCZEKIWANIE',l1:rt.scaleTcpLast||'---',l2:'Zbliz karte RFID',l3:'QR: '+(rt.qrLast||'-'),l4:'TCP: '+(rt.cmdTcpConnected?'ON':'OFF')};
}
function renderLcdPreview(rt){ const p=lcdPreviewFromRuntime(rt); $('lcd_title').textContent=p.title||'-'; $('lcd_line1').textContent=p.l1||'-'; $('lcd_line2').textContent=p.l2||'-'; $('lcd_line3').textContent=p.l3||'-'; $('lcd_line4').textContent=p.l4||'-'; }

async function saveConfig(){
 clearError();
 const payload={
  deviceName:strVal('deviceName').trim(),mode:strVal('networkMode'),ip:strVal('networkIp').trim(),gateway:strVal('networkGateway').trim(),subnet:strVal('networkSubnet').trim(),dns1:strVal('networkDns1').trim(),dns2:strVal('networkDns2').trim(),
  tcpMode:strVal('tcpMode'),serverIp:strVal('tcpServerIp').trim(),serverPort:numVal('tcpServerPort',7000),listenPort:numVal('tcpListenPort',7000),autoReconnect:boolVal('tcpAutoReconnect'),reconnectIntervalMs:numVal('tcpReconnectIntervalMs',5000),connectTimeoutMs:numVal('tcpConnectTimeoutMs',350),
  scaleEnabled:boolVal('scaleEnabled'),scaleMode:strVal('scaleMode'),scaleServerIp:strVal('scaleServerIp').trim(),scaleServerPort:numVal('scaleServerPort',4001),scaleListenPort:numVal('scaleListenPort',4001),scaleAutoReconnect:boolVal('scaleAutoReconnect'),scaleReconnectIntervalMs:numVal('scaleReconnectIntervalMs',5000),scaleConnectTimeoutMs:numVal('scaleConnectTimeoutMs',350),
  rfidEnabled:boolVal('rfidEnabled'),rfidBaudRate:numVal('rfidBaudRate',9600),rfidEncoding:strVal('rfidEncoding'),
  qrEnabled:boolVal('qrEnabled'),qrBaudRate:numVal('qrBaudRate',9600),qrSendToTcp:boolVal('qrSendToTcp'),qrPublishToWeb:boolVal('qrPublishToWeb'),qrApplyStartupCommands:boolVal('qrApplyStartupCommands'),qrSaveToFlashAfterApply:boolVal('qrSaveToFlashAfterApply'),qrStartupCommandDelayMs:numVal('qrStartupCommandDelayMs',120),qrInterCommandDelayMs:numVal('qrInterCommandDelayMs',80),qrMaxFrameLength:numVal('qrMaxFrameLength',256),qrLinePrefix:strVal('qrLinePrefix').trim(),qrStartupCommandsHex:strVal('qrStartupCommandsHex'),
  displayEnabled:boolVal('displayEnabled'),contrast:numVal('contrast',180),flowEnabled:boolVal('flowEnabled'),flowRemoteTriggerEnabled:boolVal('flowRemoteTriggerEnabled'),flowWeightTriggerEnabled:boolVal('flowWeightTriggerEnabled'),flowWeightThresholdKg:numVal('flowWeightThresholdKg',500),flowSummaryScreenMs:numVal('flowSummaryScreenMs',2500),flowResultScreenMs:numVal('flowResultScreenMs',2500),
  keypadEnabled:boolVal('keypadEnabled'),pcf8574Address:numVal('pcf8574Address',32),discoveryEnabled:boolVal('discoveryEnabled'),udpPort:numVal('discoveryPort',40404),serviceUser:strVal('serviceUser').trim(),servicePassword:strVal('servicePassword'),otaPassword:strVal('otaPassword'),adminUser:strVal('adminUser').trim(),adminPassword:strVal('adminPassword')
 };
 const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
 if(!r.ok) throw new Error('save '+r.status);
 await refreshAll();
 showOk('Zapisano konfigurację.');
}

function fillRuntime(rt, txt){
 $('runtimeBox').value=JSON.stringify(rt,null,2);
 $('statusBox').value=txt;
 $('rt_ip').textContent=rt.ip||'-';
 $('rt_rfid').textContent=rt.rfidLast||'-';
 $('rt_qr').textContent=rt.qrLast||'-';
 $('rt_key').textContent=rt.keyLast||'-';
 $('rt_code').textContent=rt.webCodeLast||'-';
 $('rt_cmd_tcp').textContent=(rt.cmdTcpConnected?'CONNECTED':'DISCONNECTED')+' | '+(rt.cmdTcpLast||'-');
 $('rt_scale_tcp').textContent=(rt.scaleTcpConnected?'CONNECTED':'DISCONNECTED')+' | '+(rt.scaleTcpLast||'-');
 $('rt_outputs').textContent='OUT1='+(rt.out1?'ON':'OFF')+' OUT2='+(rt.out2?'ON':'OFF')+' BUZZ='+(rt.buzzer?'ON':'OFF');
 $('rt_flow').textContent=(rt.flowActive?'ACTIVE':'IDLE')+' | '+(rt.flowStep||'-');
 $('runtimePills').innerHTML='<span class="pill">flow: '+(rt.flowActive?'AKTYWNY':'IDLE')+'</span><span class="pill">krok: '+(rt.flowStep||'-')+'</span><span class="pill">moduły: '+(rt.flowModules||'-')+'</span><span class="pill">qr: '+(rt.qrLast||'-')+'</span><span class="pill">rfid: '+(rt.rfidLast||'-')+'</span><span class="pill">keypad: '+(rt.keypadDetected?'OK':'BRAK')+'</span>';
 renderLcdPreview(rt);
 $('qrLastCommandHex').value=rt.qrLastCommandHex||'';
 $('qrLastCommandStatus').value=rt.qrLastCommandStatus||'';
}

async function refreshAll(){ clearError(); const [cfg,rt,txt]=await Promise.all([getJson('/api/config'),getJson('/api/runtime'),getText('/status')]); fillConfig(cfg); fillRuntime(rt,txt); }
refreshAll().catch(showError); setInterval(()=>refreshAll().catch(showError),2500);
</script></div></body></html>
)HTML";
}

String WebConfigServer::parseStringField(const String& body, const String& key, const String& fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    int pos = colonPos + 1;
    while (pos < (int)body.length() && isspace((unsigned char)body[pos])) ++pos;
    if (pos >= (int)body.length() || body[pos] != '"') return fallback;
    ++pos;

    String raw;
    bool escape = false;
    for (; pos < (int)body.length(); ++pos) {
        const char c = body[pos];
        if (escape) {
            raw += '\\';
            raw += c;
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"') return jsonUnescape(raw);
        raw += c;
    }
    return fallback;
}

bool WebConfigServer::parseBoolField(const String& body, const String& key, bool fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    int start = colonPos + 1;
    while (start < (int)body.length() && isspace((unsigned char)body[start])) ++start;
    if (body.startsWith("true", start)) return true;
    if (body.startsWith("false", start)) return false;
    return fallback;
}

uint16_t WebConfigServer::parseUInt16Field(const String& body, const String& key, uint16_t fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    int start = colonPos + 1;
    while (start < (int)body.length() && isspace((unsigned char)body[start])) ++start;
    int end = start;
    while (end < (int)body.length() && isdigit((unsigned char)body[end])) ++end;
    if (end <= start) return fallback;
    return static_cast<uint16_t>(body.substring(start, end).toInt());
}

uint32_t WebConfigServer::parseUInt32Field(const String& body, const String& key, uint32_t fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    int start = colonPos + 1;
    while (start < (int)body.length() && isspace((unsigned char)body[start])) ++start;
    int end = start;
    while (end < (int)body.length() && isdigit((unsigned char)body[end])) ++end;
    if (end <= start) return fallback;
    return static_cast<uint32_t>(strtoul(body.substring(start, end).c_str(), nullptr, 10));
}

void WebConfigServer::applyConfigFromJson(DeviceConfig& cfg, const String& body, bool allowSecurity) const {
    cfg.network.deviceName = parseStringField(body, "deviceName", cfg.network.deviceName);
    const String netMode = parseStringField(body, "mode", "");
    if (netMode == "dhcp" || netMode == "static") cfg.network.mode = ConfigManager::networkModeFromString(netMode);
    cfg.network.ip = ConfigManager::stringToIp(parseStringField(body, "ip", cfg.network.ip.toString()));
    cfg.network.gateway = ConfigManager::stringToIp(parseStringField(body, "gateway", cfg.network.gateway.toString()));
    cfg.network.subnet = ConfigManager::stringToIp(parseStringField(body, "subnet", cfg.network.subnet.toString()));
    cfg.network.dns1 = ConfigManager::stringToIp(parseStringField(body, "dns1", cfg.network.dns1.toString()));
    cfg.network.dns2 = ConfigManager::stringToIp(parseStringField(body, "dns2", cfg.network.dns2.toString()));

    const String tcpMode = parseStringField(body, "tcpMode", "");
    if (tcpMode == "client" || tcpMode == "host" || tcpMode == "server") cfg.tcp.mode = ConfigManager::tcpModeFromString(tcpMode);
    cfg.tcp.serverIp = parseStringField(body, "serverIp", cfg.tcp.serverIp);
    cfg.tcp.serverPort = parseUInt16Field(body, "serverPort", cfg.tcp.serverPort);
    cfg.tcp.listenPort = parseUInt16Field(body, "listenPort", cfg.tcp.listenPort);
    cfg.tcp.autoReconnect = parseBoolField(body, "autoReconnect", cfg.tcp.autoReconnect);
    cfg.tcp.reconnectIntervalMs = parseUInt16Field(body, "reconnectIntervalMs", cfg.tcp.reconnectIntervalMs);
    cfg.tcp.connectTimeoutMs = parseUInt16Field(body, "connectTimeoutMs", cfg.tcp.connectTimeoutMs);

    cfg.scaleTcp.enabled = parseBoolField(body, "scaleEnabled", cfg.scaleTcp.enabled);
    const String scaleMode = parseStringField(body, "scaleMode", "");
    if (scaleMode == "client" || scaleMode == "host" || scaleMode == "server") cfg.scaleTcp.mode = ConfigManager::tcpModeFromString(scaleMode);
    cfg.scaleTcp.serverIp = parseStringField(body, "scaleServerIp", cfg.scaleTcp.serverIp);
    cfg.scaleTcp.serverPort = parseUInt16Field(body, "scaleServerPort", cfg.scaleTcp.serverPort);
    cfg.scaleTcp.listenPort = parseUInt16Field(body, "scaleListenPort", cfg.scaleTcp.listenPort);
    cfg.scaleTcp.autoReconnect = parseBoolField(body, "scaleAutoReconnect", cfg.scaleTcp.autoReconnect);
    cfg.scaleTcp.reconnectIntervalMs = parseUInt16Field(body, "scaleReconnectIntervalMs", cfg.scaleTcp.reconnectIntervalMs);
    cfg.scaleTcp.connectTimeoutMs = parseUInt16Field(body, "scaleConnectTimeoutMs", cfg.scaleTcp.connectTimeoutMs);

    cfg.rfid.enabled = parseBoolField(body, "rfidEnabled", cfg.rfid.enabled);
    cfg.rfid.baudRate = parseUInt32Field(body, "rfidBaudRate", cfg.rfid.baudRate);
    const String rfidEnc = parseStringField(body, "rfidEncoding", "");
    if (rfidEnc == "hex" || rfidEnc == "dec" || rfidEnc == "raw" || rfidEnc == "scale_frame") cfg.rfid.encoding = ConfigManager::rfidEncodingFromString(rfidEnc);

    cfg.qr.enabled = parseBoolField(body, "qrEnabled", cfg.qr.enabled);
    cfg.qr.baudRate = parseUInt32Field(body, "qrBaudRate", cfg.qr.baudRate);
    cfg.qr.sendToTcp = parseBoolField(body, "qrSendToTcp", cfg.qr.sendToTcp);
    cfg.qr.publishToWeb = parseBoolField(body, "qrPublishToWeb", cfg.qr.publishToWeb);
    cfg.qr.applyStartupCommands = parseBoolField(body, "qrApplyStartupCommands", cfg.qr.applyStartupCommands);
    cfg.qr.saveToFlashAfterApply = parseBoolField(body, "qrSaveToFlashAfterApply", cfg.qr.saveToFlashAfterApply);
    cfg.qr.startupCommandDelayMs = parseUInt16Field(body, "qrStartupCommandDelayMs", cfg.qr.startupCommandDelayMs);
    cfg.qr.interCommandDelayMs = parseUInt16Field(body, "qrInterCommandDelayMs", cfg.qr.interCommandDelayMs);
    cfg.qr.maxFrameLength = parseUInt16Field(body, "qrMaxFrameLength", cfg.qr.maxFrameLength);
    cfg.qr.linePrefix = parseStringField(body, "qrLinePrefix", cfg.qr.linePrefix);
    cfg.qr.startupCommandsHex = parseStringField(body, "qrStartupCommandsHex", cfg.qr.startupCommandsHex);

    cfg.display.enabled = parseBoolField(body, "displayEnabled", cfg.display.enabled);
    cfg.display.contrast = static_cast<uint8_t>(parseUInt16Field(body, "contrast", cfg.display.contrast));
    cfg.display.flow.enabled = parseBoolField(body, "flowEnabled", cfg.display.flow.enabled);
    cfg.display.flow.remoteTriggerEnabled = parseBoolField(body, "flowRemoteTriggerEnabled", cfg.display.flow.remoteTriggerEnabled);
    cfg.display.flow.weightTriggerEnabled = parseBoolField(body, "flowWeightTriggerEnabled", cfg.display.flow.weightTriggerEnabled);
    cfg.display.flow.weightThresholdKg = parseUInt16Field(body, "flowWeightThresholdKg", cfg.display.flow.weightThresholdKg);
    cfg.display.flow.summaryScreenMs = parseUInt16Field(body, "flowSummaryScreenMs", cfg.display.flow.summaryScreenMs);
    cfg.display.flow.resultScreenMs = parseUInt16Field(body, "flowResultScreenMs", cfg.display.flow.resultScreenMs);

    cfg.keypad.enabled = parseBoolField(body, "keypadEnabled", cfg.keypad.enabled);
    cfg.keypad.pcf8574Address = static_cast<uint8_t>(parseUInt16Field(body, "pcf8574Address", cfg.keypad.pcf8574Address));

    cfg.discovery.enabled = parseBoolField(body, "discoveryEnabled", cfg.discovery.enabled);
    cfg.discovery.udpPort = parseUInt16Field(body, "udpPort", cfg.discovery.udpPort);

    if (allowSecurity) {
        cfg.security.adminUser = parseStringField(body, "adminUser", cfg.security.adminUser);
        cfg.security.adminPassword = parseStringField(body, "adminPassword", cfg.security.adminPassword);
        cfg.security.serviceUser = parseStringField(body, "serviceUser", cfg.security.serviceUser);
        cfg.security.servicePassword = parseStringField(body, "servicePassword", cfg.security.servicePassword);
        cfg.security.otaPassword = parseStringField(body, "otaPassword", cfg.security.otaPassword);
    }
}
