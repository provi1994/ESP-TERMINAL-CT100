#pragma once

#include <Arduino.h>

enum class BootScreenPhase : uint8_t {
    LOGO = 0,
    MODULES = 1,
    TCP = 2,
    DONE = 3
};

struct FlowRuntime {
    bool active = false;
    bool completed = false;
    bool rfidDone = false;
    bool keypadDone = false;
    bool qrDone = false;

    unsigned long startedAt = 0;
    unsigned long screenUntil = 0;
    unsigned long remoteSummaryWaitUntil = 0;

    String currentStep = "IDLE";
    String summary = "";
    String remoteSummaryText = "";

    bool startedByWeight = false;
    bool waitingForRemoteSummary = false;
};

struct RuntimeState {
    unsigned long lastStatusRefresh = 0;

    String lastCard;
    String lastKey;
    String lastOutboundFrame;
    String lastInboundFrame;
    String lcdCustomText;
    unsigned long lcdCustomUntil = 0;

    String remoteStatusText = "Oczekuje na wazenie";
    unsigned long remoteStatusUntil = 0;

    String currentWeight = "---";
    unsigned long lastWeightAt = 0;

    String lastWebCode;
    String productCodeBuffer;

    String qrLastPublished;
    String qrLastCommandHex;
    String qrLastCommandStatus;
    String qrLastRawAscii;
    String qrLastRawHex;
    String qrRuntimeStatus;
    String qrLastFinalizeReason;

    uint32_t qrLastByteAt = 0;
    uint32_t qrLastFrameAt = 0;
    uint16_t qrCurrentBytesCount = 0;
    uint16_t qrLastFrameBytesCount = 0;
    bool qrFrameOpen = false;

    bool out1State = false;
    bool out2State = false;
    bool buzzerState = false;
    unsigned long buzzerOffAt = 0;

    unsigned long bootSequenceStart = 0;
    BootScreenPhase bootPhase = BootScreenPhase::DONE;
    bool keypadDetected = false;
    String rfidStatus = "OFF";

    FlowRuntime flow;
};