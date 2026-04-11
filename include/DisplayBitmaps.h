#pragma once
#include <Arduino.h>

namespace DisplayBitmaps {

constexpr uint16_t SCREEN_WIDTH = 128;
constexpr uint16_t SCREEN_HEIGHT = 64;
constexpr size_t BITMAP_SIZE_128X64 = 1024;

// Własne bitmapy podmieniasz w DisplayBitmaps.cpp
extern const unsigned char LOGO_128X64[BITMAP_SIZE_128X64] PROGMEM;
extern const unsigned char RFID_128X64[BITMAP_SIZE_128X64] PROGMEM;
extern const unsigned char UID_128X64[BITMAP_SIZE_128X64] PROGMEM;

}