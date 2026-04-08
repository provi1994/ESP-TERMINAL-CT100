#pragma once

#include <Arduino.h>

namespace Pins {
// WT32-ETH01 Ethernet (NIE UŻYWAĆ DO INNYCH CELÓW)
static constexpr int ETH_POWER = 16;
static constexpr int ETH_MDC = 23;
static constexpr int ETH_MDIO = 18;

// RFID UART2
static constexpr int RFID_RX = 5;   // ESP RX2 -> TX czytnika
static constexpr int RFID_TX = 17;  // ESP TX2 -> RX czytnika (jeśli używany)

// I2C -> PCF8574 dla klawiatury 4x4
static constexpr int I2C_SCL = 32;
static constexpr int I2C_SDA = 33;

// LCD ST7920 w trybie szeregowym 3-wire
// PSB wyświetlacza musi być podłączony do GND.
static constexpr int LCD_CLK = 14;   // E / SCLK
static constexpr int LCD_MOSI = 4;   // RW / SIDww          
static constexpr int LCD_CS = 15;    // RS / CS
static constexpr uint8_t LCD_RST = 255; // U8X8_PIN_NONE bez zależności od U8g2lib.h
}
