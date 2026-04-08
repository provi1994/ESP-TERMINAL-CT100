#pragma once

#include <Arduino.h>
#include <deque>
#include <vector>

class LogManager {
 public:
  explicit LogManager(size_t maxEntries = 80);

  void info(const String& msg);
  void warn(const String& msg);
  void error(const String& msg);
  void add(const String& level, const String& msg);

  String toText() const;
  String toHtml() const;
  std::vector<String> snapshot() const;

 private:
  size_t maxEntries_;
  std::deque<String> entries_;

  static String htmlEscape(const String& value);
  static String timestamp();
};
