#pragma once

#include <Arduino.h>

class ShiftRegister74HC595 {
public:
    ShiftRegister74HC595(int dataPin, int clockPin, int latchPin);

    void begin();
    void setBit(uint8_t bitIndex, bool state);
    bool getBit(uint8_t bitIndex) const;
    void writeByte(uint8_t value);
    uint8_t state() const { return state_; }

private:
    int dataPin_;
    int clockPin_;
    int latchPin_;
    uint8_t state_ = 0;

    void flush();
};
