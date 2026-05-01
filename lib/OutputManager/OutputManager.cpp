#include "OutputManager.h"

#include "Pins.h"

OutputManager::OutputManager(LogManager& logger, ShiftRegister74HC595& shift, RuntimeState& state)
    : logger_(logger), shift_(shift), rt_(state) {}

void OutputManager::begin() {
    shift_.begin();
    setOut1(false);
    setOut2(false);
    setBuzzer(false);
    rt_.buzzerOffAt = 0;
}

bool OutputManager::toPhysicalLevel(bool logicalState, bool activeLow) {
    return activeLow ? !logicalState : logicalState;
}

void OutputManager::setOut1(bool logicalState) {
    rt_.out1State = logicalState;
    shift_.setBit(Pins::OUT1_BIT, toPhysicalLevel(logicalState, Pins::OUT1_ACTIVE_LOW));
}

void OutputManager::setOut2(bool logicalState) {
    rt_.out2State = logicalState;
    shift_.setBit(Pins::OUT2_BIT, toPhysicalLevel(logicalState, Pins::OUT2_ACTIVE_LOW));
}

void OutputManager::setBuzzer(bool logicalState) {
    rt_.buzzerState = logicalState;
    shift_.setBit(Pins::BUZZER_BIT, toPhysicalLevel(logicalState, Pins::BUZZER_ACTIVE_LOW));
}

void OutputManager::beep(unsigned long durationMs) {
    if (durationMs == 0) durationMs = 120;
    if (durationMs > 5000UL) durationMs = 5000UL;

    setBuzzer(true);
    rt_.buzzerOffAt = millis() + durationMs;
}

void OutputManager::loop() {
    if (rt_.buzzerState && rt_.buzzerOffAt > 0 && millis() >= rt_.buzzerOffAt) {
        setBuzzer(false);
        rt_.buzzerOffAt = 0;
    }
}

bool OutputManager::handleCommand(const String& cmd, const String& sourceTag) {
    if (cmd.equalsIgnoreCase("OUT1:ON")) {
        setOut1(true);
        logger_.info("OUT1=ON " + sourceTag);
        return true;
    }

    if (cmd.equalsIgnoreCase("OUT1:OFF")) {
        setOut1(false);
        logger_.info("OUT1=OFF " + sourceTag);
        return true;
    }

    if (cmd.equalsIgnoreCase("OUT2:ON")) {
        setOut2(true);
        logger_.info("OUT2=ON " + sourceTag);
        return true;
    }

    if (cmd.equalsIgnoreCase("OUT2:OFF")) {
        setOut2(false);
        logger_.info("OUT2=OFF " + sourceTag);
        return true;
    }

    if (cmd.startsWith("BUZZER:")) {
        String msText = cmd.substring(7);
        msText.trim();

        unsigned long duration = (unsigned long)msText.toInt();
        beep(duration);

        logger_.info("BUZZER=" + String(duration == 0 ? 120 : duration) + "ms " + sourceTag);
        return true;
    }

    return false;
}