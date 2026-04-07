#pragma once

#include <Keypad.h>
#include <functional>
#include "LogManager.h"

class KeypadManager {
 public:
  KeypadManager(LogManager& logger, byte rowPins[4], byte colPins[4]);
  void loop();
  void onKey(std::function<void(char)> callback);

 private:
  LogManager& logger_;
  char keys_[4][4] = {
      {'1', '2', '3', 'A'},
      {'4', '5', '6', 'B'},
      {'7', '8', '9', 'C'},
      {'*', '0', '#', 'D'},
  };
  Keypad keypad_;
  std::function<void(char)> callback_;
};
