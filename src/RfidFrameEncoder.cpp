#include "RfidFrameEncoder.h"

String RfidFrameEncoder::normalizeHex(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (isxdigit(static_cast<unsigned char>(c))) {
      out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
  }
  return out;
}

String RfidFrameEncoder::reverseByteOrder(const String& hex) {
  if (hex.length() < 2) return hex;

  String out;
  out.reserve(hex.length());

  const int usable = (hex.length() / 2) * 2;
  for (int i = usable - 2; i >= 0; i -= 2) {
    out += hex.substring(i, i + 2);
  }

  if (hex.length() % 2) {
    out += hex.substring(hex.length() - 1);
  }
  return out;
}

String RfidFrameEncoder::toDecimalString(const String& hex) {
  if (hex.isEmpty()) return String();

  unsigned long long value = 0;
  for (size_t i = 0; i < hex.length(); ++i) {
    const char c = hex[i];
    uint8_t nibble = 0;

    if (c >= '0' && c <= '9') nibble = c - '0';
    else if (c >= 'A' && c <= 'F') nibble = 10 + (c - 'A');
    else continue;

    value = (value << 4) | nibble;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", value);
  return String(buf);
}

String RfidFrameEncoder::ct100Frame(const String& rawUid) {
  const String hex = normalizeHex(rawUid);
  const String dec = toDecimalString(hex);

  // Własna, czytelna ramka projektu.
  // Możesz ją łatwo sparsować po stronie programu wagowego.
  String out;
  out.reserve(32 + rawUid.length() + hex.length() + dec.length());
  out += "{CT100}";
  out += "UID=";
  out += rawUid;
  out += ";HEX=";
  out += hex;
  out += ";DEC=";
  out += dec;
  out += "{END}";
  return out;
}

String RfidFrameEncoder::encode(const String& rawUid, Mode mode) {
  const String hex = normalizeHex(rawUid);

  switch (mode) {
    case Mode::RAW:
      return rawUid;
    case Mode::HEX_NORMALIZED:
      return hex;
    case Mode::DECIMAL:
      return toDecimalString(hex);
    case Mode::DECIMAL_LE:
      return toDecimalString(reverseByteOrder(hex));
    case Mode::CT100_FRAME:
      return ct100Frame(rawUid);
    default:
      return rawUid;
  }
}

String RfidFrameEncoder::modeName(Mode mode) {
  switch (mode) {
    case Mode::RAW: return "RAW";
    case Mode::HEX_NORMALIZED: return "HEX";
    case Mode::DECIMAL: return "DEC";
    case Mode::DECIMAL_LE: return "DEC_LE";
    case Mode::CT100_FRAME: return "CT100";
    default: return "?";
  }
}
