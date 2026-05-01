#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "ConfigManager.h"
#include "FlowManager.h"
#include "NetManager.h"
#include "QrTcpBridge.h"
#include "RuntimeState.h"
#include "TcpManager.h"

class RuntimeStatusBuilder {
public:
    RuntimeStatusBuilder(
        DeviceConfig& config,
        RuntimeState& state,
        NetManager& net,
        TcpManager& cmdTcp,
        TcpManager& scaleTcp,
        TcpManager& keypadTcp,
        QrTcpBridge& qrBridge,
        FlowManager& flow);

    String buildText();
    String buildJson();

private:
    DeviceConfig& cfg_;
    RuntimeState& rt_;
    NetManager& net_;
    TcpManager& cmdTcp_;
    TcpManager& scaleTcp_;
    TcpManager& keypadTcp_;
    QrTcpBridge& qrBridge_;
    FlowManager& flow_;

    static String jsonEscape(const String& value);
    String normalizeQrLinePrefix(const String& value) const;
    String buildWeightForDisplay() const;
};