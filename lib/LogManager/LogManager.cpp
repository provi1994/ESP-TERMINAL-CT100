#include "LogManager.h"

LogManager::LogManager(size_t maxEntries) : maxEntries_(maxEntries) {}

void LogManager::info(const String& msg) { add("INFO", msg); }
void LogManager::warn(const String& msg) { add("WARN", msg); }
void LogManager::error(const String& msg) { add("ERROR", msg); }

void LogManager::add(const String& level, const String& msg) {
  String line = "[" + timestamp() + "] [" + level + "] " + msg;
  Serial.println(line);
  entries_.push_back(line);
  while (entries_.size() > maxEntries_) {
    entries_.pop_front();
  }
}

String LogManager::toText() const {
  String out;
  for (const auto& line : entries_) {
    out += line + "\n";
  }
  return out;
}

String LogManager::toHtml() const {
  String out = "<pre>";
  for (const auto& line : entries_) {
    out += htmlEscape(line) + "\n";
  }
  out += "</pre>";
  return out;
}

std::vector<String> LogManager::snapshot() const {
  return std::vector<String>(entries_.begin(), entries_.end());
}

String LogManager::htmlEscape(const String& value) {
  String out = value;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

String LogManager::timestamp() {
  const unsigned long ms = millis();
  const unsigned long sec = ms / 1000UL;
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%lu.%03lu", sec, ms % 1000UL);
  return String(buffer);
}
