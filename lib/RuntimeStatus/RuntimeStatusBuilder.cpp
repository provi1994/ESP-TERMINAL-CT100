#include "RuntimeStatusBuilder.h"

RuntimeStatusBuilder::RuntimeStatusBuilder(
    DeviceConfig& config,
    RuntimeState& state,
    NetManager& net,
    TcpManager& cmdTcp,
    TcpManager& scaleTcp,
    TcpManager& keypadTcp,
    QrTcpBridge& qrBridge,
    FlowManager& flow)
    : cfg_(config),
      rt_(state),
      net_(net),
      cmdTcp_(cmdTcp),
      scaleTcp_(scaleTcp),
      keypadTcp_(keypadTcp),
      qrBridge_(qrBridge),
      flow_(flow) {}

String RuntimeStatusBuilder::jsonEscape(const String& value) {
    String out;
    out.reserve(value.length() + 8);

    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];

        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((uint8_t)c >= 32) out += c;
                break;
        }
    }

    return out;
}

String RuntimeStatusBuilder::normalizeQrLinePrefix(const String& value) const {
    String out = value;
    out.trim();

    if (out.isEmpty()) out = "QR:";

    return out;
}

String RuntimeStatusBuilder::buildWeightForDisplay() const {
    const bool scaleConnected = cfg_.scaleTcp.enabled && (scaleTcp_.isConnected() || scaleTcp_.hasClient());
    const bool freshWeight = (millis() - rt_.lastWeightAt) <= 5000UL;

    if (!cfg_.scaleTcp.enabled) return "WAGA OFF";
    if (!scaleConnected || !freshWeight) return "Brak polaczenia z miernikiem";
    if (rt_.currentWeight.isEmpty()) return "Brak polaczenia z miernikiem";

    return rt_.currentWeight + " KG";
}

String RuntimeStatusBuilder::buildText() {
    const auto bridge = qrBridge_.stats();

    String out;

    out += "device=" + cfg_.network.deviceName + "\n";
    out += "ip=" + net_.localIP().toString() + "\n";
    out += "rfid_last=" + rt_.lastCard + "\n";
    out += "qr_last=" + rt_.qrLastPublished + "\n";
    out += "qr_raw_ascii=" + rt_.qrLastRawAscii + "\n";
    out += "qr_raw_hex=" + rt_.qrLastRawHex + "\n";
    out += "qr_status=" + rt_.qrRuntimeStatus + "\n";
    out += "qr_finalize_reason=" + rt_.qrLastFinalizeReason + "\n";
    out += "qr_current_bytes=" + String(rt_.qrCurrentBytesCount) + "\n";
    out += "qr_last_frame_bytes=" + String(rt_.qrLastFrameBytesCount) + "\n";
    out += "qr_last_byte_at=" + String(rt_.qrLastByteAt) + "\n";
    out += "qr_last_frame_at=" + String(rt_.qrLastFrameAt) + "\n";
    out += "qr_prefix=" + normalizeQrLinePrefix(cfg_.qr.linePrefix) + "\n";
    out += "qr_last_cmd=" + rt_.qrLastCommandHex + "\n";
    out += "qr_last_cmd_status=" + rt_.qrLastCommandStatus + "\n";
    out += "qr_bridge_enabled=" + String(cfg_.qr.tcpBridgeEnabled ? "on" : "off") + "\n";
    out += "qr_bridge_port=" + String(cfg_.qr.tcpBridgePort) + "\n";
    out += "qr_bridge_client=" + String(bridge.clientConnected ? "on" : "off") + "\n";
    out += "qr_bridge_rx_bytes=" + String(bridge.rxBytesFromQr) + "\n";
    out += "qr_bridge_tx_bytes=" + String(bridge.txBytesToQr) + "\n";
    out += "qr_bridge_last_rx_hex=" + bridge.lastRxHex + "\n";
    out += "qr_bridge_last_tx_hex=" + bridge.lastTxHex + "\n";

    out += "key_last=" + rt_.lastKey + "\n";
    out += "key_tcp_enabled=" + String(cfg_.keypad.tcpEnabled ? "on" : "off") + "\n";
    out += "key_tcp_port=" + String(cfg_.keypad.tcpPort) + "\n";
    out += "key_tcp_client=" + String(keypadTcp_.hasClient() ? "on" : "off") + "\n";

    out += "web_code_last=" + rt_.lastWebCode + "\n";
    out += "last_outbound=" + rt_.lastOutboundFrame + "\n";
    out += "last_inbound=" + rt_.lastInboundFrame + "\n";

    out += "cmd_tcp_mode=" + ConfigManager::tcpModeToString(cfg_.tcp.mode) + "\n";
    out += "cmd_tcp_last=" + cmdTcp_.lastMessage() + "\n";

    out += "scale_enabled=" + String(cfg_.scaleTcp.enabled ? "on" : "off") + "\n";
    out += "scale_tcp_mode=" + ConfigManager::tcpModeToString(cfg_.scaleTcp.mode) + "\n";
    out += "scale_display=" + buildWeightForDisplay() + "\n";

    out += "flow_active=" + String(rt_.flow.active ? "on" : "off") + "\n";
    out += "flow_step=" + rt_.flow.currentStep + "\n";
    out += "flow_modules=" + flow_.modulesText() + "\n";

    out += "out1=" + String(rt_.out1State ? "on" : "off") + "\n";
    out += "out2=" + String(rt_.out2State ? "on" : "off") + "\n";
    out += "buzzer=" + String(rt_.buzzerState ? "on" : "off") + "\n";

    return out;
}

String RuntimeStatusBuilder::buildJson() {
    const auto bridge = qrBridge_.stats();

    const bool cmdTcpConnected = cmdTcp_.isConnected() || cmdTcp_.hasClient();
    const bool scaleConnected = cfg_.scaleTcp.enabled && (scaleTcp_.isConnected() || scaleTcp_.hasClient());

    String out;

    out += "{";
    out += "\"ip\":\"" + jsonEscape(net_.localIP().toString()) + "\",";
    out += "\"uptimeMs\":" + String(millis()) + ",";

    out += "\"rfidLast\":\"" + jsonEscape(rt_.lastCard) + "\",";
    out += "\"qrLast\":\"" + jsonEscape(rt_.qrLastPublished) + "\",";
    out += "\"qrLastRawAscii\":\"" + jsonEscape(rt_.qrLastRawAscii) + "\",";
    out += "\"qrLastRawHex\":\"" + jsonEscape(rt_.qrLastRawHex) + "\",";
    out += "\"qrRuntimeStatus\":\"" + jsonEscape(rt_.qrRuntimeStatus) + "\",";
    out += "\"qrLastFinalizeReason\":\"" + jsonEscape(rt_.qrLastFinalizeReason) + "\",";
    out += "\"qrCurrentBytesCount\":" + String(rt_.qrCurrentBytesCount) + ",";
    out += "\"qrLastFrameBytesCount\":" + String(rt_.qrLastFrameBytesCount) + ",";
    out += "\"qrLastByteAt\":" + String(rt_.qrLastByteAt) + ",";
    out += "\"qrLastFrameAt\":" + String(rt_.qrLastFrameAt) + ",";
    out += "\"qrFrameOpen\":" + String(rt_.qrFrameOpen ? "true" : "false") + ",";
    out += "\"qrPrefix\":\"" + jsonEscape(normalizeQrLinePrefix(cfg_.qr.linePrefix)) + "\",";
    out += "\"qrLastCommandHex\":\"" + jsonEscape(rt_.qrLastCommandHex) + "\",";
    out += "\"qrLastCommandStatus\":\"" + jsonEscape(rt_.qrLastCommandStatus) + "\",";

    out += "\"qrBridgeEnabled\":" + String(cfg_.qr.tcpBridgeEnabled ? "true" : "false") + ",";
    out += "\"qrBridgePort\":" + String(cfg_.qr.tcpBridgePort) + ",";
    out += "\"qrBridgeClient\":" + String(bridge.clientConnected ? "true" : "false") + ",";
    out += "\"qrBridgeRxBytes\":" + String(bridge.rxBytesFromQr) + ",";
    out += "\"qrBridgeTxBytes\":" + String(bridge.txBytesToQr) + ",";
    out += "\"qrBridgeLastRxHex\":\"" + jsonEscape(bridge.lastRxHex) + "\",";
    out += "\"qrBridgeLastTxHex\":\"" + jsonEscape(bridge.lastTxHex) + "\",";

    out += "\"keyLast\":\"" + jsonEscape(rt_.lastKey) + "\",";
    out += "\"webCodeLast\":\"" + jsonEscape(rt_.lastWebCode) + "\",";
    out += "\"lastOutbound\":\"" + jsonEscape(rt_.lastOutboundFrame) + "\",";
    out += "\"lastInbound\":\"" + jsonEscape(rt_.lastInboundFrame) + "\",";

    out += "\"cmdTcpConnected\":" + String(cmdTcpConnected ? "true" : "false") + ",";
    out += "\"cmdTcpLast\":\"" + jsonEscape(cmdTcp_.lastMessage()) + "\",";

    out += "\"scaleTcpConnected\":" + String(scaleConnected ? "true" : "false") + ",";
    out += "\"scaleTcpLast\":\"" + jsonEscape(buildWeightForDisplay()) + "\",";

    out += "\"keypadDetected\":" + String(rt_.keypadDetected ? "true" : "false") + ",";
    out += "\"rfidEnabled\":" + String(cfg_.rfid.enabled ? "true" : "false") + ",";
    out += "\"qrEnabled\":" + String(cfg_.qr.enabled ? "true" : "false") + ",";
    out += "\"displayEnabled\":" + String(cfg_.display.enabled ? "true" : "false") + ",";
    out += "\"keypadEnabled\":" + String(cfg_.keypad.enabled ? "true" : "false") + ",";
    out += "\"discoveryEnabled\":" + String(cfg_.discovery.enabled ? "true" : "false") + ",";

    out += "\"out1\":" + String(rt_.out1State ? "true" : "false") + ",";
    out += "\"out2\":" + String(rt_.out2State ? "true" : "false") + ",";
    out += "\"buzzer\":" + String(rt_.buzzerState ? "true" : "false") + ",";

    out += "\"flowActive\":" + String(rt_.flow.active ? "true" : "false") + ",";
    out += "\"flowStep\":\"" + jsonEscape(rt_.flow.currentStep) + "\",";
    out += "\"flowModules\":\"" + jsonEscape(flow_.modulesText()) + "\"";

    out += "}";

    return out;
}