#include "WebConfigServer.h"

WebConfigServer::WebConfigServer(LogManager& logger) : logger_(logger), server_(80) {}

void WebConfigServer::begin(const DeviceConfig& config) {
  config_ = config;
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/logs", HTTP_GET, [this]() { handleLogs(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/reboot", HTTP_POST, [this]() { handleReboot(); });
  server_.begin();
  logger_.info("Web config server started on port 80");
}

void WebConfigServer::loop() { server_.handleClient(); }
void WebConfigServer::onSave(std::function<void(DeviceConfig)> callback) { onSave_ = callback; }
void WebConfigServer::onReboot(std::function<void()> callback) { onReboot_ = callback; }
void WebConfigServer::setConfigProvider(std::function<DeviceConfig()> provider) { configProvider_ = provider; }
void WebConfigServer::setStatusProvider(std::function<String()> provider) { statusProvider_ = provider; }

bool WebConfigServer::authenticate() {
  const DeviceConfig cfg = configProvider_ ? configProvider_() : config_;
  if (server_.authenticate(cfg.security.webUser.c_str(), cfg.security.webPassword.c_str())) return true;
  server_.requestAuthentication(BASIC_AUTH, "CT-100", "Logowanie wymagane");
  return false;
}

void WebConfigServer::handleRoot() {
  if (!authenticate()) return;
  const DeviceConfig cfg = configProvider_ ? configProvider_() : config_;
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

  if (onSave_) onSave_(cfg);
  logger_.warn("Configuration saved from web panel");

  server_.send(200, "text/html; charset=utf-8",
               "<html><body><h3>Zapisano konfigurację</h3><p>Urządzenie zastosuje zmiany po restarcie.</p>"
               "<a href='/'>Powrót</a></body></html>");
}

void WebConfigServer::handleLogs() {
  if (!authenticate()) return;
  server_.send(200, "text/html; charset=utf-8",
               "<html><body><h3>Logi</h3>" + logger_.toHtml() + "<p><a href='/'>Powrót</a></p></body></html>");
}

void WebConfigServer::handleStatus() {
  if (!authenticate()) return;
  const String payload = statusProvider_ ? statusProvider_() : String("Brak danych");
  server_.send(200, "text/plain; charset=utf-8", payload);
}

void WebConfigServer::handleReboot() {
  if (!authenticate()) return;
  server_.send(200, "text/html; charset=utf-8", "<html><body><h3>Restart</h3></body></html>");
  delay(300);
  if (onReboot_) onReboot_();
}

String WebConfigServer::buildPage(const DeviceConfig& cfg) const {
  String html;
  html.reserve(5000);
  html += F("<!doctype html><html><head><meta charset='utf-8'><title>CT-100</title>");
  html += F("<style>body{font-family:Arial;margin:20px;}fieldset{margin-bottom:16px;}label{display:block;margin:6px 0;}input,select{min-width:240px;}button{padding:8px 14px;}small{color:#666;}</style></head><body>");
  html += F("<h2>CT-100 - panel konfiguracji</h2><p><a href='/logs'>Logi</a> | <a href='/status'>Status TXT</a></p>");
  html += F("<form method='post' action='/save'>");

  html += F("<fieldset><legend>Sieć</legend>");
  html += "<label>Nazwa urządzenia <input name='deviceName' value='" + cfg.network.deviceName + "'></label>";
  html += F("<label>Tryb sieci <select name='networkMode'>");
  html += String("<option value='dhcp'") + (cfg.network.mode == NetworkMode::DHCP ? " selected" : "") + ">DHCP</option>";
  html += String("<option value='static'") + (cfg.network.mode == NetworkMode::STATIC ? " selected" : "") + ">Static IP</option></select></label>";
  html += "<label>IP <input name='ip' value='" + cfg.network.ip.toString() + "'></label>";
  html += "<label>Gateway <input name='gateway' value='" + cfg.network.gateway.toString() + "'></label>";
  html += "<label>Maska <input name='subnet' value='" + cfg.network.subnet.toString() + "'></label>";
  html += "<label>DNS 1 <input name='dns1' value='" + cfg.network.dns1.toString() + "'></label>";
  html += "<label>DNS 2 <input name='dns2' value='" + cfg.network.dns2.toString() + "'></label></fieldset>";

  html += F("<fieldset><legend>Bezpieczeństwo</legend>");
  html += "<label>Użytkownik WWW <input name='webUser' value='" + cfg.security.webUser + "'></label>";
  html += F("<label>Nowe hasło WWW <input name='webPassword' type='password' value=''></label>");
  html += F("<label>Nowe hasło OTA <input name='otaPassword' type='password' value=''></label></fieldset>");

  html += F("<fieldset><legend>RFID</legend>");
  html += String("<label><input type='checkbox' name='rfidEnabled'") + (cfg.rfid.enabled ? " checked" : "") + "> Włącz RFID</label>";
  html += "<label>Baud rate <input name='rfidBaud' value='" + String(cfg.rfid.baudRate) + "'></label>";
  html += F("<label>Kodowanie <select name='rfidEncoding'>");
  html += String("<option value='hex'") + (cfg.rfid.encoding == RfidEncoding::HEX_MODE ? " selected" : "") + ">HEX</option>";
  html += String("<option value='dec'") + (cfg.rfid.encoding == RfidEncoding::DEC_MODE ? " selected" : "") + ">DEC</option>";
  html += String("<option value='raw'") + (cfg.rfid.encoding == RfidEncoding::RAW_MODE ? " selected" : "") + ">RAW</option></select></label></fieldset>";

  html += F("<fieldset><legend>Wyświetlacz i klawiatura</legend>");
  html += String("<label><input type='checkbox' name='displayEnabled'") + (cfg.display.enabled ? " checked" : "") + "> Włącz LCD</label>";
  html += "<label>Kontrast LCD <input name='displayContrast' value='" + String(cfg.display.contrast) + "'></label>";
  html += String("<label><input type='checkbox' name='keypadEnabled'") + (cfg.keypad.enabled ? " checked" : "") + "> Włącz klawiaturę PCF8574</label>";
  html += "<label>Adres PCF8574 <input name='pcfAddr' value='0x" + String(cfg.keypad.pcf8574Address, HEX) + "'></label>";
  html += F("<small>Mapowanie PCF8574: P0-P3 = wiersze, P4-P7 = kolumny.</small></fieldset>");

  html += F("<fieldset><legend>TCP</legend>");
  html += F("<label>Tryb <select name='tcpMode'>");
  html += String("<option value='client'") + (cfg.tcp.mode == TcpMode::CLIENT ? " selected" : "") + ">Client</option>";
  html += String("<option value='host'") + (cfg.tcp.mode == TcpMode::HOST ? " selected" : "") + ">Host</option>";
  html += String("<option value='server'") + (cfg.tcp.mode == TcpMode::SERVER ? " selected" : "") + ">Server</option></select></label>";
  html += "<label>IP serwera TCP <input name='tcpServerIp' value='" + cfg.tcp.serverIp + "'></label>";
  html += "<label>Port serwera TCP <input name='tcpServerPort' value='" + String(cfg.tcp.serverPort) + "'></label>";
  html += "<label>Port nasłuchu <input name='tcpListenPort' value='" + String(cfg.tcp.listenPort) + "'></label></fieldset>";

  html += F("<button type='submit'>Zapisz konfigurację</button></form>");
  html += F("<form method='post' action='/reboot' style='margin-top:12px;'><button type='submit'>Restart urządzenia</button></form>");
  html += F("</body></html>");
  return html;
}
