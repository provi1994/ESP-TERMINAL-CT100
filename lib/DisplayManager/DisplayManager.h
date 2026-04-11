#pragma once

#include <U8g2lib.h>
#include "LogManager.h"

class DisplayManager {
 public:
  DisplayManager(LogManager& logger, uint8_t clk, uint8_t mosi, uint8_t cs, uint8_t rst);

  void begin(uint8_t contrast);
  void showLogo();
  void showBoot(const String& deviceName);
  void showStatus(const String& line1, const String& line2, const String& line3, const String& line4);
  void showInfo(const String& title, const String& line1, const String& line2, const String& line3, const String& line4);
  void showCard(const String& card);
  void showUidScreen(const String& uid);
  void showTcp(const String& message);
  void showIdleWeight(const String& header, const String& weight, const String& prompt);
  void showRfidPrompt(const String& title, const String& line1, const String& line2);
  void showInputScreen(const String& title, const String& value, const String& hint);
  void showSummaryScreen(const String& line1, const String& line2, const String& line3, const String& line4);
  void showResultScreen(const String& title, const String& line1, const String& line2);

 private:
  LogManager& logger_;
  U8G2_ST7920_128X64_F_SW_SPI u8g2_;

  void drawHeader(const String& title);
  void drawHeaderBar(const String& text);
  static String fit(const String& text, size_t maxLen);
};
