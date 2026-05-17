#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "LogManager.h"
#include "RuntimeState.h"

class FlowManager {
public:
    FlowManager(LogManager& logger, RuntimeState& state, DeviceConfig& config);

    void start();
    void startFromWeight(uint32_t weightKg);
    void cancel();
    void loop();

    void onWeightUpdate(uint32_t weightKg);
    void setRemoteSummary(const String& text);

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

    bool weightArmed_ = true;
    unsigned long lastBelowThresholdAt_ = 0;

    void resetFlowRuntime();
    void goToFirstInputStep();
    void goToNextInputStep();
    void goToSummaryDecision();
    void finishFlow();

    bool screenExpired() const;
    uint16_t resultMs() const;
    uint16_t summaryMs() const;
    uint16_t remoteSummaryWaitMs() const;
};