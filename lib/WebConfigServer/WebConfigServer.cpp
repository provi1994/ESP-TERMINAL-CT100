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

void WebConfigServer::loop() {
    server_.handleClient();
}

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

DeviceConfig WebConfigServer::activeConfig() const {
    return configProvider_ ? configProvider_() : config_;
}

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

bool WebConfigServer::isAdmin() {
    return detectRole() == WebUserRole::ADMIN;
}

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
            default: if ((uint8_t)c >= 32) out += c; break;
        }
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
    out += "\"listenPort\":" + String(cfg.tcp.listenPort) + "},";

    out += "\"scaleTcp\":{";
    out += "\"enabled\":" + String(cfg.scaleTcp.enabled ? "true" : "false") + ",";
    out += "\"mode\":\"" + ConfigManager::tcpModeToString(cfg.scaleTcp.mode) + "\",";
    out += "\"serverIp\":\"" + jsonEscape(cfg.scaleTcp.serverIp) + "\",";
    out += "\"serverPort\":" + String(cfg.scaleTcp.serverPort) + ",";
    out += "\"listenPort\":" + String(cfg.scaleTcp.listenPort) + "},";

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
    out += "\"flowEnabled\":" + String(cfg.display.flow.enabled ? "true" : "false") + "},";

    out += "\"keypad\":{";
    out += "\"enabled\":" + String(cfg.keypad.enabled ? "true" : "false") + ",";
    out += "\"pcf8574Address\":" + String(cfg.keypad.pcf8574Address) + "},";

    out += "\"discovery\":{";
    out += "\"enabled\":" + String(cfg.discovery.enabled ? "true" : "false") + ",";
    out += "\"udpPort\":" + String(cfg.discovery.udpPort) + "},";

    out += "\"security\":{";
    out += "\"serviceUser\":\"" + jsonEscape(cfg.security.serviceUser) + "\"";
    if (admin) out += ",\"adminUser\":\"" + jsonEscape(cfg.security.adminUser) + "\"";
    out += "},";

    out += "\"permissions\":{\"canEditSecurity\":" + String(admin ? "true" : "false") + "}";
    out += "}";
    return out;
}

String WebConfigServer::buildPage(const DeviceConfig& cfg) {
    String html = R"HTML(
<!doctype html>
<html lang="pl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CT-100 panel</title>
<style>
body{font-family:Arial,sans-serif;background:#eef2f5;color:#13202b;margin:0;padding:18px}
.wrap{max-width:1400px;margin:0 auto}.top{display:flex;justify-content:space-between;gap:16px;align-items:center;flex-wrap:wrap;margin-bottom:18px}
.card{background:#fff;border:1px solid #d9e0e6;border-radius:18px;padding:18px;box-shadow:0 8px 24px rgba(0,0,0,.05)}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.grid3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px}.grid4{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px}.row{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}.btn{border:0;border-radius:12px;padding:12px 16px;font-weight:700;cursor:pointer}.blue{background:#0077C0;color:#fff}.green{background:#119700;color:#fff}.red{background:#A60000;color:#fff}.gray{background:#4B4D4F;color:#fff}.orange{background:#F18A00;color:#fff}.light{background:#fff;color:#13202b;border:1px solid #d9e0e6}label{display:block;font-size:12px;font-weight:700;color:#5b6570;margin-bottom:6px;text-transform:uppercase}input,select,textarea{width:100%;padding:11px 12px;border:1px solid #cfd8e3;border-radius:12px;box-sizing:border-box}textarea{min-height:160px}h1,h2,h3{margin:0 0 8px 0}.muted{color:#5b6570}.actions{display:flex;gap:10px;flex-wrap:wrap}.mono{font-family:monospace}.pill{display:inline-block;padding:5px 10px;background:#e9f4fb;border-radius:999px;font-size:12px;font-weight:700;margin:0 6px 6px 0}.small{font-size:12px}.kbd{font-family:monospace;background:#0f172a;color:#fff;border-radius:6px;padding:2px 6px}.preset-list button{margin:0 8px 8px 0}.ok{color:#119700}.warn{color:#A60000}@media(max-width:980px){.grid,.grid3,.grid4,.row{grid-template-columns:1fr}}</style>
</head><body><div class="wrap">
<div class="top"><div><h1>CT-100 · panel intuicyjny</h1><div class="muted">Rozszerzony webserver: flow ważenia, QR-CAM GM805-L, moduły i diagnostyka.</div></div><div class="actions"><button class="btn light" onclick="openUrl('/status')">Status</button><button class="btn light" onclick="openUrl('/logs')">Logi</button><button class="btn light" onclick="openUrl('/firmware')">Firmware</button><button class="btn red" onclick="reboot()">Restart</button></div></div>
<div class="grid">
<div class="card"><h2>Flow ważenia</h2><div class="actions"><button class="btn green" onclick="post('/api/flow/start')">Start ważenia</button><button class="btn red" onclick="post('/api/flow/cancel')">Anuluj flow</button><button class="btn orange" onclick="buzz()">Buzzer</button></div><div style="margin-top:14px" class="muted">Sekwencja zawiera tylko aktywne moduły: RFID, keyboard, QR.</div><div style="margin-top:14px" id="runtimePills"></div></div>
<div class="card"><h2>Szybkie sterowanie</h2><div class="actions"><button class="btn green" onclick="post('/api/output/out1/on')">OUT1 ON</button><button class="btn gray" onclick="post('/api/output/out1/off')">OUT1 OFF</button><button class="btn green" onclick="post('/api/output/out2/on')">OUT2 ON</button><button class="btn gray" onclick="post('/api/output/out2/off')">OUT2 OFF</button></div><div style="margin-top:14px" class="row"><div><label>Wirtualny klawisz</label><input id="virtKey" value="F1"></div><div><label>Wirtualny kod</label><input id="virtCode" placeholder="np. 12345"></div></div><div class="actions" style="margin-top:10px"><button class="btn blue" onclick="sendKey()">Wyślij klawisz</button><button class="btn blue" onclick="sendCode()">Wyślij kod</button></div></div>
</div>
<div class="card" style="margin-top:16px"><h2>Konfiguracja główna</h2><div class="grid4"><div><label>Nazwa urządzenia</label><input id="deviceName"></div><div><label>Tryb sieci</label><select id="networkMode"><option value="dhcp">DHCP</option><option value="static">STATIC</option></select></div><div><label>Kontrast LCD</label><input id="contrast" type="number"></div><div><label>Prefix linii QR -> TCP</label><input id="qrLinePrefix" placeholder="QR:"></div><div><label>RFID</label><select id="rfidEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Keyboard</label><select id="keypadEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>QR-CAM</label><select id="qrEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>QR baud</label><input id="qrBaudRate" type="number"></div><div><label>QR -> TCP</label><select id="qrSendToTcp"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>QR w runtime/web</label><select id="qrPublishToWeb"><option value="true">ON</option><option value="false">OFF</option></select></div><div><label>Startup komendy GM805</label><select id="qrApplyStartupCommands"><option value="true">APPLY</option><option value="false">OFF</option></select></div><div><label>Zapisz do flash po apply</label><select id="qrSaveToFlashAfterApply"><option value="true">YES</option><option value="false">NO</option></select></div><div><label>Opóźnienie startowe [ms]</label><input id="qrStartupCommandDelayMs" type="number"></div><div><label>Przerwa między komendami [ms]</label><input id="qrInterCommandDelayMs" type="number"></div><div><label>Maks. długość ramki</label><input id="qrMaxFrameLength" type="number"></div><div><label>Discovery</label><select id="discoveryEnabled"><option value="true">ON</option><option value="false">OFF</option></select></div></div><div class="actions" style="margin-top:14px"><button class="btn blue" onclick="saveConfig()">Zapisz konfigurację</button></div></div>
<div class="grid" style="margin-top:16px">
<div class="card"><h2>GM805-L · presety i pełne komendy HEX</h2><div class="muted small">Wbudowane przyciski dodają gotowe komendy z manuala. Każdą inną opcję z GM805-L wklejasz poniżej jako osobną linię HEX. Komentarz po <span class="kbd">#</span> jest ignorowany.</div><div class="preset-list actions" style="margin-top:12px"><button class="btn light" onclick="appendPreset('Baud 9600')">Baud 9600</button><button class="btn light" onclick="appendPreset('Find baud')">Find baud</button><button class="btn light" onclick="appendPreset('Continuous profile')">Continuous</button><button class="btn light" onclick="appendPreset('Trigger mode')">Trigger mode</button><button class="btn light" onclick="appendPreset('Full area + all barcodes')">Full area</button><button class="btn light" onclick="appendPreset('Allow Code39')">Code39</button><button class="btn light" onclick="appendPreset('AIM ID on')">AIM ID on</button><button class="btn light" onclick="appendPreset('AIM ID off')">AIM ID off</button><button class="btn light" onclick="appendPreset('Save Flash')">Save Flash</button></div><label style="margin-top:12px">Startup commands HEX</label><textarea id="qrStartupCommandsHex" class="mono" placeholder="7E 00 08 02 00 2A 39 01 A7 EA # baud 9600&#10;7E 00 08 01 00 02 01 AB CD # trigger mode"></textarea><div class="actions" style="margin-top:10px"><button class="btn blue" onclick="applyQrStartupNow()">Wyślij startup teraz</button><button class="btn orange" onclick="saveQrFlashNow()">Save to flash teraz</button><button class="btn gray" onclick="clearQrCommands()">Wyczyść</button></div><div class="row" style="margin-top:12px"><div><label>Jednorazowa komenda HEX</label><input id="qrHexNow" class="mono" placeholder="7E 00 08 01 00 D0 80 AB CD"></div><div><label>&nbsp;</label><button class="btn blue" onclick="sendQrHexNow()">Wyślij teraz</button></div></div><div class="small muted" style="margin-top:12px">Obsługiwane są też wszystkie pozostałe komendy producenta, o ile wpiszesz ich dokładny HEX z manuala GM805-L.</div></div>
<div class="card"><h2>QR runtime</h2><div id="qrLive" class="mono">-</div><div class="row" style="margin-top:12px"><div><label>Ostatnia komenda HEX</label><input id="qrLastCommandHex" readonly class="mono"></div><div><label>Status komendy</label><input id="qrLastCommandStatus" readonly></div></div><div class="small muted" style="margin-top:12px">Jeżeli skaner pracuje po UART TTL, odczyt trafia do runtime JSON, status TXT i opcjonalnie do TCP z prefixem ustawionym wyżej.</div></div>
</div>
<div class="grid" style="margin-top:16px"><div class="card"><h2>Runtime JSON</h2><textarea id="runtimeBox" class="mono" readonly></textarea></div><div class="card"><h2>Status TXT</h2><textarea id="statusBox" class="mono" readonly></textarea></div></div>
</div>
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
function openUrl(url){ window.open(url,'_blank'); }
async function getJson(url){ const r=await fetch(url,{cache:'no-store'}); if(!r.ok) throw new Error(url+' '+r.status); return r.json(); }
async function getText(url){ const r=await fetch(url,{cache:'no-store'}); if(!r.ok) throw new Error(url+' '+r.status); return r.text(); }
async function post(url, body){ const opt={method:'POST'}; if(body!==undefined){ opt.headers={'Content-Type':'text/plain;charset=utf-8'}; opt.body=body; } const r=await fetch(url,opt); if(!r.ok) throw new Error(url+' '+r.status); await refresh(); }
async function reboot(){ if(confirm('Uruchomić urządzenie ponownie?')) await fetch('/reboot',{method:'POST'}); }
async function buzz(){ await fetch('/api/output/buzzer?ms=120',{method:'POST'}); await refresh(); }
async function sendKey(){ await post('/api/keypad/key', document.getElementById('virtKey').value); }
async function sendCode(){ await post('/api/keypad/code', document.getElementById('virtCode').value); }
function appendPreset(name){ const area=document.getElementById('qrStartupCommandsHex'); const line=GM805_PRESETS[name] + ' # ' + name; area.value=(area.value.trim()?area.value.trim()+'\n':'')+line; }
function clearQrCommands(){ document.getElementById('qrStartupCommandsHex').value=''; }
async function sendQrHexNow(){ const value=document.getElementById('qrHexNow').value.trim(); if(!value) return; await post('/api/qr/command','HEX:'+value); }
async function applyQrStartupNow(){ await post('/api/qr/apply-startup'); }
async function saveQrFlashNow(){ await post('/api/qr/save-flash'); }
async function saveConfig(){
 const payload={
  deviceName:deviceName.value.trim(),
  mode:networkMode.value,
  contrast:Number(contrast.value||0),
  rfidEnabled:rfidEnabled.value==='true',
  keypadEnabled:keypadEnabled.value==='true',
  qrEnabled:qrEnabled.value==='true',
  qrBaudRate:Number(qrBaudRate.value||9600),
  qrSendToTcp:qrSendToTcp.value==='true',
  qrPublishToWeb:qrPublishToWeb.value==='true',
  qrApplyStartupCommands:qrApplyStartupCommands.value==='true',
  qrSaveToFlashAfterApply:qrSaveToFlashAfterApply.value==='true',
  qrStartupCommandDelayMs:Number(qrStartupCommandDelayMs.value||0),
  qrInterCommandDelayMs:Number(qrInterCommandDelayMs.value||0),
  qrMaxFrameLength:Number(qrMaxFrameLength.value||256),
  qrLinePrefix:qrLinePrefix.value.trim(),
  qrStartupCommandsHex:qrStartupCommandsHex.value,
  discoveryEnabled:discoveryEnabled.value==='true'
 };
 const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
 if(!r.ok) throw new Error('save '+r.status);
 await refresh();
 alert('Zapisano. Zmiany runtime dla UART/GM805 zastosuj po restarcie albo użyj przycisku "Wyślij startup teraz".');
}
async function refresh(){
 const [cfg,rt,txt]=await Promise.all([getJson('/api/config'),getJson('/api/runtime'),getText('/status')]);
 deviceName.value=cfg.network.deviceName||'';
 networkMode.value=cfg.network.mode||'dhcp';
 contrast.value=cfg.display.contrast||180;
 rfidEnabled.value=String(cfg.rfid.enabled);
 keypadEnabled.value=String(cfg.keypad.enabled);
 qrEnabled.value=String(cfg.qr.enabled);
 qrBaudRate.value=cfg.qr.baudRate||9600;
 qrSendToTcp.value=String(cfg.qr.sendToTcp);
 qrPublishToWeb.value=String(cfg.qr.publishToWeb);
 qrApplyStartupCommands.value=String(cfg.qr.applyStartupCommands);
 qrSaveToFlashAfterApply.value=String(cfg.qr.saveToFlashAfterApply);
 qrStartupCommandDelayMs.value=cfg.qr.startupCommandDelayMs||120;
 qrInterCommandDelayMs.value=cfg.qr.interCommandDelayMs||80;
 qrMaxFrameLength.value=cfg.qr.maxFrameLength||256;
 qrLinePrefix.value=cfg.qr.linePrefix||'QR:';
 qrStartupCommandsHex.value=cfg.qr.startupCommandsHex||'';
 discoveryEnabled.value=String(cfg.discovery.enabled);
 runtimeBox.value=JSON.stringify(rt,null,2);
 statusBox.value=txt;
 qrLive.textContent=(rt.qrLast||'-') + ' | prefix=' + (rt.qrPrefix||'QR:');
 qrLastCommandHex.value=rt.qrLastCommandHex||'';
 qrLastCommandStatus.value=rt.qrLastCommandStatus||'';
 runtimePills.innerHTML='<span class="pill">flow: '+(rt.flowActive?'AKTYWNY':'IDLE')+'</span> <span class="pill">krok: '+(rt.flowStep||'-')+'</span> <span class="pill">moduły: '+(rt.flowModules||'-')+'</span> <span class="pill">qr: '+(rt.qrLast||'-')+'</span>';
}
refresh(); setInterval(refresh,3000);
</script></body></html>
)HTML";
    (void)cfg;
    return html;
}

String WebConfigServer::parseStringField(const String& body, const String& key, const String& fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    const int firstQuote = body.indexOf('"', colonPos + 1);
    if (firstQuote < 0) return fallback;
    const int secondQuote = body.indexOf('"', firstQuote + 1);
    if (secondQuote < 0) return fallback;
    return body.substring(firstQuote + 1, secondQuote);
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

    const String tcpMode = parseStringField(body, "tcpMode", parseStringField(body, "mode", ""));
    if (tcpMode == "client" || tcpMode == "host" || tcpMode == "server") cfg.tcp.mode = ConfigManager::tcpModeFromString(tcpMode);
    cfg.tcp.serverIp = parseStringField(body, "serverIp", cfg.tcp.serverIp);
    cfg.tcp.serverPort = parseUInt16Field(body, "serverPort", cfg.tcp.serverPort);
    cfg.tcp.listenPort = parseUInt16Field(body, "listenPort", cfg.tcp.listenPort);

    cfg.scaleTcp.enabled = parseBoolField(body, "scaleEnabled", cfg.scaleTcp.enabled);
    const String scaleMode = parseStringField(body, "scaleMode", "");
    if (scaleMode == "client" || scaleMode == "host" || scaleMode == "server") cfg.scaleTcp.mode = ConfigManager::tcpModeFromString(scaleMode);
    cfg.scaleTcp.serverIp = parseStringField(body, "scaleServerIp", cfg.scaleTcp.serverIp);
    cfg.scaleTcp.serverPort = parseUInt16Field(body, "scaleServerPort", cfg.scaleTcp.serverPort);
    cfg.scaleTcp.listenPort = parseUInt16Field(body, "scaleListenPort", cfg.scaleTcp.listenPort);

    cfg.rfid.enabled = parseBoolField(body, "rfidEnabled", cfg.rfid.enabled);
    cfg.rfid.baudRate = parseUInt32Field(body, "baudRate", cfg.rfid.baudRate);
    const String rfidEnc = parseStringField(body, "encoding", "");
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
