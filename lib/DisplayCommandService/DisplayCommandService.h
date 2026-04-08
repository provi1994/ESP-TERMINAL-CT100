#pragma once
#include <Arduino.h>
#include <functional>

class WebServer;
class DisplayManager;
class LogManager;

class DisplayCommandService {
public:
    static constexpr uint8_t kMaxLines = 4;
    static constexpr uint8_t kMaxCharsPerLine = 21;

    DisplayCommandService(DisplayManager& display, LogManager& logger);

    void begin();
    void loop();

    String handleTcpCommand(const String& line);
    void attachHttpRoutes(WebServer& server, std::function<bool()> authGuard);

    bool setLine(uint8_t lineNo, const String& text);
    bool show(const String& line1, const String& line2 = "", const String& line3 = "", const String& line4 = "");
    void clear();
    void home();
    void setHomeScreen(const String& line1, const String& line2 = "", const String& line3 = "", const String& line4 = "");

    String getLine(uint8_t lineNo) const;
    String getStateJson() const;

private:
    DisplayManager& display_;
    LogManager& logger_;

    String lines_[kMaxLines];
    String homeLines_[kMaxLines];
    bool dirty_ = false;

    static String normalizeText(const String& input);
    static String jsonEscape(const String& value);
    static String parseStringField(const String& body, const String& key, const String& fallback = "");
    static uint8_t parseUInt8Field(const String& body, const String& key, uint8_t fallback);

    void render();
    bool parseSetCommand(const String& cmd, String& response);
    bool parseShowCommand(const String& cmd, String& response);

    void handleApiDisplayLine(WebServer& server, std::function<bool()> authGuard);
    void handleApiDisplayShow(WebServer& server, std::function<bool()> authGuard);
    void handleApiDisplayClear(WebServer& server, std::function<bool()> authGuard);
    void handleApiDisplayHome(WebServer& server, std::function<bool()> authGuard);
    void handleApiDisplayState(WebServer& server, std::function<bool()> authGuard);
};
