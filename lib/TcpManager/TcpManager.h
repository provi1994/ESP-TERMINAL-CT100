#pragma once

#include <Arduino.h>
#include <ETH.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <functional>
#include <memory>

#include "AppTypes.h"
#include "LogManager.h"

class TcpManager {
 public:
  explicit TcpManager(LogManager& logger);

  void begin(const TcpSettings& settings);
  void loop();

  bool sendLine(const String& line);
  bool sendRaw(const uint8_t* data, size_t len);

  bool isConnected();
  bool hasClient();
  String lastMessage() const;

  void onLineReceived(std::function<void(const String&)> callback);

 private:
  void loopClient();
  void loopServer();
  void ensureClientConnected();
  void acceptServerClient();
  void readClient(WiFiClient& client);

  LogManager& logger_;
  TcpSettings settings_;
  WiFiClient client_;
  std::unique_ptr<WiFiServer> server_;
  WiFiClient incomingClient_;
  unsigned long lastReconnectAttempt_ = 0;
  String lastMessage_;
  std::function<void(const String&)> lineCallback_;
};
