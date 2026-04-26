#pragma once

#include <Arduino.h>

namespace Pins {
static constexpr int ETH_POWER = 16;
static constexpr int ETH_MDC = 23;
static constexpr int ETH_MDIO = 18;

static constexpr int RFID_RX = 5;
static constexpr int RFID_TX = -1;

static constexpr int QR_RX = 36;
static constexpr int QR_TX = 2;

static constexpr int SHIFT595_DATA = 17;
static constexpr int SHIFT595_CLOCK = 1;
static constexpr int SHIFT595_LATCH = 3;

static constexpr uint8_t OUT1_BIT = 2;
static constexpr uint8_t OUT2_BIT = 1;
static constexpr uint8_t BUZZER_BIT = 3;

static constexpr bool OUT1_ACTIVE_LOW = false;
static constexpr bool OUT2_ACTIVE_LOW = false;
static constexpr bool BUZZER_ACTIVE_LOW = false;

static constexpr int I2C_SCL = 32;
static constexpr int I2C_SDA = 33;

static constexpr int LCD_CLK = 14;
static constexpr int LCD_MOSI = 4;
static constexpr int LCD_CS = 15;
static constexpr uint8_t LCD_RST = 255;
}
