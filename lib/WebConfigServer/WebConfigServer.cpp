#include "WebConfigServer.h"

WebConfigServer::WebConfigServer(LogManager& logger) : logger_(logger), server_(80) {}

void WebConfigServer::begin(const DeviceConfig& config) {
  config_ = config;
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/logs", HTTP_GET, [this]() { handleLogs(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.begin();
  logger_.info("Web config server started on port 80");
}

void WebConfigServer::loop() { server_.handleClient(); }
void WebConfigServer::onSave(std::function<void(DeviceConfig)> callback) { onSave_ = callback; }
void WebConfigServer::onReboot(std::function<void()> callback) { onReboot_ = callback; }
void WebConfigServer::setConfigProvider(std::function<DeviceConfig()> provider) { configProvider_ = provider; }
void WebConfigServer::setStatusProvider(std::function<String()> provider) { statusProvider_ = provider; }

bool WebConfigServer::authenticate() {
  DeviceConfig cfg = configProvider_ ? configProvider_() : config_;
  if (server_.authenticate(cfg.security.webUser.c_str(), cfg.security.webPassword.c_str())) return true;
  server_.requestAuthentication(BASIC_AUTH, "CT-100", "Logowanie wymagane");
  return false;
}

void WebConfigServer::handleRoot() {
  if (!authenticate()) return;
  DeviceConfig cfg = configProvider_ ? configProvider_() : config_;
  server_.send(200, "text/html; charset=utf-8", buildPage(cfg));
}

void WebConfigServer::handleSave() {
  if (!authenticate()) return;
  DeviceConfig cfg = configProvider_ ? configProvider_() : config_;

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
  cfg.rfid.baudRate = server_.arg("rfidBaud").toInt();
  cfg.rfid.encoding = ConfigManager::rfidEncodingFromString(server_.arg("rfidEncoding"));

  cfg.tcp.mode = ConfigManager::tcpModeFromString(server_.arg("tcpMode"));
  cfg.tcp.serverIp = server_.arg("tcpServerIp");
  cfg.tcp.serverPort = static_cast<uint16_t>(server_.arg("tcpServerPort").toInt());
  cfg.tcp.listenPort = static_cast<uint16_t>(server_.arg("tcpListenPort").toInt());

  if (onSave_) onSave_(cfg);
  logger_.warn("Configuration saved from web panel");
  String msg = "<html><body><h3>Zapisano konfiguracje</h3><p>Urządzenie zastosuje zmiany po restarcie.</p><p><a href='/'>Powrót</a></p></body></html>";
  server_.send(200, "text/html; charset=utf-8", msg);
}

void WebConfigServer::handleLogs() {
  if (!authenticate()) return;
  server_.send(200, "text/html; charset=utf-8", "<html><body><h3>Logi</h3>" + logger_.toHtml() + "<p><a href='/'>Powrót</a></p></body></html>");
}

void WebConfigServer::handleStatus() {
  if (!authenticate()) return;
  String payload = statusProvider_ ? statusProvider_() : String("Brak danych");
  server_.send(200, "text/plain; charset=utf-8", payload);
}

String WebConfigServer::buildPage(const DeviceConfig& cfg) const {
  String html;
  html.reserve(7000);
  html += "<!doctype html><html><head><meta charset='utf-8'><title>CT-100</title>";
  html += "<style>body{font-family:Arial;margin:24px;background:#f4f6f8;color:#1d232a}fieldset{margin:0 0 18px;padding:16px;background:#fff;border:1px solid #d0d7de;border-radius:10px}label{display:block;margin:8px 0 4px}input,select{width:100%;padding:8px;border:1px solid #bbb;border-radius:6px}button{padding:10px 18px;border:0;border-radius:8px;background:#1f6feb;color:#fff;margin-top:12px}a{color:#1f6feb}</style></head><body>";
  html += "<h2>CT-100 - panel konfiguracji</h2>";
  html += "<p><a href='/logs'>Logi</a> | <a href='/status'>Status TXT</a></p>";
  html += "<form method='post' action='/save'>";

  html += "<fieldset><legend>Sieć</legend>";
  html += "<label>Nazwa urządzenia</label><input name='deviceName' value='" + cfg.network.deviceName + "'>";
  html += "<label>Tryb sieci</label><select name='networkMode'>";
  html += "<option value='dhcp'" + String(cfg.network.mode == NetworkMode::DHCP ? " selected" : "") + ">DHCP</option>";
  html += "<option value='static'" + String(cfg.network.mode == NetworkMode::STATIC ? " selected" : "") + ">Static IP</option></select>";
  html += "<label>IP</label><input name='ip' value='" + cfg.network.ip.toString() + "'>";
  html += "<label>Gateway</label><input name='gateway' value='" + cfg.network.gateway.toString() + "'>";
  html += "<label>Maska</label><input name='subnet' value='" + cfg.network.subnet.toString() + "'>";
  html += "<label>DNS 1</label><input name='dns1' value='" + cfg.network.dns1.toString() + "'>";
  html += "<label>DNS 2</label><input name='dns2' value='" + cfg.network.dns2.toString() + "'>";
  html += "</fieldset>";

  html += "<fieldset><legend>Bezpieczeństwo</legend>";
  html += "<label>Użytkownik WWW</label><input name='webUser' value='" + cfg.security.webUser + "'>";
  html += "<label>Nowe hasło WWW</label><input name='webPassword' type='password' value=''>";
  html += "<label>Nowe hasło OTA</label><input name='otaPassword' type='password' value=''>";
  html += "</fieldset>";

  html += "<fieldset><legend>RFID 125 kHz</legend>";
  html += "<label><input type='checkbox' name='rfidEnabled'" + String(cfg.rfid.enabled ? " checked" : "") + "> Włącz RFID</label>";
  html += "<label>Baud rate</label><input name='rfidBaud' value='" + String(cfg.rfid.baudRate) + "'>";
  html += "<label>Kodowanie</label><select name='rfidEncoding'>";
  html += "<option value='hex'" + String(cfg.rfid.encoding == RfidEncoding::HEX_MODE ? " selected" : "") + ">HEX</option>";
  html += "<option value='dec'" + String(cfg.rfid.encoding == RfidEncoding::DEC_MODE ? " selected" : "") + ">DEC</option>";
  html += "<option value='raw'" + String(cfg.rfid.encoding == RfidEncoding::RAW_MODE ? " selected" : "") + ">RAW</option></select>";
  html += "</fieldset>";

  html += "<fieldset><legend>TCP</legend>";
  html += "<label>Tryb</label><select name='tcpMode'>";
  html += "<option value='client'" + String(cfg.tcp.mode == TcpMode::CLIENT ? " selected" : "") + ">Client</option>";
  html += "<option value='host'" + String(cfg.tcp.mode == TcpMode::HOST ? " selected" : "") + ">Host</option>";
  html += "<option value='server'" + String(cfg.tcp.mode == TcpMode::SERVER ? " selected" : "") + ">Server</option></select>";
  html += "<label>IP serwera TCP</label><input name='tcpServerIp' value='" + cfg.tcp.serverIp + "'>";
  html += "<label>Port serwera TCP</label><input name='tcpServerPort' value='" + String(cfg.tcp.serverPort) + "'>";
  html += "<label>Port nasłuchu</label><input name='tcpListenPort' value='" + String(cfg.tcp.listenPort) + "'>";
  html += "</fieldset>";

  html += "<button type='submit'>Zapisz konfigurację</button></form></body></html>";
  return html;
}
