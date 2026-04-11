#include "DisplayManager.h"
#include "DisplayBitmaps.h"

DisplayManager::DisplayManager(LogManager& logger, uint8_t clk, uint8_t mosi, uint8_t cs, uint8_t rst)
    : logger_(logger), u8g2_(U8G2_R0, clk, mosi, cs, rst) {}

void DisplayManager::begin(uint8_t contrast) {
  u8g2_.begin();
  u8g2_.setContrast(contrast);
  setDefaultFont();
  logger_.info("ST7920 display initialized in 3-wire serial mode");
}

void DisplayManager::setDefaultFont() {
  u8g2_.setFont(u8g2_font_6x12_tf);
}

void DisplayManager::setLargeFont() {
  u8g2_.setFont(u8g2_font_10x20_tf);
}

String DisplayManager::fit(const String& text, size_t maxLen) {
  if (text.length() <= maxLen) return text;
  return text.substring(0, maxLen);
}

void DisplayManager::beginScreen() {
  u8g2_.clearBuffer();
}

void DisplayManager::endScreen() {
  u8g2_.sendBuffer();
}

void DisplayManager::drawOuterFrame() {
  u8g2_.drawFrame(0, 0, 128, 64);
}

void DisplayManager::drawHeader(const String& title) {
  drawOuterFrame();
  u8g2_.drawBox(0, 0, 128, 12);
  u8g2_.setDrawColor(0);
  u8g2_.drawStr(4, 10, fit(title, 20).c_str());
  u8g2_.setDrawColor(1);
}

void DisplayManager::drawHeaderBar(const String& text) {
  drawHeader(text);
}

void DisplayManager::drawTextLine(uint8_t y, const String& text, uint8_t x) {
  u8g2_.drawStr(x, y, fit(text, 20).c_str());
}

void DisplayManager::drawValueBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const String& value) {
  u8g2_.drawFrame(x, y, w, h);
  u8g2_.drawStr(x + 4, y + h - 4, value.c_str());
}

void DisplayManager::drawHintLine(const String& text) {
  u8g2_.drawStr(4, 58, fit(text, 20).c_str());
}

void DisplayManager::drawBitmapFullScreen(const unsigned char* bitmap) {
  u8g2_.drawXBMP(0, 0, DisplayBitmaps::SCREEN_WIDTH, DisplayBitmaps::SCREEN_HEIGHT, bitmap);
}

void DisplayManager::showLogo() {
  beginScreen();
  drawBitmapFullScreen(DisplayBitmaps::LOGO_128X64);
  endScreen();
}

void DisplayManager::showBoot(const String& deviceName) {
  beginScreen();
  drawHeader("START");
  drawTextLine(24, "Uruchamianie...");
  drawTextLine(38, fit(deviceName, 20));
  drawTextLine(54, "ETH + RFID + TCP");
  endScreen();
}

void DisplayManager::showStatus(const String& line1, const String& line2, const String& line3, const String& line4) {
  beginScreen();
  drawHeader("STATUS");
  drawTextLine(22, line1);
  drawTextLine(34, line2);
  drawTextLine(46, line3);
  drawTextLine(58, line4);
  endScreen();
}

void DisplayManager::showInfo(const String& title,
                              const String& line1,
                              const String& line2,
                              const String& line3,
                              const String& line4) {
  beginScreen();
  drawHeader(fit(title, 18));
  drawTextLine(22, line1);
  drawTextLine(34, line2);
  drawTextLine(46, line3);
  drawTextLine(58, line4);
  endScreen();
}

void DisplayManager::showCard(const String& card) {
  beginScreen();
  drawHeader("RFID");
  drawTextLine(24, "Odczyt karty:");
  drawTextLine(40, fit(card, 20));
  endScreen();
}

void DisplayManager::showUidScreen(const String& uid) {
  beginScreen();
  drawHeader("ODCZYTANE UID");
  drawTextLine(24, "UID:");
  drawTextLine(40, fit(uid, 20));
  drawTextLine(56, "Karta odczytana");
  endScreen();
}

void DisplayManager::showTcp(const String& message) {
  beginScreen();
  drawHeader("TCP");
  drawTextLine(24, "Komunikat:");
  drawTextLine(40, fit(message, 20));
  drawTextLine(54, fit(message.substring(20), 20));
  endScreen();
}

void DisplayManager::showIdleWeight(const String& header, const String& weight, const String& prompt) {
  beginScreen();
  drawHeaderBar(header);
  setDefaultFont();
  drawTextLine(22, "Aktualna waga:");
  u8g2_.drawFrame(4, 26, 120, 20);
  setLargeFont();
  u8g2_.drawStr(8, 42, fit(weight, 12).c_str());
  setDefaultFont();
  drawHintLine(prompt);
  endScreen();
}

void DisplayManager::showRfidPrompt(const String& title, const String& line1, const String& line2) {
  beginScreen();
  drawHeader(fit(title, 18));

  // Ikona karty
  u8g2_.drawFrame(8, 18, 20, 14);
  u8g2_.drawFrame(11, 21, 14, 8);

  drawTextLine(28, fit(line1, 14), 34);
  drawTextLine(52, fit(line2, 20), 4);
  endScreen();
}

void DisplayManager::showInputScreen(const String& title, const String& value, const String& hint) {
  beginScreen();
  drawHeader(fit(title, 18));
  drawTextLine(22, "Wprowadz:");
  u8g2_.drawFrame(4, 28, 120, 16);
  u8g2_.drawStr(8, 40, fit(value, 16).c_str());
  drawHintLine(hint);
  endScreen();
}

void DisplayManager::showSummaryScreen(const String& line1, const String& line2, const String& line3, const String& line4) {
  beginScreen();
  drawHeader("PODSUMOWANIE");
  drawTextLine(22, line1);
  drawTextLine(34, line2);
  drawTextLine(46, line3);
  drawTextLine(58, line4);
  endScreen();
}

void DisplayManager::showResultScreen(const String& title, const String& line1, const String& line2) {
  beginScreen();
  drawHeader(fit(title, 18));
  drawTextLine(28, line1);
  drawTextLine(46, line2);
  endScreen();
}