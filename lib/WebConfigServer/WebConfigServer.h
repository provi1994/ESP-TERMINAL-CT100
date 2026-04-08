#pragma once

#include <WebServer.h>
#include <functional>

#include "ConfigManager.h"
#include "LogManager.h"

class WebConfigServer {
 public:
  explicit WebConfigServer(LogManager& logger);

  void begin(const DeviceConfig& config);
  void loop();
  void onSave(std::function<void(DeviceConfig)> callback);
  void onReboot(std::function<void()> callback);
  void setConfigProvider(std::function<DeviceConfig()> provider);
  void setStatusProvider(std::function<String()> provider);

 private:
  LogManager& logger_;
  WebServer server_;
  DeviceConfig config_;
  std::function<void(DeviceConfig)> onSave_;
  std::function<void()> onReboot_;
  std::function<DeviceConfig()> configProvider_;
  std::function<String()> statusProvider_;

  bool authenticate();
  void handleRoot();
  void handleSave();
  void handleLogs();
  void handleStatus();
  void handleReboot();
  String buildPage(const DeviceConfig& cfg) const;
};
