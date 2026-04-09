#include "WebConfigServer.h"
#include <ETH.h>
#include <ctype.h>
#include <stdlib.h>

WebConfigServer::WebConfigServer(LogManager& logger) : logger_(logger), server_(80) {}

void WebConfigServer::begin(const DeviceConfig& config) {
    config_ = config;

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/save", HTTP_POST, [this]() { handleSave(); });
    server_.on("/logs", HTTP_GET, [this]() { handleLogs(); });
    server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/reboot", HTTP_POST, [this]() { handleReboot(); });

    server_.on("/api/device/info", HTTP_GET, [this]() { handleApiDeviceInfo(); });
    server_.on("/api/config", HTTP_GET, [this]() { handleApiConfigGet(); });
    server_.on("/api/config", HTTP_POST, [this]() { handleApiConfigPost(); });

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

DeviceConfig WebConfigServer::activeConfig() const {
    return configProvider_ ? configProvider_() : config_;
}

bool WebConfigServer::authenticate() {
    const DeviceConfig cfg = activeConfig();
    if (server_.authenticate(cfg.security.webUser.c_str(), cfg.security.webPassword.c_str())) {
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

void WebConfigServer::handleSave() {
    if (!authenticate()) return;

    DeviceConfig cfg = activeConfig();
    cfg.network.deviceName = server_.arg("deviceName");
    cfg.network.mode = ConfigManager::networkModeFromString(server_.arg("networkMode"));
    cfg.network.ip = ConfigManager::stringToIp(server_.arg("ip"));
    cfg.network.gateway = ConfigManager::stringToIp(server_.arg("gateway"));
    cfg.network.subnet = ConfigManager::stringToIp(server_.arg("subnet"));
    cfg.network.dns1 = ConfigManager::stringToIp(server_.arg("dns1"));
    cfg.network.dns2 = ConfigManager::stringToIp(server_.arg("dns2"));

    cfg.security.webUser = server_.arg("webUser");
    if (!server_.arg("webPassword").isEmpty()) cfg.security.webPassword = server_.arg("webPassword");
    if (!server_.arg("otaPassword").isEmpty()) cfg.security.otaPassword = server_.arg("otaPassword");

    cfg.rfid.enabled = server_.hasArg("rfidEnabled");
    cfg.rfid.baudRate = static_cast<uint32_t>(server_.arg("rfidBaud").toInt());
    cfg.rfid.encoding = ConfigManager::rfidEncodingFromString(server_.arg("rfidEncoding"));

    cfg.display.enabled = server_.hasArg("displayEnabled");
    cfg.display.contrast = static_cast<uint8_t>(server_.arg("displayContrast").toInt());

    cfg.keypad.enabled = server_.hasArg("keypadEnabled");
    cfg.keypad.pcf8574Address = static_cast<uint8_t>(strtoul(server_.arg("pcfAddr").c_str(), nullptr, 0));

    cfg.tcp.mode = ConfigManager::tcpModeFromString(server_.arg("tcpMode"));
    cfg.tcp.serverIp = server_.arg("tcpServerIp");
    cfg.tcp.serverPort = static_cast<uint16_t>(server_.arg("tcpServerPort").toInt());
    cfg.tcp.listenPort = static_cast<uint16_t>(server_.arg("tcpListenPort").toInt());

    cfg.scaleTcp.enabled = server_.hasArg("scaleEnabled");
    cfg.scaleTcp.mode = ConfigManager::tcpModeFromString(server_.arg("scaleMode"));
    cfg.scaleTcp.serverIp = server_.arg("scaleServerIp");
    cfg.scaleTcp.serverPort = static_cast<uint16_t>(server_.arg("scaleServerPort").toInt());
    cfg.scaleTcp.listenPort = static_cast<uint16_t>(server_.arg("scaleListenPort").toInt());

    cfg.discovery.enabled = server_.hasArg("discoveryEnabled");
    cfg.discovery.udpPort = static_cast<uint16_t>(server_.arg("discoveryPort").toInt());

    if (onSave_) onSave_(cfg);
    logger_.warn("Configuration saved from web panel");

    server_.send(
        200,
        "text/html; charset=utf-8",
        "<h3>Zapisano konfigurację</h3><p>Urządzenie zastosuje zmiany po restarcie.</p><p><a href='/'>Powrót</a></p>"
    );
}

void WebConfigServer::handleLogs() {
    if (!authenticate()) return;
    server_.send(200, "text/html; charset=utf-8", "<h3>Logi</h3><pre>" + logger_.toHtml() + "</pre><p><a href='/'>Powrót</a></p>");
}

void WebConfigServer::handleStatus() {
    if (!authenticate()) return;
    const String payload = statusProvider_ ? statusProvider_() : String("Brak danych");
    server_.send(200, "text/plain; charset=utf-8", payload);
}

void WebConfigServer::handleReboot() {
    if (!authenticate()) return;
    server_.send(200, "text/html; charset=utf-8", "<h3>Restart</h3><p>Urządzenie uruchamia się ponownie.</p>");
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

void WebConfigServer::handleApiConfigPost() {
    if (!authenticate()) return;
    DeviceConfig cfg = activeConfig();
    applyConfigFromJson(cfg, server_.arg("plain"));
    if (onSave_) onSave_(cfg);
    logger_.warn("Configuration saved from REST API");
    server_.send(200, "application/json; charset=utf-8", "{\"ok\":true,\"restartRequired\":true}");
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
            default: out += c; break;
        }
    }
    return out;
}

String WebConfigServer::buildDeviceInfoJson(const DeviceConfig& cfg) const {
    String out;
    out.reserve(384);
    out += "{";
    out += "\"deviceId\":\"" + jsonEscape(String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX)) + "\",";
    out += "\"deviceName\":\"" + jsonEscape(cfg.network.deviceName) + "\",";
    out += "\"fw\":\"0.1.0\",";
    out += "\"ip\":\"" + ETH.localIP().toString() + "\",";
    out += "\"mac\":\"" + ETH.macAddress() + "\",";
    out += "\"tcpListenPort\":" + String(cfg.tcp.listenPort) + ",";
    out += "\"scaleListenPort\":" + String(cfg.scaleTcp.listenPort) + ",";
    out += "\"discoveryPort\":" + String(cfg.discovery.udpPort);
    out += "}";
    return out;
}

String WebConfigServer::buildConfigJson(const DeviceConfig& cfg) const {
    String out;
    out.reserve(1400);
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
    out += "}";

    out += "}";
    return out;
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

void WebConfigServer::applyConfigFromJson(DeviceConfig& cfg, const String& body) const {
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

    cfg.rfid.enabled = parseBoolField(body, "rfidEnabled", parseBoolField(body, "enabled", cfg.rfid.enabled));
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
}

String WebConfigServer::buildPage(const DeviceConfig& cfg) const {
    String html;
    html.reserve(9500);

    html += F("<!doctype html><html lang='pl'><head><meta charset='utf-8'>");
    html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    html += F("<title>CT-100</title>");
    html += F("<style>body{font-family:Arial,sans-serif;max-width:980px;margin:20px auto;padding:0 12px;}fieldset{margin:14px 0;padding:12px;}label{display:block;margin:8px 0 4px;}input,select{width:100%;padding:8px;box-sizing:border-box;}button{padding:10px 14px;margin-right:10px;}small{color:#555}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.checkbox{display:flex;align-items:center;gap:8px;margin:8px 0} @media(max-width:700px){.row{grid-template-columns:1fr;}}</style></head><body>");
    html += F("<h2>CT-100 - panel konfiguracji</h2>");
    html += F("<p><a href='/logs'>Logi</a> | <a href='/status'>Status TXT</a> | <a href='/api/device/info'>API info</a> | <a href='/api/config'>API config</a></p>");
    html += F("<form method='post' action='/save'>");

    html += F("<fieldset><legend>Sieć</legend>");
    html += F("<label>Nazwa urządzenia</label><input name='deviceName' value='");
    html += cfg.network.deviceName;
    html += F("'>");

    html += F("<label>Tryb sieci</label><select name='networkMode'>");
    html += F("<option value='dhcp'");
    if (cfg.network.mode == NetworkMode::DHCP) html += F(" selected");
    html += F(">DHCP</option><option value='static'");
    if (cfg.network.mode == NetworkMode::STATIC) html += F(" selected");
    html += F(">Static IP</option></select>");

    html += F("<div class='row'>");
    html += F("<div><label>IP</label><input name='ip' value='"); html += cfg.network.ip.toString(); html += F("'></div>");
    html += F("<div><label>Gateway</label><input name='gateway' value='"); html += cfg.network.gateway.toString(); html += F("'></div>");
    html += F("<div><label>Maska</label><input name='subnet' value='"); html += cfg.network.subnet.toString(); html += F("'></div>");
    html += F("<div><label>DNS 1</label><input name='dns1' value='"); html += cfg.network.dns1.toString(); html += F("'></div>");
    html += F("<div><label>DNS 2</label><input name='dns2' value='"); html += cfg.network.dns2.toString(); html += F("'></div>");
    html += F("</div></fieldset>");

    html += F("<fieldset><legend>Bezpieczeństwo</legend>");
    html += F("<label>Użytkownik WWW</label><input name='webUser' value='"); html += cfg.security.webUser; html += F("'>");
    html += F("<label>Nowe hasło WWW</label><input name='webPassword' type='password' value=''>");
    html += F("<label>Nowe hasło OTA</label><input name='otaPassword' type='password' value=''>");
    html += F("</fieldset>");

    html += F("<fieldset><legend>RFID</legend>");
    html += F("<label class='checkbox'><input type='checkbox' name='rfidEnabled'");
    if (cfg.rfid.enabled) html += F(" checked");
    html += F(">Włącz RFID</label>");
    html += F("<label>Baud rate</label><input name='rfidBaud' value='"); html += String(cfg.rfid.baudRate); html += F("'>");
    html += F("<label>Kodowanie</label><select name='rfidEncoding'>");
    html += F("<option value='hex'"); if (cfg.rfid.encoding == RfidEncoding::HEX_MODE) html += F(" selected"); html += F(">HEX</option>");
    html += F("<option value='dec'"); if (cfg.rfid.encoding == RfidEncoding::DEC_MODE) html += F(" selected"); html += F(">DEC</option>");
    html += F("<option value='raw'"); if (cfg.rfid.encoding == RfidEncoding::RAW_MODE) html += F(" selected"); html += F(">RAW</option>");
    html += F("<option value='scale_frame'"); if (cfg.rfid.encoding == RfidEncoding::SCALE_FRAME_MODE) html += F(" selected"); html += F(">SCALE FRAME</option>");
    html += F("</select></fieldset>");

    html += F("<fieldset><legend>Wyświetlacz i klawiatura</legend>");
    html += F("<label class='checkbox'><input type='checkbox' name='displayEnabled'");
    if (cfg.display.enabled) html += F(" checked");
    html += F(">Włącz LCD</label>");
    html += F("<label>Kontrast LCD</label><input name='displayContrast' value='"); html += String(cfg.display.contrast); html += F("'>");
    html += F("<label class='checkbox'><input type='checkbox' name='keypadEnabled'");
    if (cfg.keypad.enabled) html += F(" checked");
    html += F(">Włącz klawiaturę PCF8574</label>");
    html += F("<label>Adres PCF8574</label><input name='pcfAddr' value='"); html += String(cfg.keypad.pcf8574Address); html += F("'>");
    html += F("<small>Mapowanie PCF8574: P0-P3 = wiersze, P4-P7 = kolumny.</small>");
    html += F("</fieldset>");

    html += F("<fieldset><legend>TCP komendy / status PC</legend>");
    html += F("<label>Tryb</label><select name='tcpMode'>");
    html += F("<option value='client'"); if (cfg.tcp.mode == TcpMode::CLIENT) html += F(" selected"); html += F(">Client</option>");
    html += F("<option value='host'"); if (cfg.tcp.mode == TcpMode::HOST) html += F(" selected"); html += F(">Host</option>");
    html += F("<option value='server'"); if (cfg.tcp.mode == TcpMode::SERVER) html += F(" selected"); html += F(">Server</option>");
    html += F("</select>");
    html += F("<label>IP serwera TCP</label><input name='tcpServerIp' value='"); html += cfg.tcp.serverIp; html += F("'>");
    html += F("<label>Port serwera TCP</label><input name='tcpServerPort' value='"); html += String(cfg.tcp.serverPort); html += F("'>");
    html += F("<label>Port nasłuchu</label><input name='tcpListenPort' value='"); html += String(cfg.tcp.listenPort); html += F("'>");
    html += F("<small>Obsługa komend: STATUS:tekst oraz LCD:tekst</small>");
    html += F("</fieldset>");

    html += F("<fieldset><legend>TCP waga ASCII</legend>");
    html += F("<label class='checkbox'><input type='checkbox' name='scaleEnabled'");
    if (cfg.scaleTcp.enabled) html += F(" checked");
    html += F(">Włącz połączenie z wagą</label>");
    html += F("<label>Tryb</label><select name='scaleMode'>");
    html += F("<option value='client'"); if (cfg.scaleTcp.mode == TcpMode::CLIENT) html += F(" selected"); html += F(">Client</option>");
    html += F("<option value='host'"); if (cfg.scaleTcp.mode == TcpMode::HOST) html += F(" selected"); html += F(">Host</option>");
    html += F("<option value='server'"); if (cfg.scaleTcp.mode == TcpMode::SERVER) html += F(" selected"); html += F(">Server</option>");
    html += F("</select>");
    html += F("<label>IP serwera wagi</label><input name='scaleServerIp' value='"); html += cfg.scaleTcp.serverIp; html += F("'>");
    html += F("<label>Port serwera wagi</label><input name='scaleServerPort' value='"); html += String(cfg.scaleTcp.serverPort); html += F("'>");
    html += F("<label>Port nasłuchu wagi</label><input name='scaleListenPort' value='"); html += String(cfg.scaleTcp.listenPort); html += F("'>");
    html += F("<small>Każda odebrana linia ASCII jest traktowana jako aktualna waga.</small>");
    html += F("</fieldset>");

    html += F("<fieldset><legend>Autodiscovery</legend>");
    html += F("<label class='checkbox'><input type='checkbox' name='discoveryEnabled'");
    if (cfg.discovery.enabled) html += F(" checked");
    html += F(">Włącz wykrywanie UDP</label>");
    html += F("<label>Port UDP discovery</label><input name='discoveryPort' value='"); html += String(cfg.discovery.udpPort); html += F("'>");
    html += F("</fieldset>");

    html += F("<p><button type='submit'>Zapisz konfigurację</button></p>");
    html += F("</form>");
    html += F("<form method='post' action='/reboot'><button type='submit'>Restart urządzenia</button></form>");
    html += F("</body></html>");
    return html;
}