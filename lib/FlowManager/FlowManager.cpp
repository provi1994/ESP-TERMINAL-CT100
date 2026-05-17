#include "FlowManager.h"
    rt_.flow.remoteSummaryText = clean;

    if (rt_.flow.active && rt_.flow.currentStep == "WAIT_REMOTE_SUMMARY") {
        rt_.flow.currentStep = "SUMMARY_REMOTE";
        rt_.flow.screenUntil = millis() + summaryMs();
        rt_.flow.waitingForRemoteSummary = false;
    }

    logger_.info("Remote summary received: " + clean);
}

void FlowManager::markRfidDone() {
    if (!rt_.flow.active) return;
    if (rt_.flow.currentStep != "RFID") return;

    rt_.flow.rfidDone = true;
    rt_.flow.currentStep = "RFID_OK";
    rt_.flow.screenUntil = millis() + resultMs();
}

void FlowManager::markKeypadDone() {
    if (!rt_.flow.active) return;
    if (rt_.flow.currentStep != "KEYPAD") return;

    rt_.flow.keypadDone = true;
    rt_.flow.currentStep = "KEYPAD_OK";
    rt_.flow.screenUntil = millis() + resultMs();
}

void FlowManager::markQrDone() {
    if (!rt_.flow.active) return;
    if (rt_.flow.currentStep != "QR") return;

    rt_.flow.qrDone = true;
    rt_.flow.currentStep = "QR_OK";
    rt_.flow.screenUntil = millis() + resultMs();
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