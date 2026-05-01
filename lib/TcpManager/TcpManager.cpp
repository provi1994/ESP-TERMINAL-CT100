#include "TcpManager.h"

TcpManager::TcpManager(LogManager& logger) : logger_(logger) {}

void TcpManager::begin(const TcpSettings& settings) {
  settings_ = settings;
  client_.stop();
  incomingClient_.stop();
  server_.reset();
  lastReconnectAttempt_ = 0;
  lastMessage_ = "";

  if (settings_.mode == TcpMode::CLIENT) {
    logger_.info("TCP mode: CLIENT -> " + settings_.serverIp + ":" + String(settings_.serverPort));
  } else {
    server_.reset(new WiFiServer(settings_.listenPort));
    server_->begin();
    server_->setNoDelay(true);
    logger_.info("TCP mode: SERVER/HOST listening on port " + String(settings_.listenPort));
  }
}

void TcpManager::onLineReceived(std::function<void(const String&)> callback) { lineCallback_ = callback; }

void TcpManager::loop() {
  if (!ETH.linkUp()) return;
  if (settings_.mode == TcpMode::CLIENT) loopClient();
  else loopServer();
}

void TcpManager::loopClient() {
  ensureClientConnected();
  if (client_.connected()) readClient(client_);
}

void TcpManager::loopServer() {
  if (!server_) return;
  if (!incomingClient_.connected()) acceptServerClient();
  if (incomingClient_.connected()) readClient(incomingClient_);
}

void TcpManager::ensureClientConnected() {
  if (client_.connected()) return;
  if (!settings_.autoReconnect) return;

  const unsigned long now = millis();
  const unsigned long reconnectEvery = settings_.reconnectIntervalMs > 0 ? settings_.reconnectIntervalMs : 5000UL;
  if (now - lastReconnectAttempt_ < reconnectEvery) return;
  lastReconnectAttempt_ = now;

  if (ETH.localIP() == IPAddress((uint32_t)0)) {
    logger_.warn("TCP reconnect skipped: local IP not ready");
    return;
  }

  IPAddress ip;
  if (!ip.fromString(settings_.serverIp)) {
    logger_.error("TCP invalid server IP: " + settings_.serverIp);
    return;
  }

  client_.stop();
  client_.setTimeout(settings_.connectTimeoutMs > 0 ? settings_.connectTimeoutMs : 350);
  logger_.info("TCP connecting to " + settings_.serverIp + ":" + String(settings_.serverPort));

#if defined(ESP32)
  bool connected = client_.connect(ip, settings_.serverPort, settings_.connectTimeoutMs > 0 ? settings_.connectTimeoutMs : 350);
#else
  bool connected = client_.connect(ip, settings_.serverPort);
#endif

  if (connected) {
    client_.setNoDelay(true);
    logger_.info("TCP connected");
  } else {
    client_.stop();
    logger_.warn("TCP connect failed");
  }
}

void TcpManager::acceptServerClient() {
  WiFiClient newClient = server_->available();
  if (!newClient) return;

  if (incomingClient_.connected()) {
    newClient.stop();
    logger_.warn("TCP rejected extra client");
    return;
  }

  incomingClient_ = newClient;
  incomingClient_.setNoDelay(true);
  logger_.info("TCP client connected to server socket");
}

void TcpManager::readClient(WiFiClient& client) {
  while (client.available()) {
    char c = static_cast<char>(client.read());
    if (c == '\r') continue;

    if (c == '\n') {
      if (!lastMessage_.isEmpty()) {
        logger_.info("TCP RX: " + lastMessage_);
        if (lineCallback_) lineCallback_(lastMessage_);
        lastMessage_.clear();
      }
    } else {
      lastMessage_ += c;



      if (lastMessage_.length() > 256) lastMessage_.remove(0, lastMessage_.length() - 256);
    }
  }

  if (!client.connected() && !lastMessage_.isEmpty()) {
    logger_.info("TCP RX on close: " + lastMessage_);
    if (lineCallback_) lineCallback_(lastMessage_);
    lastMessage_.clear();
  }
}

bool TcpManager::sendLine(const String& line) {
  const String out = line + "\r\n";

  if (settings_.mode == TcpMode::CLIENT) {
    if (!client_.connected()) return false;
    client_.print(out);
    return true;
  }

  if (!incomingClient_.connected()) return false;
  incomingClient_.print(out);
  return true;
}

bool TcpManager::sendRaw(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) return false;
  if (settings_.mode == TcpMode::CLIENT) return client_.connected() && client_.write(data, len) == len;
  return incomingClient_.connected() && incomingClient_.write(data, len) == len;
}

bool TcpManager::isConnected() {
  return settings_.mode == TcpMode::CLIENT ? client_.connected() : incomingClient_.connected();
}

bool TcpManager::hasClient() { return incomingClient_.connected(); }
String TcpManager::lastMessage() const { return lastMessage_; }
