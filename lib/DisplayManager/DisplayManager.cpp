#include "DisplayManager.h"
#include "DisplayBitmaps.h"

DisplayManager::DisplayManager(LogManager& logger, uint8_t clk, uint8_t mosi, uint8_t cs, uint8_t rst)
    : logger_(logger), u8g2_(U8G2_R2, clk, mosi, cs, rst) {}

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
  setDefaultFont();
  drawTextLine(10, "Aktualna waga:");
  u8g2_.drawFrame(4, 15, 120, 40);
  setLargeFont();
  u8g2_.drawStr(8, 42, fit(weight, 16).c_str());
  setDefaultFont();
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

void DisplayManager::showDriverHint(const String& title,
                                    const String& line1,
                                    const String& line2,
                                    DriverHintIcon icon,
                                    uint8_t animFrame) {
  beginScreen();
  setDefaultFont();

  drawHeader(fit(title, 20));

  // Lewa część: ikona 32x32
  drawDriverIcon(icon, 6, 20, animFrame);

  // Prawa część: komunikat dla kierowcy
  setDefaultFont();
  u8g2_.drawStr(44, 28, fit(line1, 13).c_str());

  if (!line2.isEmpty()) {
    u8g2_.drawStr(44, 42, fit(line2, 13).c_str());
  }

  // Dolny pasek z delikatną animacją aktywności
  const uint8_t dotX = 104 + ((animFrame % 4) * 5);
  u8g2_.drawHLine(4, 57, 96);
  u8g2_.drawBox(dotX, 55, 4, 4);

  endScreen();
}

void DisplayManager::drawDriverIcon(DriverHintIcon icon, int x, int y, uint8_t animFrame) {
  switch (icon) {
    case DriverHintIcon::RFID:
      drawRfidIcon(x, y, animFrame);
      break;

    case DriverHintIcon::KEYPAD:
      drawKeypadIcon(x, y, animFrame);
      break;

    case DriverHintIcon::QR:
      drawQrIcon(x, y, animFrame);
      break;

    case DriverHintIcon::WAIT:
      drawWaitIcon(x, y, animFrame);
      break;

    case DriverHintIcon::SUCCESS:
      drawSuccessIcon(x, y, animFrame);
      break;

    case DriverHintIcon::ERROR_ICON:
      drawErrorIcon(x, y, animFrame);
      break;

    case DriverHintIcon::PROCESSING:
      drawProcessingIcon(x, y, animFrame);
      break;

    case DriverHintIcon::NONE:
    default:
      break;
  }
}

void DisplayManager::drawRfidIcon(int x, int y, uint8_t animFrame) {
  // Karta
  u8g2_.drawRFrame(x, y + 3, 20, 26, 2);
  u8g2_.drawFrame(x + 4, y + 8, 12, 8);
  u8g2_.drawBox(x + 7, y + 18, 6, 4);

  // Fale RFID - prosta animacja
  const int cx = x + 23;
  const int cy = y + 16;

  if ((animFrame % 2) == 0) {
    u8g2_.drawCircle(cx, cy, 4, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
    u8g2_.drawCircle(cx, cy, 8, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
  } else {
    u8g2_.drawCircle(cx, cy, 7, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
    u8g2_.drawCircle(cx, cy, 11, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
  }
}

void DisplayManager::drawKeypadIcon(int x, int y, uint8_t animFrame) {
  u8g2_.drawRFrame(x + 3, y, 26, 32, 2);

  const int keyW = 5;
  const int keyH = 4;
  const int gap = 2;

  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 3; ++col) {
      const int px = x + 7 + col * (keyW + gap);
      const int py = y + 5 + row * (keyH + gap);

      // Migający środkowy dolny klawisz
      if ((animFrame % 2 == 0) && row == 3 && col == 1) {
        u8g2_.drawBox(px, py, keyW, keyH);
      } else {
        u8g2_.drawFrame(px, py, keyW, keyH);
      }
    }
  }

  // Mały "wyświetlacz" klawiatury
  u8g2_.drawHLine(x + 8, y + 28, 16);
}

void DisplayManager::drawQrIcon(int x, int y, uint8_t animFrame) {
  u8g2_.drawFrame(x + 1, y + 1, 30, 30);

  // Znaczniki QR
  u8g2_.drawFrame(x + 4, y + 4, 8, 8);
  u8g2_.drawBox(x + 6, y + 6, 4, 4);

  u8g2_.drawFrame(x + 20, y + 4, 8, 8);
  u8g2_.drawBox(x + 22, y + 6, 4, 4);

  u8g2_.drawFrame(x + 4, y + 20, 8, 8);
  u8g2_.drawBox(x + 6, y + 22, 4, 4);

  // Losowe piksele w środku
  u8g2_.drawBox(x + 15, y + 15, 2, 2);
  u8g2_.drawBox(x + 18, y + 17, 2, 2);
  u8g2_.drawBox(x + 14, y + 22, 2, 2);
  u8g2_.drawBox(x + 23, y + 20, 2, 2);
  u8g2_.drawBox(x + 18, y + 25, 2, 2);

  // Animowana linia skanowania
  const int lineY = y + 5 + (animFrame % 4) * 6;
  u8g2_.drawHLine(x + 3, lineY, 26);
}

void DisplayManager::drawWaitIcon(int x, int y, uint8_t animFrame) {
  // Prosty terminal / waga
  u8g2_.drawRFrame(x + 2, y + 2, 28, 22, 2);
  u8g2_.drawHLine(x + 8, y + 28, 16);
  u8g2_.drawFrame(x + 7, y + 25, 18, 5);

  // Migający punkt gotowości
  if ((animFrame % 2) == 0) {
    u8g2_.drawDisc(x + 25, y + 8, 2);
  } else {
    u8g2_.drawCircle(x + 25, y + 8, 2);
  }

  // Symbol karty
  u8g2_.drawFrame(x + 7, y + 9, 12, 8);
}

void DisplayManager::drawSuccessIcon(int x, int y, uint8_t animFrame) {
  u8g2_.drawCircle(x + 16, y + 16, 14);

  // Check
  u8g2_.drawLine(x + 8, y + 17, x + 14, y + 23);
  u8g2_.drawLine(x + 14, y + 23, x + 25, y + 9);

  if ((animFrame % 2) == 0) {
    u8g2_.drawCircle(x + 16, y + 16, 12);
  }
}

void DisplayManager::drawErrorIcon(int x, int y, uint8_t animFrame) {
  u8g2_.drawCircle(x + 16, y + 16, 14);

  // X
  u8g2_.drawLine(x + 9, y + 9, x + 23, y + 23);
  u8g2_.drawLine(x + 23, y + 9, x + 9, y + 23);

  if ((animFrame % 2) == 0) {
    u8g2_.drawFrame(x + 2, y + 2, 28, 28);
  }
}

void DisplayManager::drawProcessingIcon(int x, int y, uint8_t animFrame) {
  u8g2_.drawCircle(x + 16, y + 16, 13);

  // Spinner 4 klatki
  switch (animFrame % 4) {
    case 0:
      u8g2_.drawBox(x + 15, y + 3, 3, 7);
      break;
    case 1:
      u8g2_.drawBox(x + 22, y + 15, 7, 3);
      break;
    case 2:
      u8g2_.drawBox(x + 15, y + 22, 3, 7);
      break;
    default:
      u8g2_.drawBox(x + 3, y + 15, 7, 3);
      break;
  }

  u8g2_.drawDisc(x + 16, y + 16, 3);
}