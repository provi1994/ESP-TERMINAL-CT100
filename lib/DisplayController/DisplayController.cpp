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

    const uint8_t animFrame = (millis() / 250UL) % 4;

    if (rt_.flow.currentStep == "RFID") {
        display_.showDriverHint(
            "KROK 1 / RFID",
            "PRZYLOZ",
            "KARTE",
            DriverHintIcon::RFID,
            animFrame);

        return;
    }

    if (rt_.flow.currentStep == "KEYPAD") {
        String value = rt_.lastWebCode.isEmpty() ? "WPISZ KOD" : rt_.lastWebCode;

        display_.showDriverHint(
            "KROK 2 / KOD",
            DisplayManager::fit(value, 13),
            "NA KLAWIAT.",
            DriverHintIcon::KEYPAD,
            animFrame);

        return;
    }

    if (rt_.flow.currentStep == "QR") {
        display_.showDriverHint(
            "KROK 3 / QR",
            "ZESKANUJ",
            "KOD QR",
            DriverHintIcon::QR,
            animFrame);

        return;
    }

    if (rt_.flow.currentStep == "SUMMARY") {
        display_.showDriverHint(
            "PODSUMOWANIE",
            "TRWA",
            "ZAPIS...",
            DriverHintIcon::PROCESSING,
            animFrame);

        return;
    }
}

void DisplayController::renderIdleScreen() {
    if (!cfg_.display.enabled) return;

    const uint8_t animFrame = (millis() / 300UL) % 4;

    if (rt_.lcdCustomUntil > millis()) {
        display_.showTcp(rt_.lcdCustomText);
        return;
    }

    String weight = buildWeightForDisplay();

    if (weight == "Brak polaczenia z miernikiem") {
        display_.showDriverHint(
            "WAGA",
            "BRAK",
            "POLACZENIA",
            DriverHintIcon::ERROR_ICON,
            animFrame);

        return;
    }

    if (!cfg_.scaleTcp.enabled) {
        display_.showDriverHint(
            "WAGA",
            "WAGA TCP",
            "WYLACZONA",
            DriverHintIcon::ERROR_ICON,
            animFrame);

        return;
    }

    // Ekran gotowości: kierowca ma zacząć od RFID.
    // Jeżeli później chcesz inaczej, możemy warunkować po konfiguracji flow.
    display_.showDriverHint(
        "GOTOWY",
        "ZBLIZ",
        "KARTE",
        DriverHintIcon::RFID,
        animFrame);
}

void DisplayController::loop() {
    if (!cfg_.display.enabled) return;

    // Dla animacji 250 ms jest OK. 
    // Ekran będzie miał 4 klatki na sekundę.
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