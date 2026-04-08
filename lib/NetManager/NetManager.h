#pragma once

#include <Arduino.h>
#include <ETH.h>
#include "AppTypes.h"
#include "LogManager.h"

class NetManager {
public:
  explicit NetManager(LogManager& logger);

  bool begin(const NetworkSettings& settings);
  void loop();

  bool isConnected() const;
  IPAddress localIP() const;

private:
  static void onEvent(WiFiEvent_t event);

  LogManager& logger_;
  String hostname_;
  bool ethConnected_ = false;

  static NetManager* instance_;
};