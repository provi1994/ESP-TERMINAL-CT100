#include "LogManager.h"

LogManager::LogManager(size_t maxEntries) : maxEntries_(maxEntries) {}

void LogManager::info(const String& msg) { add("INFO", msg); }
void LogManager::warn(const String& msg) { add("WARN", msg); }
void LogManager::error(const String& msg) { add("ERROR", msg); }

void LogManager::add(const String& level, const String& msg) {
  const String line = timestamp() + " [" + level + "] " + msg;
  Serial.println(line);
  entries_.push_back(line);
  while (entries_.size() > maxEntries_) {
    entries_.pop_front();
  }
}

String LogManager::toText() const {
  String out;
  for (const String& entry : entries_) {
    out += entry + "\n";
  }
  return out;
}

String LogManager::toHtml() const {
  String out = "<pre>";
  for (const String& entry : entries_) {
    out += htmlEscape(entry) + "\n";
  }
  out += "</pre>";
  return out;
}

std::vector<String> LogManager::snapshot() const {
  return std::vector<String>(entries_.begin(), entries_.end());
}

String LogManager::htmlEscape(const String& value) {
  String out;
  out.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += value[i]; break;
    }
  }
  return out;
}

String LogManager::timestamp() {
  return String(millis());
}
