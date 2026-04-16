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

    server_.on("/api/output/out1/on", HTTP_POST, [this]() { handleApiOutputOut1On(); });
    server_.on("/api/output/out1/off", HTTP_POST, [this]() { handleApiOutputOut1Off(); });
    server_.on("/api/output/out2/on", HTTP_POST, [this]() { handleApiOutputOut2On(); });
    server_.on("/api/output/out2/off", HTTP_POST, [this]() { handleApiOutputOut2Off(); });
    server_.on("/api/output/buzzer", HTTP_POST, [this]() { handleApiOutputBuzzer(); });

    server_.on("/api/keypad/key", HTTP_POST, [this]() { handleApiVirtualKey(); });
    server_.on("/api/keypad/code", HTTP_POST, [this]() { handleApiVirtualCode(); });

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

void WebConfigServer::onSave(std::function<void(DeviceConfig)> callback) {
    onSave_ = callback;
}

void WebConfigServer::onReboot(std::function<void()> callback) {
    onReboot_ = callback;
}

void WebConfigServer::onOutputCommand(std::function<void(const String&)> callback) {
    onOutputCommand_ = callback;
}

void WebConfigServer::onVirtualKey(std::function<void(const String&)> callback) {
    onVirtualKey_ = callback;
}

void WebConfigServer::onVirtualCode(std::function<void(const String&)> callback) {
    onVirtualCode_ = callback;
}

void WebConfigServer::setConfigProvider(std::function<DeviceConfig()> provider) {
    configProvider_ = provider;
}

void WebConfigServer::setStatusProvider(std::function<String()> provider) {
    statusProvider_ = provider;
}

void WebConfigServer::setRuntimeJsonProvider(std::function<String()> provider) {
    runtimeJsonProvider_ = provider;
}

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
    const WebUserRole role = detectRole();
    const DeviceConfig cfg = activeConfig();

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
    const DeviceConfig cfg = activeConfig();
    server_.send(200, "text/html; charset=utf-8", buildPage(cfg));
}

void WebConfigServer::handleLogs() {
    if (!authenticate()) return;

    String html;
    html.reserve(1200 + logger_.toHtml().length());
    html += "<!doctype html><html lang='pl'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>CT-100 logi</title>";
    html += "<style>body{font-family:Arial,sans-serif;max-width:1100px;margin:24px auto;padding:0 16px;background:#eef2f5;color:#13202b}"
            "a{color:#0077C0}pre{white-space:pre-wrap;background:#0f172a;color:#dbe4ee;padding:16px;border-radius:16px;overflow:auto}"
            ".top{display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin-bottom:16px}.btn{background:#0077C0;color:#fff;padding:10px 14px;border-radius:10px;text-decoration:none}</style>";
    html += "</head><body>";
    html += "<div class='top'><a class='btn' href='/'>Panel</a><a class='btn' href='/status'>Status TXT</a><a class='btn' href='/logout'>Wyloguj</a></div>";
    html += "<h1>Logi i diagnostyka</h1><pre>";
    html += logger_.toHtml();
    html += "</pre></body></html>";

    server_.send(200, "text/html; charset=utf-8", html);
}

void WebConfigServer::handleStatus() {
    if (!authenticate()) return;
    const String payload = statusProvider_ ? statusProvider_() : String("Brak danych");
    server_.send(200, "text/plain; charset=utf-8", payload);
}

void WebConfigServer::handleReboot() {
    if (!authenticate()) return;
    server_.send(
        200,
        "text/html; charset=utf-8",
        "<h3>Restart</h3><p>Urzadzenie uruchamia sie ponownie.</p><p><a href='/'>Powrot</a></p>");
    delay(300);
    if (onReboot_) onReboot_();
}

void WebConfigServer::handleLogout() {
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server_.sendHeader("Pragma", "no-cache");
    server_.sendHeader("Expires", "0");
    server_.sendHeader("WWW-Authenticate", "Basic realm=\"CT-100-logout\"");
    server_.send(
        401,
        "text/html; charset=utf-8",
        "<!doctype html><html lang='pl'><head><meta charset='utf-8'><title>Wylogowano</title></head>"
        "<body style='font-family:Arial,sans-serif;background:#eef2f5;color:#13202b;padding:24px'>"
        "<h2>Sesja zamknieta</h2><p>Przegladarka dostala zadanie ponownego logowania.</p>"
        "<p><a href='/'>Zaloguj ponownie</a></p></body></html>");
}

void WebConfigServer::handleApiDeviceInfo() {
    if (!authenticate()) return;
    const DeviceConfig cfg = activeConfig();
    server_.send(200, "application/json; charset=utf-8", buildDeviceInfoJson(cfg));
}

void WebConfigServer::handleApiConfigGet() {
    if (!authenticate()) return;
    const DeviceConfig cfg = activeConfig();
    server_.send(200, "application/json; charset=utf-8", buildConfigJson(cfg));
}

void WebConfigServer::handleApiRuntimeGet() {
    if (!authenticate()) return;
    const String payload = runtimeJsonProvider_ ? runtimeJsonProvider_() : String("{}");
    server_.send(200, "application/json; charset=utf-8", payload);
}

void WebConfigServer::handleApiConfigPost() {
    if (!authenticate()) return;

    DeviceConfig cfg = activeConfig();
    const bool allowSecurity = isAdmin();
    applyConfigFromJson(cfg, server_.arg("plain"), allowSecurity);

    if (onSave_) onSave_(cfg);
    logger_.warn("Configuration saved from REST API");

    server_.send(
        200,
        "application/json; charset=utf-8",
        String("{\"ok\":true,\"restartRequired\":true,\"securityEditable\":") +
            (allowSecurity ? "true" : "false") + "}");
}

void WebConfigServer::handleApiOutputOut1On() {
    if (!authenticate()) return;
    if (onOutputCommand_) onOutputCommand_("OUT1:ON");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT1:ON\"}");
}

void WebConfigServer::handleApiOutputOut1Off() {
    if (!authenticate()) return;
    if (onOutputCommand_) onOutputCommand_("OUT1:OFF");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT1:OFF\"}");
}

void WebConfigServer::handleApiOutputOut2On() {
    if (!authenticate()) return;
    if (onOutputCommand_) onOutputCommand_("OUT2:ON");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT2:ON\"}");
}

void WebConfigServer::handleApiOutputOut2Off() {
    if (!authenticate()) return;
    if (onOutputCommand_) onOutputCommand_("OUT2:OFF");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"cmd\":\"OUT2:OFF\"}");
}

void WebConfigServer::handleApiOutputBuzzer() {
    if (!authenticate()) return;

    String ms = server_.arg("ms");
    ms.trim();
    if (ms.isEmpty()) ms = "120";

    if (onOutputCommand_) onOutputCommand_("BUZZER:" + ms);

    server_.send(
        200,
        "application/json; charset=utf-8",
        String("{\"ok\":true,\"cmd\":\"BUZZER:") + ms + "\"}");
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
    logger_.info("Virtual key from web: " + key);
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
    logger_.info("Virtual code from web: " + code);
    server_.send(200, "application/json; charset=utf-8", String("{\"ok\":true,\"code\":\"") + jsonEscape(code) + "\"}");
}

void WebConfigServer::handleFirmwarePage() {
    if (!authenticate()) return;

    String html;
    html.reserve(2200);
    html += "<!doctype html><html lang='pl'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Firmware OTA</title>";
    html += "<style>body{font-family:Arial,sans-serif;max-width:860px;margin:24px auto;padding:0 16px;background:#eef2f5;color:#13202b}"
            "section{background:#fff;border:1px solid #d9e0e6;border-radius:16px;padding:18px;margin:14px 0}"
            "button,input{padding:10px}a{color:#0077C0}.btn{background:#0077C0;color:#fff;border:0;border-radius:10px;padding:12px 16px;cursor:pointer}"
            ".sub{color:#5b6570;line-height:1.45}</style>";
    html += "</head><body>";
    html += "<section><h1>Firmware / OTA</h1>";
    html += "<div class='sub'>Rola: <strong>" + currentRoleName() + "</strong> | Uzytkownik: <strong>" + jsonEscape(currentUserName()) + "</strong></div></section>";
    html += "<section><h2>Upload pliku .bin</h2>";
    html += "<form method='POST' action='/firmware/upload' enctype='multipart/form-data' style='margin-top:16px'>";
    html += "<p><input type='file' name='firmware' accept='.bin' required></p>";
    html += "<p><button class='btn' type='submit'>Wgraj firmware</button></p>";
    html += "</form></section><p><a href='/'>Powrot do panelu</a></p></body></html>";

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
        if (Update.end(true)) {
            logger_.warn("Firmware upload success");
        } else {
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

String WebConfigServer::buildDeviceInfoJson(const DeviceConfig& cfg) {
    String out;
    out.reserve(512);

    out += "{";
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

    String out;
    out.reserve(1800);

    out += "{";

    out += "\"network\":{";
    out += "\"deviceName\":\"" + jsonEscape(cfg.network.deviceName) + "\",";
    out += "\"mode\":\"" + ConfigManager::networkModeToString(cfg.network.mode) + "\",";
    out += "\"ip\":\"" + cfg.network.ip.toString() + "\",";
    out += "\"gateway\":\"" + cfg.network.gateway.toString() + "\",";
    out += "\"subnet\":\"" + cfg.network.subnet.toString() + "\",";
    out += "\"dns1\":\"" + cfg.network.dns1.toString() + "\",";
    out += "\"dns2\":\"" + cfg.network.dns2.toString() + "\"";
    out += "},";

    out += "\"tcp\":{";
    out += "\"mode\":\"" + ConfigManager::tcpModeToString(cfg.tcp.mode) + "\",";
    out += "\"serverIp\":\"" + jsonEscape(cfg.tcp.serverIp) + "\",";
    out += "\"serverPort\":" + String(cfg.tcp.serverPort) + ",";
    out += "\"listenPort\":" + String(cfg.tcp.listenPort);
    out += "},";

    out += "\"scaleTcp\":{";
    out += "\"enabled\":" + String(cfg.scaleTcp.enabled ? "true" : "false") + ",";
    out += "\"mode\":\"" + ConfigManager::tcpModeToString(cfg.scaleTcp.mode) + "\",";
    out += "\"serverIp\":\"" + jsonEscape(cfg.scaleTcp.serverIp) + "\",";
    out += "\"serverPort\":" + String(cfg.scaleTcp.serverPort) + ",";
    out += "\"listenPort\":" + String(cfg.scaleTcp.listenPort);
    out += "},";

    out += "\"rfid\":{";
    out += "\"enabled\":" + String(cfg.rfid.enabled ? "true" : "false") + ",";
    out += "\"baudRate\":" + String(cfg.rfid.baudRate) + ",";
    out += "\"encoding\":\"" + ConfigManager::rfidEncodingToString(cfg.rfid.encoding) + "\"";
    out += "},";

    out += "\"display\":{";
    out += "\"enabled\":" + String(cfg.display.enabled ? "true" : "false") + ",";
    out += "\"contrast\":" + String(cfg.display.contrast);
    out += "},";

    out += "\"keypad\":{";
    out += "\"enabled\":" + String(cfg.keypad.enabled ? "true" : "false") + ",";
    out += "\"pcf8574Address\":" + String(cfg.keypad.pcf8574Address);
    out += "},";

    out += "\"discovery\":{";
    out += "\"enabled\":" + String(cfg.discovery.enabled ? "true" : "false") + ",";
    out += "\"udpPort\":" + String(cfg.discovery.udpPort);
    out += "},";

    out += "\"security\":{";
    out += "\"serviceUser\":\"" + jsonEscape(cfg.security.serviceUser) + "\"";
    if (admin) {
        out += ",\"adminUser\":\"" + jsonEscape(cfg.security.adminUser) + "\"";
    }
    out += "},";

    out += "\"permissions\":{";
    out += "\"canEditSecurity\":" + String(admin ? "true" : "false");
    out += "}";

    out += "}";

    return out;
}

String WebConfigServer::buildPage(const DeviceConfig& cfg) {
    String html = R"HTML(
<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CT-100 panel operatorski</title>
<style>
:root{--bg:#e9ecef;--panel:#a7a7a7;--screen:#f2f2f2;--blue:#0d77bf;--ok:#119700;--cancel:#a60000;--text:#1b1b1b}
*{box-sizing:border-box}body{margin:0;font-family:Arial,sans-serif;background:var(--bg);color:var(--text)}
.wrap{max-width:1280px;margin:0 auto;padding:24px}
.top{display:flex;gap:16px;flex-wrap:wrap;align-items:center;justify-content:space-between;margin-bottom:20px}
.badges{display:flex;gap:10px;flex-wrap:wrap}
.badge{background:#fff;border:1px solid #d0d7df;border-radius:12px;padding:10px 14px}
.btn{border:0;border-radius:10px;padding:12px 16px;font-weight:700;cursor:pointer;text-decoration:none;display:inline-block}
.btn-blue{background:#0d77bf;color:#fff}.btn-gray{background:#4b4d4f;color:#fff}.btn-red{background:#a60000;color:#fff}.btn-green{background:#119700;color:#fff}.btn-orange{background:#d97800;color:#fff}
.layout{display:grid;grid-template-columns:380px 1fr;gap:24px}
.terminal{background:#a7a7a7;border:2px solid #3c3c3c;padding:18px 20px 10px;border-radius:2px;max-width:340px;margin:0 auto}
.logo{width:122px;margin:0 auto 8px;background:var(--blue);color:#fff;text-align:center;padding:8px 10px;font-weight:700;font-size:18px}
.screen{background:#f2f2f2;border:3px solid #111;height:152px;padding:10px 12px;display:flex;flex-direction:column;justify-content:space-between}
.screen-title{font-size:15px;font-weight:700;text-align:center}
.screen-main{font-size:28px;font-weight:700;text-align:center;word-break:break-word;min-height:34px}
.screen-sub{font-size:14px;text-align:center;min-height:18px}
.screen-hint{font-size:13px;text-align:center;color:#444}
.func-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;margin:20px 12px 18px}
.func-btn,.digit,.ok,.cancel{height:56px;border-radius:8px;border:2px solid #d0d0d0;color:#fff;font-size:18px;cursor:pointer}
.func-btn{background:#969696;color:#eaeaea;border-color:#727272;height:40px;font-size:20px}
.mid-hole{background:#fff;color:#000;border:3px solid #111;font-size:14px;font-weight:700;line-height:1.1}
.keys{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px}
.digit{background:#5a5a5a;font-size:42px}
.cancel{background:var(--cancel);font-size:26px}
.ok{background:var(--ok);font-size:26px}
.rightcol{display:grid;gap:18px}
.card{background:#fff;border:1px solid #d7dfe7;border-radius:18px;padding:18px}
.section-title{font-size:22px;font-weight:700;margin-bottom:8px}
.muted{color:#5f6872}
.inline{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.input{width:100%;padding:12px 14px;border:1px solid #cfd8e3;border-radius:12px;font-size:18px}
.status{font-size:15px;font-weight:700;min-height:20px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
.kv{background:#f8fafc;border:1px solid #e2e8ef;border-radius:14px;padding:12px}
.kv .k{font-size:12px;text-transform:uppercase;color:#6b7280;margin-bottom:6px}
.kv .v{font-size:18px;font-weight:700;word-break:break-word}
@media(max-width:980px){.layout{grid-template-columns:1fr}.terminal{max-width:360px}}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <div>
      <h1 style="margin:0 0 6px 0">CT-100 panel operatorski</h1>
      <div class="muted">Sterowanie urzadzeniem z webservera, wpisywanie kodu i obsluga wyjsc.</div>
    </div>
    <div class="badges">
      <div class="badge"><strong>Rola:</strong> %CURRENT_ROLE%</div>
      <div class="badge"><strong>Uzytkownik:</strong> %CURRENT_USER%</div>
      <div class="badge"><strong>Urzadzenie:</strong> %DEVICE_NAME%</div>
      <a class="btn btn-gray" href="/logs">Logi</a>
      <a class="btn btn-gray" href="/firmware">Firmware</a>
      <a class="btn btn-red" href="/logout">Wyloguj</a>
    </div>
  </div>

  <div class="layout">
    <section>
      <div class="terminal">
        <div class="logo">TAMTRON</div>
        <div class="screen">
          <div class="screen-title" id="deviceScreenTitle">Terminal CT-100</div>
          <div class="screen-main" id="deviceScreenMain">---</div>
          <div class="screen-sub" id="deviceScreenSub">Gotowe</div>
          <div class="screen-hint" id="deviceScreenHint">Wpisz kod lub uzyj klawiszy.</div>
        </div>

        <div class="func-row">
          <button class="func-btn" type="button" onclick="sendVirtualKey('F1')">F1</button>
          <button class="func-btn mid-hole" type="button" onclick="sendVirtualKey('MID')">OTWOR<br>20x10</button>
          <button class="func-btn" type="button" onclick="sendVirtualKey('F2')">F2</button>
        </div>

        <div class="keys">
          <button class="digit" type="button" onclick="appendDigit('1')">1</button>
          <button class="digit" type="button" onclick="appendDigit('2')">2</button>
          <button class="digit" type="button" onclick="appendDigit('3')">3</button>
          <button class="digit" type="button" onclick="appendDigit('4')">4</button>
          <button class="digit" type="button" onclick="appendDigit('5')">5</button>
          <button class="digit" type="button" onclick="appendDigit('6')">6</button>
          <button class="digit" type="button" onclick="appendDigit('7')">7</button>
          <button class="digit" type="button" onclick="appendDigit('8')">8</button>
          <button class="digit" type="button" onclick="appendDigit('9')">9</button>
          <button class="cancel" type="button" onclick="clearCode()">X</button>
          <button class="digit" type="button" onclick="appendDigit('0')">0</button>
          <button class="ok" type="button" onclick="submitCode()">OK</button>
        </div>
      </div>
    </section>

    <section class="rightcol">
      <div class="card">
        <div class="section-title">Kod z webservera</div>
        <div class="muted">Kod mozesz wpisac klawiatura ekranowa albo recznie w polu ponizej.</div>
        <div class="inline" style="margin-top:14px">
          <input id="manualCode" class="input" placeholder="Wpisz kod produktu / operatora">
        </div>
        <div class="inline" style="margin-top:14px">
          <button class="btn btn-blue" type="button" onclick="submitManualCode()">Wyslij kod</button>
          <button class="btn btn-gray" type="button" onclick="clearCode()">Wyczysc</button>
        </div>
        <div class="status" id="codeStatus" style="margin-top:12px">Gotowe.</div>
      </div>

      <div class="card">
        <div class="section-title">Sterowanie wyjsciami</div>
        <div class="inline" style="margin-top:10px">
          <button class="btn btn-green" type="button" onclick="fireOutput('/api/output/out1/on')">OUT1 ON</button>
          <button class="btn btn-red" type="button" onclick="fireOutput('/api/output/out1/off')">OUT1 OFF</button>
          <button class="btn btn-green" type="button" onclick="fireOutput('/api/output/out2/on')">OUT2 ON</button>
          <button class="btn btn-red" type="button" onclick="fireOutput('/api/output/out2/off')">OUT2 OFF</button>
          <button class="btn btn-orange" type="button" onclick="fireOutput('/api/output/buzzer?ms=120')">BUZZER</button>
        </div>
      </div>

      <div class="card">
        <div class="section-title">Stan urzadzenia</div>
        <div class="grid">
          <div class="kv"><div class="k">IP</div><div class="v" id="liveIp">-</div></div>
          <div class="kv"><div class="k">Ostatnia karta</div><div class="v" id="rfidLast">-</div></div>
          <div class="kv"><div class="k">Ostatni klawisz</div><div class="v" id="keyLast">-</div></div>
          <div class="kv"><div class="k">Ostatnia waga</div><div class="v" id="scaleLast">-</div></div>
          <div class="kv"><div class="k">OUT1</div><div class="v" id="out1State">-</div></div>
          <div class="kv"><div class="k">OUT2</div><div class="v" id="out2State">-</div></div>
        </div>
      </div>
    </section>
  </div>
</div>

<script>
(function(){
  let codeBuffer = "";
  const el = (id) => document.getElementById(id);

  function setText(id, value){
    const node = el(id);
    if(node) node.textContent = (value === undefined || value === null || value === "") ? "-" : value;
  }

  function syncDisplay(){
    setText("deviceScreenMain", codeBuffer || "---");
    setText("deviceScreenSub", codeBuffer ? "Kod gotowy do wyslania" : "Gotowe");
    if(el("manualCode")) el("manualCode").value = codeBuffer;
  }

  async function postPlain(url, payload){
    const res = await fetch(url, {
      method: "POST",
      headers: {"Content-Type":"text/plain;charset=utf-8"},
      body: payload
    });
    if(!res.ok) throw new Error("HTTP " + res.status);
    return res.json();
  }

  window.appendDigit = function(d){
    codeBuffer += d;
    syncDisplay();
    setText("codeStatus", "Budowanie kodu...");
  };

  window.clearCode = function(){
    codeBuffer = "";
    syncDisplay();
    setText("codeStatus", "Kod wyczyszczony.");
  };

  window.submitCode = async function(){
    if(!codeBuffer){
      setText("codeStatus", "Brak kodu do wyslania.");
      return;
    }
    try{
      await postPlain("/api/keypad/code", codeBuffer);
      setText("codeStatus", "Kod wyslany: " + codeBuffer);
      setText("deviceScreenHint", "Kod wyslany do urzadzenia.");
    }catch(err){
      setText("codeStatus", "Blad: " + err.message);
    }
  };

  window.submitManualCode = async function(){
    const value = (el("manualCode")?.value || "").trim();
    codeBuffer = value;
    syncDisplay();
    await window.submitCode();
  };

  window.sendVirtualKey = async function(key){
    try{
      await postPlain("/api/keypad/key", key);
      setText("codeStatus", "Wyslano klawisz: " + key);
      setText("deviceScreenTitle", "KLAWISZ");
      setText("deviceScreenMain", key);
      setText("deviceScreenSub", "Wyslano z webservera");
      setText("deviceScreenHint", "Mozesz dalej obslugiwac urzadzenie.");
    }catch(err){
      setText("codeStatus", "Blad: " + err.message);
    }
  };

  window.fireOutput = async function(url){
    try{
      const res = await fetch(url, {method:"POST"});
      if(!res.ok) throw new Error("HTTP " + res.status);
      await loadRuntime();
      setText("codeStatus", "Komenda wykonana.");
    } catch(err){
      setText("codeStatus", "Blad: " + err.message);
    }
  };

  async function loadRuntime(){
    try{
      const data = await fetch("/api/runtime", {cache:"no-store"}).then(r => r.json());
      setText("liveIp", data.ip || "-");
      setText("rfidLast", data.rfidLast || "-");
      setText("keyLast", data.keyLast || "-");
      setText("scaleLast", data.scaleTcpLast || "-");
      setText("out1State", data.out1 ? "ON" : "OFF");
      setText("out2State", data.out2 ? "ON" : "OFF");
    } catch(err){
      setText("codeStatus", "Blad runtime: " + err.message);
    }
  }

  syncDisplay();
  loadRuntime();
  setInterval(loadRuntime, 3000);
})();
</script>
</body>
</html>
)HTML";

    html.replace("%CURRENT_ROLE%", currentRoleName());
    html.replace("%CURRENT_USER%", jsonEscape(currentUserName()));
    html.replace("%DEVICE_NAME%", jsonEscape(cfg.network.deviceName));
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
    if (netMode == "dhcp" || netMode == "static") {
        cfg.network.mode = ConfigManager::networkModeFromString(netMode);
    }

    cfg.network.ip = ConfigManager::stringToIp(parseStringField(body, "ip", cfg.network.ip.toString()));
    cfg.network.gateway = ConfigManager::stringToIp(parseStringField(body, "gateway", cfg.network.gateway.toString()));
    cfg.network.subnet = ConfigManager::stringToIp(parseStringField(body, "subnet", cfg.network.subnet.toString()));
    cfg.network.dns1 = ConfigManager::stringToIp(parseStringField(body, "dns1", cfg.network.dns1.toString()));
    cfg.network.dns2 = ConfigManager::stringToIp(parseStringField(body, "dns2", cfg.network.dns2.toString()));

    const String tcpMode = parseStringField(body, "tcpMode", parseStringField(body, "mode", ""));
    if (tcpMode == "client" || tcpMode == "host" || tcpMode == "server") {
        cfg.tcp.mode = ConfigManager::tcpModeFromString(tcpMode);
    }

    cfg.tcp.serverIp = parseStringField(body, "serverIp", cfg.tcp.serverIp);
    cfg.tcp.serverPort = parseUInt16Field(body, "serverPort", cfg.tcp.serverPort);
    cfg.tcp.listenPort = parseUInt16Field(body, "listenPort", cfg.tcp.listenPort);

    cfg.scaleTcp.enabled = parseBoolField(body, "scaleEnabled", cfg.scaleTcp.enabled);

    const String scaleMode = parseStringField(body, "scaleMode", "");
    if (scaleMode == "client" || scaleMode == "host" || scaleMode == "server") {
        cfg.scaleTcp.mode = ConfigManager::tcpModeFromString(scaleMode);
    }

    cfg.scaleTcp.serverIp = parseStringField(body, "scaleServerIp", cfg.scaleTcp.serverIp);
    cfg.scaleTcp.serverPort = parseUInt16Field(body, "scaleServerPort", cfg.scaleTcp.serverPort);
    cfg.scaleTcp.listenPort = parseUInt16Field(body, "scaleListenPort", cfg.scaleTcp.listenPort);

    cfg.rfid.enabled = parseBoolField(body, "rfidEnabled", cfg.rfid.enabled);
    cfg.rfid.baudRate = parseUInt32Field(body, "baudRate", cfg.rfid.baudRate);

    const String rfidEnc = parseStringField(body, "encoding", "");
    if (rfidEnc == "hex" || rfidEnc == "dec" || rfidEnc == "raw" || rfidEnc == "scale_frame") {
        cfg.rfid.encoding = ConfigManager::rfidEncodingFromString(rfidEnc);
    }

    cfg.display.enabled = parseBoolField(body, "displayEnabled", cfg.display.enabled);
    cfg.display.contrast = static_cast<uint8_t>(parseUInt16Field(body, "contrast", cfg.display.contrast));

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
