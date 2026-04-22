#include "QrCamGm805Protocol.h"

namespace QrCamGm805Protocol {
String baud9600() { return "7E 00 08 02 00 2A 39 01 A7 EA"; }
String saveFlash() { return "7E 00 09 01 00 00 00 DE C8"; }
String findBaud() { return "7E 00 07 01 00 2A 02 D8 0F"; }
String continuousProfile() { return "7E 00 08 01 00 00 D6 AB CD"; }
String triggerMode() { return "7E 00 08 01 00 02 01 AB CD"; }
String fullAreaAllCodes() { return "7E 00 08 01 00 2C 02 AB CD"; }
String allowCode39() { return "7E 00 08 01 00 36 01 AB CD"; }
}
