#include "DisplayController.h"

DisplayController::DisplayController(
    DisplayManager& display,
    DeviceConfig& config,
    RuntimeState& state,
    NetManager& net,
    TcpManager& scaleTcp,
    FlowManager& flow)
    : display_(display),
      cfg_(config),
      rt_(state),
      net_(net),
      scaleTcp_(scaleTcp),
      flow_(flow) {}

void DisplayController::begin() {
    if (!cfg_.display.enabled) return;

    display_.begin(cfg_.display.contrast);
    display_.showLogo();
    startBootSequence();
}

void DisplayController::startBootSequence() {
    rt_.bootSequenceStart = millis();
    rt_.bootPhase = BootScreenPhase::LOGO;
}

String DisplayController::activeHeaderText() const {
    if (rt_.remoteStatusUntil > millis() && !rt_.remoteStatusText.isEmpty()) {
        return rt_.remoteStatusText;
    }

    return "Oczekuje na wazenie";
}

String DisplayController::tcpModeLabel(TcpMode mode) const {
    switch (mode) {
        case TcpMode::CLIENT: return "CLIENT";
        case TcpMode::HOST: return "HOST";
        case TcpMode::SERVER: return "SERVER";
        default: return "?";
    }
}

String DisplayController::ipText() const {
    IPAddress ip = net_.localIP();

    if (ip == IPAddress((uint32_t)0)) {
        return "0.0.0.0";
    }

    return ip.toString();
}

String DisplayController::buildWeightForDisplay() const {
    const bool scaleConnected = cfg_.scaleTcp.enabled && (scaleTcp_.isConnected() || scaleTcp_.hasClient());
    const bool freshWeight = (millis() - rt_.lastWeightAt) <= 5000UL;

    if (!cfg_.scaleTcp.enabled) return "WAGA OFF";
    if (!scaleConnected || !freshWeight) return "Brak polaczenia z miernikiem";
    if (rt_.currentWeight.isEmpty()) return "Brak polaczenia z miernikiem";

    return rt_.currentWeight + " KG";
}

void DisplayController::renderBootSequence() {
    if (!cfg_.display.enabled) return;

    const unsigned long elapsed = millis() - rt_.bootSequenceStart;

    if (elapsed < 3000UL) {
        rt_.bootPhase = BootScreenPhase::LOGO;
        display_.showLogo();
        return;
    }

    if (elapsed < 6500UL) {
        rt_.bootPhase = BootScreenPhase::MODULES;

        display_.showInfo(
            "START / MODULY",
            "RFID: " + rt_.rfidStatus,
            cfg_.keypad.enabled ? ("KEYPAD: " + String(rt_.keypadDetected ? "OK" : "BRAK")) : "KEYPAD: OFF",
            String("QR: ") + (cfg_.qr.enabled ? "ON" : "OFF"),
            "IP: " + ipText());

        return;
    }

    if (elapsed < 10000UL) {
        rt_.bootPhase = BootScreenPhase::TCP;

        display_.showInfo(
            "START / TCP",
            "CMD: " + tcpModeLabel(cfg_.tcp.mode),
            "WAGA: " + String(cfg_.scaleTcp.enabled ? tcpModeLabel(cfg_.scaleTcp.mode) : "OFF"),
            "FLOW: " + String(cfg_.display.flow.enabled ? "ON" : "OFF"),
            "MOD: " + flow_.modulesText());

        return;
    }

    rt_.bootPhase = BootScreenPhase::DONE;
}

void DisplayController::renderFlowScreen() {
    if (!cfg_.display.enabled || !rt_.flow.active) return;

    if (rt_.flow.currentStep == "RFID") {
        display_.showRfidPrompt("FLOW / RFID", "Zbliz karte", "Czekam na odczyt");
        return;
    }

    if (rt_.flow.currentStep == "KEYPAD") {
        display_.showInputScreen(
            "FLOW / KEY",
            rt_.lastWebCode.isEmpty() ? "----" : rt_.lastWebCode,
            "Wprowadz kod");

        return;
    }

    if (rt_.flow.currentStep == "QR") {
        display_.showInputScreen(
            "FLOW / QR",
            rt_.qrLastPublished.isEmpty() ? "SCAN" : rt_.qrLastPublished,
            "Zeskanuj kod QR");

        return;
    }

    if (rt_.flow.currentStep == "SUMMARY") {
        display_.showSummaryScreen(
            String("RFID: ") + (rt_.lastCard.isEmpty() ? "-" : rt_.lastCard),
            String("KEY: ") + (rt_.lastWebCode.isEmpty() ? rt_.lastKey : rt_.lastWebCode),
            String("QR: ") + (rt_.qrLastPublished.isEmpty() ? "-" : rt_.qrLastPublished),
            buildWeightForDisplay());

        return;
    }
}

void DisplayController::renderIdleScreen() {
    if (!cfg_.display.enabled) return;

    if (rt_.lcdCustomUntil > millis()) {
        display_.showTcp(rt_.lcdCustomText);
        return;
    }

    display_.showIdleWeight(
        activeHeaderText(),
        buildWeightForDisplay(),
        "Zbliz karte RFID");
}

void DisplayController::loop() {
    if (!cfg_.display.enabled) return;

    if (millis() - rt_.lastStatusRefresh <= 250UL) {
        return;
    }

    rt_.lastStatusRefresh = millis();

    if (rt_.bootPhase != BootScreenPhase::DONE) {
        renderBootSequence();
        return;
    }

    if (rt_.flow.active) {
        renderFlowScreen();
        return;
    }

    renderIdleScreen();
}