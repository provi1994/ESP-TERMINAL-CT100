#include "DisplayCommandService.h"
#include <WebServer.h>
#include "DisplayManager.h"
#include "LogManager.h"

DisplayCommandService::DisplayCommandService(DisplayManager& display, LogManager& logger)
    : display_(display), logger_(logger) {}

void DisplayCommandService::begin() {
    setHomeScreen("CT-100", "Gotowy", "", "");
    home();
}

void DisplayCommandService::loop() {
    if (dirty_) {
        render();
        dirty_ = false;
    }
}

String DisplayCommandService::normalizeText(const String& input) {
    String out;
    out.reserve(kMaxCharsPerLine);
    for (size_t i = 0; i < input.length() && out.length() < kMaxCharsPerLine; ++i) {
        char c = input[i];
        if (c == '\r' || c == '\n') break;
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) {
            out += '?';
        } else {
            out += c;
        }
    }
    return out;
}

String DisplayCommandService::jsonEscape(const String& value) {
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

String DisplayCommandService::parseStringField(const String& body, const String& key, const String& fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    const int firstQuote = body.indexOf('"', colonPos + 1);
    if (firstQuote < 0) return fallback;
    const int secondQuote = body.indexOf('"', firstQuote + 1);
    if (secondQuote < 0) return fallback;
    return body.substring(firstQuote + 1, secondQuote);
}

uint8_t DisplayCommandService::parseUInt8Field(const String& body, const String& key, uint8_t fallback) {
    const String needle = "\"" + key + "\"";
    const int keyPos = body.indexOf(needle);
    if (keyPos < 0) return fallback;
    const int colonPos = body.indexOf(':', keyPos + needle.length());
    if (colonPos < 0) return fallback;
    int start = colonPos + 1;
    while (start < (int)body.length() && isspace((unsigned char)body[start])) ++start;
    int end = start;
    while (end < (int)body.length() && isdigit((unsigned char)body[end])) ++end;
    if (end <= start) return fallback;
    return static_cast<uint8_t>(body.substring(start, end).toInt());
}

bool DisplayCommandService::setLine(uint8_t lineNo, const String& text) {
    if (lineNo < 1 || lineNo > kMaxLines) return false;
    lines_[lineNo - 1] = normalizeText(text);
    dirty_ = true;
    return true;
}

bool DisplayCommandService::show(const String& line1, const String& line2, const String& line3, const String& line4) {
    lines_[0] = normalizeText(line1);
    lines_[1] = normalizeText(line2);
    lines_[2] = normalizeText(line3);
    lines_[3] = normalizeText(line4);
    dirty_ = true;
    return true;
}

void DisplayCommandService::clear() {
    for (uint8_t i = 0; i < kMaxLines; ++i) lines_[i] = "";
    dirty_ = true;
}

void DisplayCommandService::home() {
    for (uint8_t i = 0; i < kMaxLines; ++i) lines_[i] = homeLines_[i];
    dirty_ = true;
}

void DisplayCommandService::setHomeScreen(const String& line1, const String& line2, const String& line3, const String& line4) {
    homeLines_[0] = normalizeText(line1);
    homeLines_[1] = normalizeText(line2);
    homeLines_[2] = normalizeText(line3);
    homeLines_[3] = normalizeText(line4);
}

String DisplayCommandService::getLine(uint8_t lineNo) const {
    if (lineNo < 1 || lineNo > kMaxLines) return "";
    return lines_[lineNo - 1];
}

String DisplayCommandService::getStateJson() const {
    String out;
    out.reserve(256);
    out += "{";
    out += "\"ok\":true,";
    out += "\"line1\":\"" + jsonEscape(lines_[0]) + "\",";
    out += "\"line2\":\"" + jsonEscape(lines_[1]) + "\",";
    out += "\"line3\":\"" + jsonEscape(lines_[2]) + "\",";
    out += "\"line4\":\"" + jsonEscape(lines_[3]) + "\"";
    out += "}";
    return out;
}

void DisplayCommandService::render() {
    // Adapt this one line if your DisplayManager uses a different API.
    display_.showText4(lines_[0], lines_[1], lines_[2], lines_[3]);
    logger_.info("LCD updated");
}

bool DisplayCommandService::parseSetCommand(const String& cmd, String& response) {
    const int p1 = cmd.indexOf(':');
    const int p2 = cmd.indexOf(':', p1 + 1);
    const int p3 = cmd.indexOf(':', p2 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0) {
        response = "ERR:LCD:INVALID_CMD";
        return false;
    }
    const uint8_t lineNo = static_cast<uint8_t>(cmd.substring(p2 + 1, p3).toInt());
    const String text = cmd.substring(p3 + 1);
    if (!setLine(lineNo, text)) {
        response = "ERR:LCD:BAD_LINE";
        return false;
    }
    response = "OK:LCD:SET:" + String(lineNo);
    return true;
}

bool DisplayCommandService::parseShowCommand(const String& cmd, String& response) {
    String payload = cmd.substring(String("LCD:SHOW:").length());
    String parts[4];
    int start = 0;
    int idx = 0;
    while (idx < 4) {
        int sep = payload.indexOf('|', start);
        if (sep < 0) {
            parts[idx++] = payload.substring(start);
            break;
        }
        parts[idx++] = payload.substring(start, sep);
        start = sep + 1;
    }
    while (idx < 4) parts[idx++] = "";
    show(parts[0], parts[1], parts[2], parts[3]);
    response = "OK:LCD:SHOW";
    return true;
}

String DisplayCommandService::handleTcpCommand(const String& line) {
    String cmd = line;
    cmd.trim();
    if (cmd == "LCD:CLEAR") {
        clear();
        return "OK:LCD:CLEAR";
    }
    if (cmd == "LCD:HOME") {
        home();
        return "OK:LCD:HOME";
    }
    if (cmd.startsWith("LCD:SET:")) {
        String response;
        parseSetCommand(cmd, response);
        return response;
    }
    if (cmd.startsWith("LCD:SHOW:")) {
        String response;
        parseShowCommand(cmd, response);
        return response;
    }
    return "ERR:LCD:INVALID_CMD";
}

void DisplayCommandService::attachHttpRoutes(WebServer& server, std::function<bool()> authGuard) {
    server.on("/api/display/line", HTTP_POST, [&]() { handleApiDisplayLine(server, authGuard); });
    server.on("/api/display/show", HTTP_POST, [&]() { handleApiDisplayShow(server, authGuard); });
    server.on("/api/display/clear", HTTP_POST, [&]() { handleApiDisplayClear(server, authGuard); });
    server.on("/api/display/home", HTTP_POST, [&]() { handleApiDisplayHome(server, authGuard); });
    server.on("/api/display/state", HTTP_GET, [&]() { handleApiDisplayState(server, authGuard); });
}

void DisplayCommandService::handleApiDisplayLine(WebServer& server, std::function<bool()> authGuard) {
    if (authGuard && !authGuard()) return;
    const String body = server.arg("plain");
    const uint8_t lineNo = parseUInt8Field(body, "line", 0);
    const String text = parseStringField(body, "text", "");
    if (!setLine(lineNo, text)) {
        server.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"BAD_LINE\"}");
        return;
    }
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

void DisplayCommandService::handleApiDisplayShow(WebServer& server, std::function<bool()> authGuard) {
    if (authGuard && !authGuard()) return;
    const String body = server.arg("plain");
    show(
        parseStringField(body, "line1", ""),
        parseStringField(body, "line2", ""),
        parseStringField(body, "line3", ""),
        parseStringField(body, "line4", "")
    );
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

void DisplayCommandService::handleApiDisplayClear(WebServer& server, std::function<bool()> authGuard) {
    if (authGuard && !authGuard()) return;
    clear();
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

void DisplayCommandService::handleApiDisplayHome(WebServer& server, std::function<bool()> authGuard) {
    if (authGuard && !authGuard()) return;
    home();
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

void DisplayCommandService::handleApiDisplayState(WebServer& server, std::function<bool()> authGuard) {
    if (authGuard && !authGuard()) return;
    server.send(200, "application/json; charset=utf-8", getStateJson());
}
