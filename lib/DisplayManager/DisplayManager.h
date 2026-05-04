#pragma once

#include <U8g2lib.h>
#include "LogManager.h"

enum class DriverHintIcon : uint8_t {
  NONE = 0,
  RFID,
  KEYPAD,
  QR,
  WAIT,
  SUCCESS,
  ERROR_ICON,
  PROCESSING
};

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

  // Nowy ekran operatorski: ikona + tekst + prosta animacja
  void showDriverHint(const String& title,
                      const String& line1,
                      const String& line2,
                      DriverHintIcon icon,
                      uint8_t animFrame = 0);

  // Prymitywy do składania własnych ekranów
  void beginScreen();
  void endScreen();
  void drawOuterFrame();
  void drawHeader(const String& title);
  void drawHeaderBar(const String& text);
  void drawTextLine(uint8_t y, const String& text, uint8_t x = 4);
  void drawValueBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const String& value);
  void drawHintLine(const String& text);
  void drawBitmapFullScreen(const unsigned char* bitmap);
  void setDefaultFont();
  void setLargeFont();

  static String fit(const String& text, size_t maxLen);

 private:
  LogManager& logger_;
  U8G2_ST7920_128X64_F_SW_SPI u8g2_;

  void drawDriverIcon(DriverHintIcon icon, int x, int y, uint8_t animFrame);
  void drawRfidIcon(int x, int y, uint8_t animFrame);
  void drawKeypadIcon(int x, int y, uint8_t animFrame);
  void drawQrIcon(int x, int y, uint8_t animFrame);
  void drawWaitIcon(int x, int y, uint8_t animFrame);
  void drawSuccessIcon(int x, int y, uint8_t animFrame);
  void drawErrorIcon(int x, int y, uint8_t animFrame);
  void drawProcessingIcon(int x, int y, uint8_t animFrame);
};