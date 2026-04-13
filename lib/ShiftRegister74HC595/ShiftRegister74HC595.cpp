#include "ShiftRegister74HC595.h"

ShiftRegister74HC595::ShiftRegister74HC595(int dataPin, int clockPin, int latchPin)
    : dataPin_(dataPin), clockPin_(clockPin), latchPin_(latchPin) {}

void ShiftRegister74HC595::begin() {
    pinMode(dataPin_, OUTPUT);
    pinMode(clockPin_, OUTPUT);
    pinMode(latchPin_, OUTPUT);

    digitalWrite(dataPin_, LOW);
    digitalWrite(clockPin_, LOW);
    digitalWrite(latchPin_, LOW);

    state_ = 0;
    flush();
}

void ShiftRegister74HC595::setBit(uint8_t bitIndex, bool state) {
    if (bitIndex > 7) return;
    if (state) state_ |= (1U << bitIndex);
    else state_ &= ~(1U << bitIndex);
    flush();
}

bool ShiftRegister74HC595::getBit(uint8_t bitIndex) const {
    if (bitIndex > 7) return false;
    return (state_ & (1U << bitIndex)) != 0;
}

void ShiftRegister74HC595::writeByte(uint8_t value) {
    state_ = value;
    flush();
}

void ShiftRegister74HC595::flush() {
    digitalWrite(latchPin_, LOW);
    shiftOut(dataPin_, clockPin_, MSBFIRST, state_);
    digitalWrite(latchPin_, HIGH);
}
