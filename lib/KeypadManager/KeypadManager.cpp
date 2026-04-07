#include "KeypadManager.h"

KeypadManager::KeypadManager(LogManager& logger, byte rowPins[4], byte colPins[4])
    : logger_(logger), keypad_(makeKeymap(keys_), rowPins, colPins, 4, 4) {}

void KeypadManager::onKey(std::function<void(char)> callback) { callback_ = callback; }

void KeypadManager::loop() {
  char key = keypad_.getKey();
  if (key != NO_KEY) {
    logger_.info(String("Keypad key: ") + key);
    if (callback_) callback_(key);
  }
}
