#pragma once

// WT32-ETH01: część GPIO jest zajęta przez PHY Ethernet.
// Poniższe mapowanie jest propozycją startową i może wymagać korekty
// pod Twoją finalną płytkę / sposób okablowania.

namespace Pins {
// RFID 125 kHz po UART2
static constexpr int RFID_RX = 32;
static constexpr int RFID_TX = 33;

// ST7920 w trybie software SPI przez U8g2
static constexpr int LCD_CLK = 14;
static constexpr int LCD_MOSI = 15;
static constexpr int LCD_CS = 2;
static constexpr int LCD_RST = 4;

// Klawiatura 4x4 membranowa
static constexpr int KEYPAD_ROW_1 = 16;
static constexpr int KEYPAD_ROW_2 = 17;
static constexpr int KEYPAD_ROW_3 = 5;
static constexpr int KEYPAD_ROW_4 = 13;
static constexpr int KEYPAD_COL_1 = 34;
static constexpr int KEYPAD_COL_2 = 35;
static constexpr int KEYPAD_COL_3 = 36;
static constexpr int KEYPAD_COL_4 = 39;
}
