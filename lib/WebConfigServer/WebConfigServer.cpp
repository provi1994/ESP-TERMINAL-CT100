#include "WebConfigServer.h"
#include <ETH.h>
#include <Update.h>
#include <base64.h>
#include <ctype.h>
#include <stdlib.h>

WebConfigServer::WebConfigServer(LogManager& logger) : logger_(logger), server_(80) {}

void WebConfigServer::begin(const DeviceConfig& config) {
    config_ = config;

    server_.collectHeaders("Authorization", 1);

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
            server_.send(200, "text/html; charset=utf-8",
                         "<h3>Firmware upload OK</h3><p>Urządzenie uruchomi się ponownie.</p>");
            delay(500);
            if (onReboot_) onReboot_();
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
    server_.send(200, "text/html; charset=utf-8",
                 "<h3>Logi</h3><pre>" + logger_.toHtml() + "</pre><p><a href='/'>Powrot</a></p>");
}

void WebConfigServer::handleStatus() {
    if (!authenticate()) return;
    const String payload = statusProvider_ ? statusProvider_() : String("Brak danych");
    server_.send(200, "text/plain; charset=utf-8", payload);
}

void WebConfigServer::handleReboot() {
    if (!authenticate()) return;
    server_.send(200, "text/html; charset=utf-8",
                 "<h3>Restart</h3><p>Urządzenie uruchamia się ponownie.</p>");
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
    html.reserve(1800);
    html += "<!doctype html><html lang='pl'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Firmware OTA</title>";
    html += "<style>body{font-family:Arial,sans-serif;max-width:760px;margin:24px auto;padding:0 16px}"
            "fieldset{padding:16px;margin:16px 0}button,input{padding:10px}small{color:#555}</style>";
    html += "</head><body>";
    html += "<h2>Aktualizacja firmware</h2>";
    html += "<p>Rola: <strong>" + currentRoleName() + "</strong> | Uzytkownik: <strong>" +
            jsonEscape(currentUserName()) + "</strong></p>";
    html += "<fieldset><legend>Upload pliku</legend>";
    html += "<form method='POST' action='/firmware/upload' enctype='multipart/form-data'>";
    html += "<p><input type='file' name='firmware' accept='.bin' required></p>";
    html += "<p><button type='submit'>Wgraj firmware</button></p>";
    html += "</form>";
    html += "<small>Po poprawnym uploadzie urządzenie zrestartuje się automatycznie.</small>";
    html += "</fieldset><p><a href='/'>Powrot</a></p></body></html>";

    server_.send(200, "text/html; charset=utf-8", html);
}

void WebConfigServer::handleFirmwareUpload() {
    if (!authenticate()) return;

    HTTPUpload& upload = server_.upload();

    if (upload.status == UPLOAD_FILE_START) {
        logger_.warn("Firmware upload start: " + upload.filename);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
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
    const bool admin = isAdmin();

    String html;
    html.reserve(12000);

    html += "<!doctype html><html lang='pl'><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>CT-100 panel serwisowy</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;background:#f3f5f7;color:#1b1f23;max-width:1180px;margin:20px auto;padding:0 14px}";
    html += "section{background:#fff;border:1px solid #d9e0e6;border-radius:16px;padding:16px;margin:14px 0}";
    html += "h1,h2,h3{margin-top:0}";
    html += ".sub{color:#5b6570;font-size:14px;line-height:1.45}";
    html += ".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}";
    html += ".grid3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}";
    html += ".card{border:1px solid #e5eaef;border-radius:14px;padding:12px;background:#fafbfc}";
    html += ".label{font-size:12px;text-transform:uppercase;color:#6b7280;margin-bottom:6px}";
    html += ".value{font-weight:700;font-size:16px}";
    html += "input,select{width:100%;padding:10px;border:1px solid #cfd8e3;border-radius:10px;box-sizing:border-box}";
    html += "button{padding:11px 14px;border:0;border-radius:10px;cursor:pointer}";
    html += ".btn{background:#0077C0;color:#fff}.btn2{background:#4B4D4F;color:#fff}.btn3{background:#F18A00;color:#fff}";
    html += ".row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.mt{margin-top:12px}";
    html += ".checkbox{display:flex;gap:8px;align-items:center}";
    html += ".checkbox input{width:auto}";
    html += "pre{white-space:pre-wrap;background:#0f172a;color:#dbe4ee;padding:12px;border-radius:12px;overflow:auto}";
    html += "@media(max-width:900px){.grid,.grid3,.row{grid-template-columns:1fr}}";
    html += "</style></head><body>";

    html += "<section><h1>CT-100 panel serwisowy</h1><div class='sub'>Rola: " + currentRoleName() +
            " | Uzytkownik: " + jsonEscape(currentUserName()) +
            " | service moze konfigurowac integracje, logi i OTA. Admin dodatkowo zmienia hasla.</div></section>";

    html += "<section><div class='grid3'>";
    html += "<div class='card'><div class='label'>Urzadzenie</div><div class='value'>" + jsonEscape(cfg.network.deviceName) + "</div></div>";
    html += "<div class='card'><div class='label'>IP</div><div class='value'>" + ETH.localIP().toString() + "</div></div>";
    html += "<div class='card'><div class='label'>Rola</div><div class='value'>" + currentRoleName() + "</div></div>";
    html += "</div></section>";

    html += "<section><h2>Konfiguracja montazowa</h2><div class='sub'>IP, porty, RFID, discovery, testy, logi i OTA.</div></section>";

    html += "<section><h2>API i serwis</h2>";
    html += "<p><a href='/logs'>Logi</a> | <a href='/status'>Status TXT</a> | <a href='/api/device/info'>API info</a> | <a href='/api/config'>API config</a> | <a href='/api/runtime'>API runtime</a> | <a href='/firmware'>Firmware / OTA</a></p>";
    html += "</section>";

    html += "<section><h2>Konfiguracja JSON</h2>";
    html += "<div class='sub'>Zapis konfiguracji przez REST API. Dla service pola bezpieczenstwa sa ignorowane.</div>";
    html += "<pre>" + buildConfigJson(cfg) + "</pre>";
    html += "</section>";

    html += "<section><h2>Uprawnienia</h2><div class='grid'>";
    html += "<div class='card'><div class='label'>service</div><div class='value'>IP, porty, TCP, RFID, logi, OTA, restart</div></div>";
    html += "<div class='card'><div class='label'>admin</div><div class='value'>Wszystko z service + hasla i ustawienia krytyczne</div></div>";
    html += "</div></section>";

    if (admin) {
        html += "<section><h2>Administracja</h2>";
        html += "<div class='sub'>Zmiana hasel i ustawien krytycznych tylko dla admina.</div>";
        html += "<div class='grid'>";
        html += "<div class='card'><div class='label'>Admin user</div><div class='value'>" + jsonEscape(cfg.security.adminUser) + "</div></div>";
        html += "<div class='card'><div class='label'>Service user</div><div class='value'>" + jsonEscape(cfg.security.serviceUser) + "</div></div>";
        html += "</div></section>";
    }

    html += "<section><form method='post' action='/reboot'><button class='btn3' type='submit'>Restart urzadzenia</button></form></section>";

    html += "</body></html>";
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