#include "WorkflowManager.h"

WorkflowManager::WorkflowManager(LogManager& logger, DisplayManager& display)
    : logger_(logger), display_(display) {}

void WorkflowManager::begin(const WorkflowSettings& settings) {
  settings_ = settings;
  resetToEntryState();
}

void WorkflowManager::updateConfig(const WorkflowSettings& settings) {
  settings_ = settings;
  resetToEntryState();
}

void WorkflowManager::setSender(std::function<bool(const String&)> sender) {
  sender_ = sender;
}

void WorkflowManager::setCurrentWeight(const String& weight) {
  currentWeight_ = weight.isEmpty() ? String("---") : weight;
}

bool WorkflowManager::isEnabled() const {
  return settings_.enabled;
}

void WorkflowManager::setState(State state) {
  state_ = state;
  stateSince_ = millis();
}

bool WorkflowManager::field1Enabled() const {
  return settings_.field1.enabled;
}

bool WorkflowManager::field2Enabled() const {
  return settings_.field2.enabled;
}

WorkflowManager::State WorkflowManager::entryStateFromConfig() const {
  if (!settings_.enabled) return State::OFF;
  if (settings_.requireRfid) return State::WAIT_RFID;
  if (field1Enabled()) return State::INPUT_FIELD1;
  if (field2Enabled()) return State::INPUT_FIELD2;
  return State::SUMMARY;
}

void WorkflowManager::resetToEntryState() {
  active_ = false;
  sessionId_ = 0;
  cardUid_ = "";
  field1_ = "";
  field2_ = "";
  messageLine1_ = "";
  messageLine2_ = "";
  lastOutboundFrame_ = "";
  setState(entryStateFromConfig());
}

void WorkflowManager::startSession() {
  active_ = true;
  sessionId_ = ++sessionCounter_;
  cardUid_ = "";
  field1_ = "";
  field2_ = "";
  messageLine1_ = "";
  messageLine2_ = "";
  setState(entryStateFromConfig());
}

bool WorkflowManager::validateValue(const String& value, const WorkflowFieldSettings& field) const {
  if (value.length() < field.minLen) return false;
  if (field.maxLen > 0 && value.length() > field.maxLen) return false;
  return true;
}

String* WorkflowManager::activeValue() {
  if (state_ == State::INPUT_FIELD1) return &field1_;
  if (state_ == State::INPUT_FIELD2) return &field2_;
  return nullptr;
}

const WorkflowFieldSettings* WorkflowManager::activeFieldConfig() const {
  if (state_ == State::INPUT_FIELD1) return &settings_.field1;
  if (state_ == State::INPUT_FIELD2) return &settings_.field2;
  return nullptr;
}

bool WorkflowManager::isInputKey(char key) const {
  return (key >= '0' && key <= '9') || (key >= 'A' && key <= 'D');
}

String WorkflowManager::buildStepFrame(const String& step, const String& value) const {
  String out;
  out += "SESSION:" + String(sessionId_);
  out += ";STEP:" + step;
  out += ";VALUE:" + value;
  return out;
}

String WorkflowManager::buildFinalFrame() const {
  String out;
  out += "SESSION:" + String(sessionId_);
  if (!cardUid_.isEmpty()) out += ";CARD:" + cardUid_;
  if (field1Enabled()) out += ";FIELD1:" + field1_;
  if (field2Enabled()) out += ";FIELD2:" + field2_;
  if (settings_.sendWeight) out += ";WEIGHT:" + currentWeight_;
  return out;
}

bool WorkflowManager::sendStepIfNeeded(const String& step, const String& value) {
  if (settings_.sendMode != WorkflowSendMode::SINGLE) return true;
  if (!sender_) {
    logger_.error("Workflow sender not set");
    return false;
  }

  const String payload = buildStepFrame(step, value);
  const bool ok = sender_(payload);
  if (ok) {
    lastOutboundFrame_ = payload;
    logger_.info("Workflow step sent: " + payload);
  } else {
    logger_.error("Workflow step send failed");
  }
  return ok;
}

bool WorkflowManager::sendFinal() {
  if (!sender_) {
    logger_.error("Workflow sender not set");
    return false;
  }

  const String payload = buildFinalFrame();
  const bool ok = sender_(payload);
  if (ok) {
    lastOutboundFrame_ = payload;
    logger_.info("Workflow final sent: " + payload);
  } else {
    logger_.error("Workflow final send failed");
  }
  return ok;
}

void WorkflowManager::setMessage(State messageState,
                                 const String& line1,
                                 const String& line2,
                                 State returnState) {
  messageLine1_ = line1;
  messageLine2_ = line2;
  returnStateAfterMessage_ = returnState;
  setState(messageState);
}

void WorkflowManager::advanceAfterCard() {
  if (field1Enabled()) {
    setState(State::INPUT_FIELD1);
    return;
  }
  if (field2Enabled()) {
    setState(State::INPUT_FIELD2);
    return;
  }
  finalizeOrShowSummary();
}

void WorkflowManager::advanceAfterField1() {
  if (field2Enabled()) {
    setState(State::INPUT_FIELD2);
    return;
  }
  finalizeOrShowSummary();
}

void WorkflowManager::advanceAfterField2() {
  finalizeOrShowSummary();
}

void WorkflowManager::finalizeOrShowSummary() {
  if (settings_.showSummary) {
    setState(State::SUMMARY);
    return;
  }

  if (sendFinal()) {
    setMessage(State::RESULT_OK, "Dane wyslane", "TCP OK", entryStateFromConfig());
  } else {
    setMessage(State::RESULT_ERROR, "Blad wysylki", "TCP final", State::SUMMARY);
  }
}

void WorkflowManager::onCard(const String& encoded) {
  if (!settings_.enabled) return;

  if (!active_) startSession();
  if (state_ != State::WAIT_RFID) return;

  cardUid_ = encoded;
  if (!sendStepIfNeeded("RFID", cardUid_)) {
    setMessage(State::RESULT_ERROR, "Blad wysylki", "RFID step", State::WAIT_RFID);
    return;
  }

  advanceAfterCard();
}

void WorkflowManager::onKey(char key) {
  if (!settings_.enabled) return;

  if (!active_) startSession();

  if (key == 'C') {
    setMessage(State::RESULT_OK, "Sesja anulowana", "Powrot", entryStateFromConfig());
    return;
  }

  if (state_ == State::SUMMARY) {
    if (key == '#') {
      if (sendFinal()) {
        setMessage(State::RESULT_OK, "Dane wyslane", "TCP OK", entryStateFromConfig());
      } else {
        setMessage(State::RESULT_ERROR, "Blad wysylki", "TCP final", State::SUMMARY);
      }
    }
    return;
  }

  String* target = activeValue();
  const WorkflowFieldSettings* field = activeFieldConfig();
  if (!target || !field) return;

  if (key == '*') {
    if (!target->isEmpty()) {
      target->remove(target->length() - 1);
    }
    return;
  }

  if (key == '#') {
    if (!validateValue(*target, *field)) {
      setMessage(State::RESULT_ERROR, "Bledna wartosc", "Sprawdz dlugosc", state_);
      return;
    }

    if (state_ == State::INPUT_FIELD1) {
      if (!sendStepIfNeeded("FIELD1", field1_)) {
        setMessage(State::RESULT_ERROR, "Blad wysylki", "FIELD1 step", State::INPUT_FIELD1);
        return;
      }
      advanceAfterField1();
      return;
    }

    if (state_ == State::INPUT_FIELD2) {
      if (!sendStepIfNeeded("FIELD2", field2_)) {
        setMessage(State::RESULT_ERROR, "Blad wysylki", "FIELD2 step", State::INPUT_FIELD2);
        return;
      }
      advanceAfterField2();
      return;
    }

    return;
  }

  if (!isInputKey(key)) return;
  if (field->maxLen > 0 && target->length() >= field->maxLen) return;

  *target += key;
}

void WorkflowManager::loop() {
  if (!settings_.enabled) return;

  if ((state_ == State::RESULT_OK || state_ == State::RESULT_ERROR) &&
      (millis() - stateSince_ > 1500UL)) {
    if (state_ == State::RESULT_OK) {
      resetToEntryState();
    } else {
      setState(returnStateAfterMessage_);
    }
  }
}

void WorkflowManager::render(const String& header) {
  switch (state_) {
    case State::OFF:
      display_.showIdleWeight(header, currentWeight_, "Workflow OFF");
      break;

    case State::WAIT_RFID:
      display_.showRfidPrompt("RFID", "Prosze odbic", "karte RFID");
      break;

    case State::INPUT_FIELD1:
      display_.showInputScreen(settings_.field1.label, field1_, "#=OK *=DEL C=ESC");
      break;

    case State::INPUT_FIELD2:
      display_.showInputScreen(settings_.field2.label, field2_, "#=OK *=DEL C=ESC");
      break;

    case State::SUMMARY:
      display_.showSummaryScreen(
          "K:" + cardUid_,
          "F1:" + field1_,
          "F2:" + field2_,
          "W:" + currentWeight_);
      break;

    case State::RESULT_OK:
    case State::RESULT_ERROR:
      display_.showResultScreen(
          state_ == State::RESULT_OK ? "Workflow" : "Blad",
          messageLine1_,
          messageLine2_);
      break;
  }
}

String WorkflowManager::stateName() const {
  switch (state_) {
    case State::OFF: return "off";
    case State::WAIT_RFID: return "wait_rfid";
    case State::INPUT_FIELD1: return "input_field1";
    case State::INPUT_FIELD2: return "input_field2";
    case State::SUMMARY: return "summary";
    case State::RESULT_OK: return "result_ok";
    case State::RESULT_ERROR: return "result_error";
    default: return "unknown";
  }
}

String WorkflowManager::lastOutboundFrame() const {
  return lastOutboundFrame_;
}

String WorkflowManager::jsonEscape(const String& value) {
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
      default: out += c; break;
    }
  }
  return out;
}

String WorkflowManager::runtimeJson() const {
  String out;
  out.reserve(512);
  out += "{";
  out += "\"enabled\":" + String(settings_.enabled ? "true" : "false") + ",";
  out += "\"active\":" + String(active_ ? "true" : "false") + ",";
  out += "\"state\":\"" + stateName() + "\",";
  out += "\"sessionId\":" + String(sessionId_) + ",";
  out += "\"sendMode\":\"" + ConfigManager::workflowSendModeToString(settings_.sendMode) + "\",";
  out += "\"card\":\"" + jsonEscape(cardUid_) + "\",";
  out += "\"field1\":\"" + jsonEscape(field1_) + "\",";
  out += "\"field2\":\"" + jsonEscape(field2_) + "\",";
  out += "\"weight\":\"" + jsonEscape(currentWeight_) + "\",";
  out += "\"lastOutbound\":\"" + jsonEscape(lastOutboundFrame_) + "\"";
  out += "}";
  return out;
}