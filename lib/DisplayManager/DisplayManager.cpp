#include "DisplayManager.h"

DisplayManager::DisplayManager(LogManager& logger, uint8_t clk, uint8_t mosi, uint8_t cs, uint8_t rst)
    : logger_(logger), u8g2_(U8G2_R0, clk, mosi, cs, rst) {}

void DisplayManager::begin(uint8_t contrast) {
  u8g2_.begin();
  u8g2_.setContrast(contrast);
  u8g2_.setFont(u8g2_font_6x12_tf);
  logger_.info("ST7920 display initialized");
}

void DisplayManager::drawHeader(const String& title) {
  u8g2_.clearBuffer();
  u8g2_.drawFrame(0, 0, 128, 64);
  u8g2_.drawBox(0, 0, 128, 12);
  u8g2_.setDrawColor(0);
  u8g2_.drawStr(4, 10, title.c_str());
  u8g2_.setDrawColor(1);
}

void DisplayManager::showBoot(const String& deviceName) {
  drawHeader("CT-100");
  u8g2_.drawStr(4, 24, "Start systemu");
  u8g2_.drawStr(4, 38, deviceName.c_str());
  u8g2_.drawStr(4, 52, "Ethernet + Web + TCP");
  u8g2_.sendBuffer();
}

void DisplayManager::showStatus(const String& line1, const String& line2, const String& line3, const String& line4) {
  drawHeader("Status");
  u8g2_.drawStr(4, 24, line1.c_str());
  u8g2_.drawStr(4, 36, line2.c_str());
  u8g2_.drawStr(4, 48, line3.c_str());
  u8g2_.drawStr(4, 60, line4.c_str());
  u8g2_.sendBuffer();
}

void DisplayManager::showCard(const String& card) {
  drawHeader("RFID");
  u8g2_.drawStr(4, 24, "Odczyt karty:");
  u8g2_.drawStr(4, 40, card.c_str());
  u8g2_.sendBuffer();
}

void DisplayManager::showTcp(const String& message) {
  drawHeader("TCP");
  u8g2_.drawStr(4, 24, "Ostatnia ramka:");
  u8g2_.drawStr(4, 40, message.substring(0, 20).c_str());
  u8g2_.drawStr(4, 54, message.substring(20, 40).c_str());
  u8g2_.sendBuffer();
}
