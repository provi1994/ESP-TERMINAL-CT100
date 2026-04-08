#pragma once

#include <U8g2lib.h>

#include "LogManager.h"

class DisplayManager {
 public:
  DisplayManager(LogManager& logger, uint8_t clk, uint8_t mosi, uint8_t cs, uint8_t rst);

  void begin(uint8_t contrast);
  void showBoot(const String& deviceName);
  void showStatus(const String& line1, const String& line2, const String& line3, const String& line4);
  void showCard(const String& card);
  void showTcp(const String& message);

 private:
  LogManager& logger_;
  U8G2_ST7920_128X64_F_SW_SPI u8g2_;

  void drawHeader(const String& title);
  static String fit(const String& text, size_t maxLen);
};
