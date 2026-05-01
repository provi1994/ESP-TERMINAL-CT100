#include "FlowManager.h"

FlowManager::FlowManager(LogManager& logger, RuntimeState& state, DeviceConfig& config)
    : logger_(logger), rt_(state), cfg_(config) {}

void FlowManager::start() {
    rt_.flow.active = true;
    rt_.flow.completed = false;
    rt_.flow.rfidDone = !cfg_.rfid.enabled;
    rt_.flow.keypadDone = !cfg_.keypad.enabled;
    rt_.flow.qrDone = !cfg_.qr.enabled;
    rt_.flow.startedAt = millis();
    rt_.flow.screenUntil = 0;
    rt_.flow.summary = "";

    updateStep();
    logger_.info("Flow started");
}

void FlowManager::cancel() {
    rt_.flow.active = false;
    rt_.flow.completed = false;
    rt_.flow.currentStep = "IDLE";
    rt_.flow.summary = "anulowano";

    logger_.warn("Flow cancelled");
}

void FlowManager::updateStep() {
    if (!rt_.flow.active) {
        rt_.flow.currentStep = "IDLE";
        return;
    }

    if (cfg_.rfid.enabled && !rt_.flow.rfidDone) {
        rt_.flow.currentStep = "RFID";
        return;
    }

    if (cfg_.keypad.enabled && !rt_.flow.keypadDone) {
        rt_.flow.currentStep = "KEYPAD";
        return;
    }

    if (cfg_.qr.enabled && !rt_.flow.qrDone) {
        rt_.flow.currentStep = "QR";
        return;
    }

    rt_.flow.currentStep = "SUMMARY";
    rt_.flow.completed = true;
}

void FlowManager::markRfidDone() {
    if (!rt_.flow.active) return;
    rt_.flow.rfidDone = true;
    updateStep();
}

void FlowManager::markKeypadDone() {
    if (!rt_.flow.active) return;
    rt_.flow.keypadDone = true;
    updateStep();
}

void FlowManager::markQrDone() {
    if (!rt_.flow.active) return;
    rt_.flow.qrDone = true;
    updateStep();
}

bool FlowManager::active() const {
    return rt_.flow.active;
}

bool FlowManager::completed() const {
    return rt_.flow.completed;
}

String FlowManager::currentStep() const {
    return rt_.flow.currentStep;
}

String FlowManager::modulesText() const {
    String out;

    if (cfg_.rfid.enabled) out += "RFID ";
    if (cfg_.keypad.enabled) out += "KEYPAD ";
    if (cfg_.qr.enabled) out += "QR ";

    out.trim();
    if (out.isEmpty()) out = "BRAK";

    return out;
}