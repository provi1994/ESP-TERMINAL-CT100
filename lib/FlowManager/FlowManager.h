#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "LogManager.h"
#include "RuntimeState.h"

class FlowManager {
public:
    FlowManager(LogManager& logger, RuntimeState& state, DeviceConfig& config);

    void start();
    void cancel();
    void updateStep();

    void markRfidDone();
    void markKeypadDone();
    void markQrDone();

    bool active() const;
    bool completed() const;
    String currentStep() const;
    String modulesText() const;

private:
    LogManager& logger_;
    RuntimeState& rt_;
    DeviceConfig& cfg_;
};