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
    DeviceConfig activeConfig() const;

    void handleRoot();
    void handleSave();
    void handleLogs();
    void handleStatus();
    void handleReboot();

    void handleApiDeviceInfo();
    void handleApiConfigGet();
    void handleApiConfigPost();

    String buildPage(const DeviceConfig& cfg) const;
    String buildDeviceInfoJson(const DeviceConfig& cfg) const;
    String buildConfigJson(const DeviceConfig& cfg) const;
    void applyConfigFromJson(DeviceConfig& cfg, const String& body) const;
    static String jsonEscape(const String& value);
    static bool parseBoolField(const String& body, const String& key, bool fallback);
    static uint16_t parseUInt16Field(const String& body, const String& key, uint16_t fallback);
    static uint32_t parseUInt32Field(const String& body, const String& key, uint32_t fallback);
    static String parseStringField(const String& body, const String& key, const String& fallback);
};
