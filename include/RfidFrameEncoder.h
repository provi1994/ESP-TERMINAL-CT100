#pragma once

#include <Arduino.h>

// Bezpieczny enkoder UID -> ramka wyjściowa.
// Nie odtwarza zewnętrznych, nieudokumentowanych formatów.
class RfidFrameEncoder {
public:
  enum class Mode : uint8_t {
    RAW = 0,            // oryginalny tekst z czytnika
    HEX_NORMALIZED = 1, // tylko znaki HEX, uppercase
    DECIMAL = 2,        // UID HEX -> liczba dziesiętna
    DECIMAL_LE = 3,     // UID HEX po odwróceniu kolejności bajtów -> dziesiętna
    CT100_FRAME = 4     // własna ramka tekstowa projektu
  };

  static String encode(const String& rawUid, Mode mode);
  static String modeName(Mode mode);

private:
  static String normalizeHex(const String& in);
  static String toDecimalString(const String& hex);
  static String reverseByteOrder(const String& hex);
  static String ct100Frame(const String& rawUid);
};
