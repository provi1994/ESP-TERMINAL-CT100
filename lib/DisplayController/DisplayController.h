#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "DisplayManager.h"
#include "FlowManager.h"
#include "NetManager.h"
#include "RuntimeState.h"
#include "TcpManager.h"

class DisplayController {
public:
    DisplayController(
        DisplayManager& display,
        DeviceConfig& config,
        RuntimeState& state,
        NetManager& net,
        TcpManager& scaleTcp,
        FlowManager& flow);

    void begin();
    void startBootSequence();
    void loop();

    String buildWeightForDisplay() const;

private:
    DisplayManager& display_;
    DeviceConfig& cfg_;
    RuntimeState& rt_;
    NetManager& net_;
    TcpManager& scaleTcp_;
    FlowManager& flow_;

    String activeHeaderText() const;
    String ipText() const;
    String tcpModeLabel(TcpMode mode) const;

    void renderBootSequence();
    void renderFlowScreen();
    void renderIdleScreen();
};