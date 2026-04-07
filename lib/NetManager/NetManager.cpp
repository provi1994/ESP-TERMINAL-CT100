#include "NetManager.h"

NetManager* NetManager::instance_ = nullptr;

NetManager::NetManager(LogManager& logger) : logger_(logger) { instance_ = this; }

bool NetManager::begin(const NetworkSettings& settings) {
  hostname_ = settings.deviceName;
  WiFi.onEvent(NetManager::onEvent);
  ETH.setHostname(hostname_.c_str());

  if (!ETH.begin()) {
    logger_.error("ETH.begin() failed");
    return false;
  }

  if (settings.mode == NetworkMode::STATIC) {
    if (!ETH.config(settings.ip, settings.gateway, settings.subnet, settings.dns1, settings.dns2)) {
      logger_.warn("ETH.config() failed, fallback may occur");
    }
    logger_.info("Static IP requested: " + settings.ip.toString());
  } else {
    logger_.info("DHCP mode requested");
  }
  return true;
}

void NetManager::loop() {}
bool NetManager::isConnected() const { return connected_; }
IPAddress NetManager::localIP() const { return ETH.localIP(); }
String NetManager::hostname() const { return hostname_; }

void NetManager::onEvent(WiFiEvent_t event) {
  if (instance_ != nullptr) {
    instance_->handleEvent(event);
  }
}

void NetManager::handleEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      logger_.info("Ethernet started");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      logger_.info("Ethernet link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      connected_ = true;
      logger_.info("IP: " + ETH.localIP().toString());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      connected_ = false;
      logger_.warn("Ethernet disconnected");
      break;
    case ARDUINO_EVENT_ETH_STOP:
      connected_ = false;
      logger_.warn("Ethernet stopped");
      break;
    default:
      break;
  }
}
