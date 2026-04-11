#pragma once
#include <WebServer.h>
#include <functional>
#include "ConfigManager.h"
#include "LogManager.h"

enum class WebUserRole : uint8_t {
    NONE = 0,
    SERVICE = 1,
    ADMIN = 2
};

class WebConfigServer {
public:
    explicit WebConfigServer(LogManager& logger);

    void begin(const DeviceConfig& config);
    void loop();

    void onSave(std::function<void(DeviceConfig)> callback);
    void onReboot(std::function<void()> callback);

    void setConfigProvider(std::function<DeviceConfig()> provider);
    void setStatusProvider(std::function<String()> provider);
    void setRuntimeJsonProvider(std::function<String()> provider);

private:
    LogManager& logger_;
    WebServer server_;
    DeviceConfig config_;
    std::function<void(DeviceConfig)> onSave_;
    std::function<void()> onReboot_;
    std::function<DeviceConfig()> configProvider_;
    std::function<String()> statusProvider_;
    std::function<String()> runtimeJsonProvider_;

    bool authenticate();
    bool isAdmin();
    WebUserRole detectRole();
    String currentUserName();
    String currentRoleName();
    DeviceConfig activeConfig() const;

    void handleRoot();
    void handleLogs();
    void handleStatus();
    void handleReboot();

    void handleApiDeviceInfo();
    void handleApiConfigGet();
    void handleApiConfigPost();
    void handleApiRuntimeGet();

    void handleFirmwarePage();
    void handleFirmwareUpload();

    String buildPage(const DeviceConfig& cfg);
    String buildDeviceInfoJson(const DeviceConfig& cfg);
    String buildConfigJson(const DeviceConfig& cfg);

    void applyConfigFromJson(DeviceConfig& cfg, const String& body, bool allowSecurity) const;

    static String jsonEscape(const String& value);
    static bool parseBoolField(const String& body, const String& key, bool fallback);
    static uint16_t parseUInt16Field(const String& body, const String& key, uint16_t fallback);
    static uint32_t parseUInt32Field(const String& body, const String& key, uint32_t fallback);
    static String parseStringField(const String& body, const String& key, const String& fallback);
};
