#include <Arduino.h>
#include <ETH.h>
#include <ArduinoOTA.h>

static bool eth_connected = false;
const char* deviceName = "TAMTRON_S.A_-_Terminal_CT-100";

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("Ethernet start");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("Ethernet connected");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      eth_connected = true;
      Serial.print("Nazwa: ");
      Serial.println(deviceName);
      Serial.print("IP: ");
      Serial.println(ETH.localIP());
      Serial.print("Hostname: ");
      Serial.println(ETH.getHostname());
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      eth_connected = false;
      Serial.println("Ethernet disconnected");
      break;

    case ARDUINO_EVENT_ETH_STOP:
      eth_connected = false;
      Serial.println("Ethernet stopped");
      break;

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.onEvent(WiFiEvent);

  ETH.setHostname(deviceName);
  ETH.begin();

  while (!eth_connected) {
    delay(100);
  }

  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword("moje_haslo_ota");
  ArduinoOTA.setPort(3232);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA end");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error: %u\n", error);
  });

  ArduinoOTA.begin();

  Serial.println("OTA gotowe");
}

void loop() {
  ArduinoOTA.handle();
}