#pragma once

#include <Arduino.h>
#include <functional>

#include "AppTypes.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "LogManager.h"

class WorkflowManager {
 public:
  enum class State : uint8_t {
    OFF = 0,
    WAIT_RFID,
    INPUT_FIELD1,
    INPUT_FIELD2,
    SUMMARY,
    RESULT_OK,
    RESULT_ERROR
  };

  WorkflowManager(LogManager& logger, DisplayManager& display);

  void begin(const WorkflowSettings& settings);
  void updateConfig(const WorkflowSettings& settings);

  void setSender(std::function<bool(const String&)> sender);
  void setCurrentWeight(const String& weight);

  bool isEnabled() const;
  void onCard(const String& encoded);
  void onKey(char key);
  void loop();
  void render(const String& header);

  String runtimeJson() const;
  String stateName() const;
  String lastOutboundFrame() const;

 private:
  LogManager& logger_;
  DisplayManager& display_;
  WorkflowSettings settings_;
  std::function<bool(const String&)> sender_;

  String currentWeight_ = "---";
  bool active_ = false;
  uint32_t sessionCounter_ = 0;
  uint32_t sessionId_ = 0;
  State state_ = State::OFF;
  State returnStateAfterMessage_ = State::WAIT_RFID;
  unsigned long stateSince_ = 0;

  String cardUid_;
  String field1_;
  String field2_;
  String messageLine1_;
  String messageLine2_;
  String lastOutboundFrame_;

  void setState(State state);
  void resetToEntryState();
  void startSession();
  State entryStateFromConfig() const;

  bool field1Enabled() const;
  bool field2Enabled() const;
  bool validateValue(const String& value, const WorkflowFieldSettings& field) const;

  String* activeValue();
  const WorkflowFieldSettings* activeFieldConfig() const;
  bool isInputKey(char key) const;

  void advanceAfterCard();
  void advanceAfterField1();
  void advanceAfterField2();
  void finalizeOrShowSummary();

  bool sendStepIfNeeded(const String& step, const String& value);
  bool sendFinal();

  String buildStepFrame(const String& step, const String& value) const;
  String buildFinalFrame() const;

  void setMessage(State messageState,
                  const String& line1,
                  const String& line2,
                  State returnState);

  static String jsonEscape(const String& value);
};