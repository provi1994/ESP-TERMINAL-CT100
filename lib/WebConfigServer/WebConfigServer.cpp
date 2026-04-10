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

    server_.on("/api/device/info", HTTP_GET, [this]() { handleApiDeviceInfo(); });
    server_.on("/api/config", HTTP_GET, [this]() { handleApiConfigGet(); });
    server_.on("/api/config", HTTP_POST, [this]() { handleApiConfigPost(); });
    server_.on("/api/runtime", HTTP_GET, [this]() { handleApiRuntimeGet(); });

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
                ok
                    ? "<h3>Firmware upload OK</h3><p>Urządzenie uruchomi się ponownie.</p><p><a href='/'>Powrót</a></p>"
                    : "<h3>Firmware upload błąd</h3><p>Sprawdź logi urządzenia i spróbuj ponownie.</p><p><a href='/firmware'>Powrót</a></p>"
            );
            if (ok) {
                delay(800);
                if (onReboot_) onReboot_();
            }
        },
        [this]() { handleFirmwareUpload(); }
    );

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

    if (server_.authenticate(cfg.security.adminUser.c_str(), cfg.security.adminPassword.c_str())) {
        return true;
    }
    if (server_.authenticate(cfg.security.serviceUser.c_str(), cfg.security.servicePassword.c_str())) {
        return true;
    }

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
    html.reserve(1024 + logger_.toHtml().length());
    html += "<!doctype html><html lang='pl'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>CT-100 logi</title>";
    html += "<style>body{font-family:Arial,sans-serif;max-width:1100px;margin:24px auto;padding:0 16px;background:#eef2f5;color:#13202b}"
            "a{color:#0077C0}pre{white-space:pre-wrap;background:#0f172a;color:#dbe4ee;padding:16px;border-radius:16px;overflow:auto}"
            ".top{display:flex;gap:12px;flex-wrap:wrap;align-items:center;margin-bottom:16px}.btn{background:#0077C0;color:#fff;padding:10px 14px;border-radius:10px;text-decoration:none}</style>";
    html += "</head><body>";
    html += "<div class='top'><a class='btn' href='/'>Panel</a><a class='btn' href='/status'>Status TXT</a></div>";
    html += "<h1>Logi i diagnostyka</h1>";
    html += "<pre>";
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
    server_.send(200, "text/html; charset=utf-8",
                 "<h3>Restart</h3><p>Urządzenie uruchamia się ponownie.</p><p><a href='/'>Powrót</a></p>");
    delay(300);
    if (onReboot_) onReboot_();
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

    server_.send(200, "application/json; charset=utf-8",
                 String("{\"ok\":true,\"restartRequired\":true,\"securityEditable\":") +
                     (allowSecurity ? "true" : "false") + "}");
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
    html += "<div class='sub'>Rola: <strong>" + currentRoleName() + "</strong> | Użytkownik: <strong>" +
            jsonEscape(currentUserName()) + "</strong></div></section>";
    html += "<section><h2>Upload pliku .bin</h2>";
    html += "<div class='sub'>Serwisant może wgrać nowe oprogramowanie bez dostępu do zmiany haseł. Po poprawnym uploadzie urządzenie zrestartuje się automatycznie.</div>";
    html += "<form method='POST' action='/firmware/upload' enctype='multipart/form-data' style='margin-top:16px'>";
    html += "<p><input type='file' name='firmware' accept='.bin' required></p>";
    html += "<p><button class='btn' type='submit'>Wgraj firmware</button></p>";
    html += "</form></section><p><a href='/'>Powrót do panelu</a></p></body></html>";

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
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
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
<title>CT-100 panel serwisowy</title>
<style>
:root{--bg:#eef2f5;--card:#ffffff;--line:#d9e0e6;--text:#13202b;--muted:#5b6570;--blue:#0077C0;--dark:#00263E;--orange:#F18A00;--gray:#4B4D4F}
*{box-sizing:border-box}html,body{margin:0;padding:0}body{font-family:Arial,sans-serif;background:var(--bg);color:var(--text)}
.app{display:grid;grid-template-columns:260px 1fr;min-height:100vh}.sidebar{background:var(--dark);color:#fff;padding:20px;position:sticky;top:0;height:100vh}
.brand{font-size:22px;font-weight:700;margin-bottom:6px}.brand-sub{font-size:13px;line-height:1.45;color:#d7e5ef;margin-bottom:20px}
.nav a{display:block;color:#fff;text-decoration:none;padding:12px 14px;border-radius:12px;margin-bottom:6px;background:rgba(255,255,255,0.06)}
.nav a:hover{background:rgba(255,255,255,0.12)}.rolebox{margin-top:20px;padding:14px;border:1px solid rgba(255,255,255,0.12);border-radius:14px;background:rgba(255,255,255,0.06);font-size:13px;line-height:1.5}
.main{padding:20px}.section{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:18px;margin-bottom:16px}h1,h2,h3{margin:0 0 12px}.sub{color:var(--muted);line-height:1.45}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.grid3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}.card{border:1px solid #e5eaef;border-radius:14px;padding:14px;background:#fafbfc}
.label{font-size:12px;text-transform:uppercase;letter-spacing:.05em;color:#6b7280;margin-bottom:6px}.value{font-weight:700;font-size:16px;word-break:break-word}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.mt{margin-top:12px}
input,select{width:100%;padding:11px;border:1px solid #cfd8e3;border-radius:10px;background:#fff}.check{display:flex;gap:8px;align-items:center;font-size:14px}.check input{width:auto}
.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}button,.btn{border:0;border-radius:10px;padding:12px 16px;cursor:pointer;font-weight:700;text-decoration:none;display:inline-block}
.btn-blue{background:var(--blue);color:#fff}.btn-gray{background:var(--gray);color:#fff}.btn-orange{background:var(--orange);color:#fff}.note{font-size:13px;color:var(--muted)}.hidden{display:none!important}
@media(max-width:960px){.app{grid-template-columns:1fr}.sidebar{position:relative;height:auto}.grid,.grid3,.row{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="app">
  <aside class="sidebar">
    <div class="brand">CT-100</div>
    <div class="brand-sub">Panel serwisowy v3. Najpierw integracja, stabilność i diagnostyka. Bez zbędnych live podglądów.</div>
    <nav class="nav">
      <a href="#start">Start</a><a href="#network">Sieć</a><a href="#integration">Integracja</a><a href="#io">RFID i wejścia</a><a href="#frames">Ostatnie ramki</a><a href="#logs">Logi i diagnostyka</a><a href="#firmware">Firmware / OTA</a><a href="#admin" id="navAdmin">Administracja</a>
    </nav>
    <div class="rolebox"><div><strong>Rola:</strong> %CURRENT_ROLE%</div><div><strong>Użytkownik:</strong> %CURRENT_USER%</div><div style="margin-top:8px">service: montaż, IP, porty, testy, logi, OTA</div><div>admin: dodatkowo hasła i ustawienia krytyczne</div></div>
  </aside>
  <main class="main">
    <section class="section" id="start"><h1>Start</h1><div class="sub">To jest panel dla serwisanta i integratora. Najpierw podstawowy stan urządzenia, potem konfiguracja i diagnostyka.</div><div class="grid3 mt"><div class="card"><div class="label">Urządzenie</div><div class="value" id="deviceNameHead">%DEVICE_NAME%</div></div><div class="card"><div class="label">Rola</div><div class="value" id="currentRole">%CURRENT_ROLE%</div></div><div class="card"><div class="label">Wersja</div><div class="value" id="fwVersion">0.1.0</div></div></div><div class="grid3 mt"><div class="card"><div class="label">IP</div><div class="value" id="liveIp">-</div></div><div class="card"><div class="label">TCP komendy</div><div class="value" id="cmdState">-</div></div><div class="card"><div class="label">TCP waga</div><div class="value" id="scaleState">-</div></div></div><div class="grid3 mt"><div class="card"><div class="label">RFID</div><div class="value" id="rfidState">-</div></div><div class="card"><div class="label">Klawiatura</div><div class="value" id="keypadState">-</div></div><div class="card"><div class="label">Discovery</div><div class="value" id="discState">-</div></div></div><div class="actions"><a class="btn btn-blue" href="#network">Konfiguracja sieci</a><a class="btn btn-blue" href="#integration">Integracja TCP</a><a class="btn btn-gray" href="/logs">Pokaż logi</a><a class="btn btn-blue" href="/firmware">OTA</a></div></section>
    <section class="section" id="network"><h2>Sieć</h2><div class="sub">Ustawienia podstawowe do uruchomienia terminala w sieci klienta.</div><div class="row mt"><div><label class="label">Nazwa urządzenia</label><input id="deviceName" type="text"></div><div><label class="label">Tryb sieci</label><select id="networkMode"><option value="dhcp">DHCP</option><option value="static">Static</option></select></div></div><div class="row mt"><div><label class="label">IP</label><input id="ip" type="text"></div><div><label class="label">Brama</label><input id="gateway" type="text"></div></div><div class="row mt"><div><label class="label">Maska</label><input id="subnet" type="text"></div><div><label class="label">DNS 1</label><input id="dns1" type="text"></div></div><div class="row mt"><div><label class="label">DNS 2</label><input id="dns2" type="text"></div><div class="card"><div class="label">Aktualny IP</div><div class="value" id="networkIpNow">-</div><div class="label" style="margin-top:12px">MAC</div><div class="value" id="macNow">-</div></div></div></section>
    <section class="section" id="integration"><h2>Integracja</h2><div class="sub">Ustawienia komunikacji z programem wagowym i ewentualnym kanałem wagi ASCII.</div><div class="grid"><div class="card"><h3>TCP komendy</h3><div class="row"><div><label class="label">Tryb</label><select id="tcpMode"><option value="client">client</option><option value="host">host</option><option value="server">server</option></select></div><div><label class="label">Port serwera</label><input id="tcpServerPort" type="number"></div></div><div class="row mt"><div><label class="label">IP serwera</label><input id="tcpServerIp" type="text"></div><div><label class="label">Port nasłuchu</label><input id="tcpListenPort" type="number"></div></div></div><div class="card"><h3>TCP waga ASCII</h3><label class="check"><input id="scaleEnabled" type="checkbox"> Włącz kanał wagi</label><div class="row mt"><div><label class="label">Tryb</label><select id="scaleMode"><option value="client">client</option><option value="host">host</option><option value="server">server</option></select></div><div><label class="label">Port serwera</label><input id="scaleServerPort" type="number"></div></div><div class="row mt"><div><label class="label">IP serwera</label><input id="scaleServerIp" type="text"></div><div><label class="label">Port nasłuchu</label><input id="scaleListenPort" type="number"></div></div></div></div></section>
    <section class="section" id="io"><h2>RFID i wejścia</h2><div class="sub">Skonfiguruj lokalne wejścia i sprawdź, jakie ramki generuje terminal.</div><div class="grid"><div class="card"><h3>RFID</h3><label class="check"><input id="rfidEnabled" type="checkbox"> Włącz RFID</label><div class="row mt"><div><label class="label">Baud rate</label><input id="rfidBaud" type="number"></div><div><label class="label">Kodowanie</label><select id="rfidEncoding"><option value="hex">HEX</option><option value="dec">DEC</option><option value="raw">RAW</option><option value="scale_frame">CT100 frame</option></select></div></div><div class="mt note">Ostatni UID: <strong id="lastRfidValue">-</strong></div></div><div class="card"><h3>Klawiatura i discovery</h3><label class="check"><input id="keypadEnabled" type="checkbox"> Włącz klawiaturę</label><div class="row mt"><div><label class="label">Adres PCF8574</label><input id="pcf8574Address" type="number"></div><div><label class="label">Ostatni klawisz</label><input id="lastKeyValue" type="text" disabled></div></div><label class="check mt"><input id="discoveryEnabled" type="checkbox"> Włącz discovery UDP</label><div class="mt"><label class="label">Port discovery</label><input id="udpPort" type="number"></div></div></div></section>
    <section class="section" id="frames"><h2>Ostatnie ramki</h2><div class="sub">To jest główny podgląd diagnostyczny. Zamiast ciężkiego live dashboardu pokazujemy ostatnią komunikację i ślad transmisji.</div><div class="grid mt"><div class="card"><div class="label">Ostatnia ramka wychodząca</div><div class="value" id="lastOutbound">-</div></div><div class="card"><div class="label">Ostatnia ramka przychodząca</div><div class="value" id="lastInbound">-</div></div></div><div class="grid mt"><div class="card"><div class="label">Ostatnia wiadomość TCP komendy</div><div class="value" id="cmdLastMsg">-</div></div><div class="card"><div class="label">Ostatnia wiadomość TCP waga</div><div class="value" id="scaleLastMsg">-</div></div></div><div class="actions"><a class="btn btn-gray" href="/status" target="_blank">Status TXT</a><a class="btn btn-gray" href="/logs" target="_blank">Logi</a></div></section>
    <section class="section" id="logs"><h2>Logi i diagnostyka</h2><div class="sub">Logi tylko na żądanie, bez agresywnego odświeżania. To jest lżejsze i bezpieczniejsze dla ESP.</div><div class="actions"><a class="btn btn-gray" href="/logs" target="_blank">Otwórz logi</a><a class="btn btn-gray" href="/status" target="_blank">Otwórz status TXT</a></div></section>
    <section class="section" id="firmware"><h2>Firmware / OTA</h2><div class="sub">Aktualizacja firmware jest dostępna dla service i admin. Zmiana haseł OTA pozostaje tylko dla admina.</div><div class="actions"><a class="btn btn-blue" href="/firmware">Przejdź do uploadu .bin</a></div></section>
    <section class="section" id="admin"><h2>Administracja</h2><div class="sub">Ta sekcja jest wyłącznie dla admina. Service nie ma tu dostępu do zmiany haseł.</div><div id="adminVisibleBlock" class="hidden"><div class="row mt"><div><label class="label">Admin user</label><input id="adminUser" type="text"></div><div><label class="label">Admin hasło</label><input id="adminPassword" type="password" placeholder="Podaj nowe lub zostaw bez zmian"></div></div><div class="row mt"><div><label class="label">Service user</label><input id="serviceUser" type="text"></div><div><label class="label">Service hasło</label><input id="servicePassword" type="password" placeholder="Podaj nowe lub zostaw bez zmian"></div></div><div class="row mt"><div><label class="label">Hasło OTA</label><input id="otaPassword" type="password" placeholder="Podaj nowe lub zostaw bez zmian"></div><div class="card"><div class="label">Ostrzeżenie</div><div class="note">Hasła zapisują się tylko po kliknięciu Zapisz konfigurację. Zmiany wymagają restartu urządzenia.</div></div></div></div><div id="adminHiddenBlock" class="card"><div class="label">Brak dostępu</div><div class="note">Zaloguj się jako admin, aby zarządzać hasłami i ustawieniami krytycznymi.</div></div></section>
    <section class="section"><h2>Akcje serwisowe</h2><div class="actions"><button class="btn btn-blue" id="saveBtn" type="button">Zapisz konfigurację</button><button class="btn btn-orange" id="saveRestartBtn" type="button">Zapisz i restart</button><button class="btn btn-gray" id="reloadBtn" type="button">Odśwież dane</button><form method="post" action="/reboot" style="margin:0"><button class="btn btn-orange" type="submit">Restart teraz</button></form></div><div class="mt note" id="saveMsg">Zmiany konfiguracji zapisują się przez API i wymagają restartu urządzenia.</div></section>
  </main>
</div>
<script>
(function(){
  const role = "%CURRENT_ROLE%";
  let inflightRuntime = false, inflightConfig = false, restartAfterSave = false;
  const el = (id) => document.getElementById(id);
  function setText(id, value){ const node = el(id); if(node) node.textContent = (value === undefined || value === null || value === "") ? "-" : value; }
  function setValue(id, value){ const node = el(id); if(!node) return; if(node.type === "checkbox") node.checked = !!value; else node.value = value === undefined || value === null ? "" : value; }
  function disableButtons(disabled){ ["saveBtn","saveRestartBtn","reloadBtn"].forEach(id => { const b = el(id); if(b) b.disabled = disabled; }); }
  async function fetchJson(url){ const res = await fetch(url, {cache:"no-store"}); if(!res.ok) throw new Error("HTTP " + res.status); return res.json(); }
  async function loadDeviceInfo(){ const data = await fetchJson("/api/device/info"); setText("deviceNameHead", data.deviceName); setText("fwVersion", data.fw); setText("liveIp", data.ip); setText("networkIpNow", data.ip); setText("macNow", data.mac); setText("currentRole", data.currentRole); }
  async function loadConfig(){ if(inflightConfig) return; inflightConfig = true; try{ const data = await fetchJson("/api/config"); setValue("deviceName", data.network.deviceName); setValue("networkMode", data.network.mode); setValue("ip", data.network.ip); setValue("gateway", data.network.gateway); setValue("subnet", data.network.subnet); setValue("dns1", data.network.dns1); setValue("dns2", data.network.dns2); setValue("tcpMode", data.tcp.mode); setValue("tcpServerIp", data.tcp.serverIp); setValue("tcpServerPort", data.tcp.serverPort); setValue("tcpListenPort", data.tcp.listenPort); setValue("scaleEnabled", data.scaleTcp.enabled); setValue("scaleMode", data.scaleTcp.mode); setValue("scaleServerIp", data.scaleTcp.serverIp); setValue("scaleServerPort", data.scaleTcp.serverPort); setValue("scaleListenPort", data.scaleTcp.listenPort); setValue("rfidEnabled", data.rfid.enabled); setValue("rfidBaud", data.rfid.baudRate); setValue("rfidEncoding", data.rfid.encoding); setValue("keypadEnabled", data.keypad.enabled); setValue("pcf8574Address", data.keypad.pcf8574Address); setValue("discoveryEnabled", data.discovery.enabled); setValue("udpPort", data.discovery.udpPort); if(data.permissions && data.permissions.canEditSecurity){ el("adminVisibleBlock").classList.remove("hidden"); el("adminHiddenBlock").classList.add("hidden"); el("navAdmin").classList.remove("hidden"); setValue("adminUser", data.security.adminUser || ""); setValue("serviceUser", data.security.serviceUser || ""); } else { el("adminVisibleBlock").classList.add("hidden"); el("adminHiddenBlock").classList.remove("hidden"); if(role !== "admin") el("navAdmin").classList.add("hidden"); setValue("serviceUser", data.security.serviceUser || ""); } } finally { inflightConfig = false; } }
  async function loadRuntime(){ if(inflightRuntime) return; inflightRuntime = true; try{ const data = await fetchJson("/api/runtime"); setText("cmdState", data.cmdTcpConnected ? "połączono" : "brak połączenia"); setText("scaleState", data.scaleTcpConnected ? "połączono" : "brak połączenia"); setText("rfidState", data.rfidEnabled ? "aktywne" : "wyłączone"); setText("keypadState", data.keypadEnabled ? (data.keypadDetected ? "aktywna" : "włączona, brak odpowiedzi") : "wyłączona"); setText("discState", data.discoveryEnabled ? "aktywne" : "wyłączone"); setText("lastRfidValue", data.rfidLast); setText("lastKeyValue", data.keyLast); setText("lastOutbound", data.lastOutbound); setText("lastInbound", data.lastInbound); setText("cmdLastMsg", data.cmdTcpLast); setText("scaleLastMsg", data.scaleTcpLast); if(data.ip) { setText("liveIp", data.ip); setText("networkIpNow", data.ip); } } finally { inflightRuntime = false; } }
  function collectPayload(){ const payload = { deviceName: el("deviceName").value, mode: el("networkMode").value, ip: el("ip").value, gateway: el("gateway").value, subnet: el("subnet").value, dns1: el("dns1").value, dns2: el("dns2").value, tcpMode: el("tcpMode").value, serverIp: el("tcpServerIp").value, serverPort: Number(el("tcpServerPort").value || 0), listenPort: Number(el("tcpListenPort").value || 0), scaleEnabled: el("scaleEnabled").checked, scaleMode: el("scaleMode").value, scaleServerIp: el("scaleServerIp").value, scaleServerPort: Number(el("scaleServerPort").value || 0), scaleListenPort: Number(el("scaleListenPort").value || 0), rfidEnabled: el("rfidEnabled").checked, baudRate: Number(el("rfidBaud").value || 0), encoding: el("rfidEncoding").value, keypadEnabled: el("keypadEnabled").checked, pcf8574Address: Number(el("pcf8574Address").value || 0), discoveryEnabled: el("discoveryEnabled").checked, udpPort: Number(el("udpPort").value || 0) }; if(role === "admin"){ payload.adminUser = el("adminUser").value; if(el("adminPassword").value) payload.adminPassword = el("adminPassword").value; payload.serviceUser = el("serviceUser").value; if(el("servicePassword").value) payload.servicePassword = el("servicePassword").value; if(el("otaPassword").value) payload.otaPassword = el("otaPassword").value; } return payload; }
  async function saveConfig(){ disableButtons(true); setText("saveMsg", "Zapisywanie konfiguracji..."); try{ const payload = collectPayload(); const res = await fetch("/api/config", { method:"POST", headers:{"Content-Type":"application/json"}, body: JSON.stringify(payload) }); if(!res.ok) throw new Error("HTTP " + res.status); const data = await res.json(); setText("saveMsg", data.restartRequired ? "Konfiguracja zapisana. Wymagany restart urządzenia." : "Konfiguracja zapisana."); if(restartAfterSave){ restartAfterSave = false; setTimeout(() => { const form = document.createElement("form"); form.method = "POST"; form.action = "/reboot"; document.body.appendChild(form); form.submit(); }, 300); } } catch(err){ setText("saveMsg", "Błąd zapisu: " + err.message); restartAfterSave = false; } finally { disableButtons(false); } }
  el("saveBtn").addEventListener("click", () => { restartAfterSave = false; saveConfig(); });
  el("saveRestartBtn").addEventListener("click", () => { restartAfterSave = true; saveConfig(); });
  el("reloadBtn").addEventListener("click", async () => { disableButtons(true); try{ await Promise.all([loadDeviceInfo(), loadConfig(), loadRuntime()]); setText("saveMsg", "Dane odświeżone."); } catch(err){ setText("saveMsg", "Błąd odświeżania: " + err.message); } finally { disableButtons(false); } });
  if(role !== "admin"){ el("navAdmin").classList.add("hidden"); }
  Promise.all([loadDeviceInfo(), loadConfig(), loadRuntime()]).catch(err => setText("saveMsg", "Błąd startu panelu: " + err.message));
  setInterval(loadRuntime, 5000);
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