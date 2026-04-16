#pragma once

#include <Arduino.h>

namespace Pins {
// WT32-ETH01 Ethernet
static constexpr int ETH_POWER = 16;
static constexpr int ETH_MDC = 23;
static constexpr int ETH_MDIO = 18;

// RFID 1-line UART2
static constexpr int RFID_RX = 5;
static constexpr int RFID_TX = -1;  // nieużywany

// QR / skaner kodów
static constexpr int QR_RX = 35;

// 74HC595
static constexpr int SHIFT595_DATA = 17;   // DS
static constexpr int SHIFT595_CLOCK = 1;   // SHCP
static constexpr int SHIFT595_LATCH = 3;   // STCP

// Mapowanie wyjść 74HC595
// QA=0 QB=1 QC=2 QD=3 QE=4 QF=5 QG=6 QH=7
static constexpr uint8_t OUT1_BIT = 2;    // QB = wyjście B
static constexpr uint8_t OUT2_BIT = 1;    // QC = wyjście C
static constexpr uint8_t BUZZER_BIT = 3;  // QD = wyjście D

// Dla BD649 ma być aktywne HIGH
static constexpr bool OUT1_ACTIVE_LOW = false;
static constexpr bool OUT2_ACTIVE_LOW = false;
static constexpr bool BUZZER_ACTIVE_LOW = false;

// I2C -> PCF8574 dla klawiatury 4x4
static constexpr int I2C_SCL = 32;
static constexpr int I2C_SDA = 33;

// LCD ST7920 w trybie szeregowym 3-wire
static constexpr int LCD_CLK = 14;
static constexpr int LCD_MOSI = 4;
static constexpr int LCD_CS = 15;
static constexpr uint8_t LCD_RST = 255;
}