#pragma once

#include <Arduino.h>

#include "LogManager.h"
#include "RuntimeState.h"
#include "ShiftRegister74HC595.h"

class OutputManager {
public:
    OutputManager(LogManager& logger, ShiftRegister74HC595& shift, RuntimeState& state);

    void begin();

    void setOut1(bool logicalState);
    void setOut2(bool logicalState);
    void setBuzzer(bool logicalState);
    void beep(unsigned long durationMs);

    void loop();

    bool handleCommand(const String& cmd, const String& sourceTag);

private:
    LogManager& logger_;
    ShiftRegister74HC595& shift_;
    RuntimeState& rt_;

    static bool toPhysicalLevel(bool logicalState, bool activeLow);
};