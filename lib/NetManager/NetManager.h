#pragma once

#include <ETH.h>
#include <AppTypes.h>
#include "LogManager.h"

class NetManager {
 public:
  explicit NetManager(LogManager& logger);
  bool begin(const NetworkSettings& settings);
  void loop();
  bool isConnected() const;
  IPAddress localIP() const;
  String hostname() const;

 private:
  static NetManager* instance_;
  LogManager& logger_;
  bool connected_ = false;
  String hostname_;
  static void onEvent(WiFiEvent_t event);
  void handleEvent(WiFiEvent_t event);
};
