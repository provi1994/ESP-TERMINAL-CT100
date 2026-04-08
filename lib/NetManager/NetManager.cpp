#include "NetManager.h"

NetManager* NetManager::instance_ = nullptr;

NetManager::NetManager(LogManager& logger) : logger_(logger) {
  instance_ = this;
}

bool NetManager::begin(const NetworkSettings& settings) {
  hostname_ = settings.deviceName;
  WiFi.onEvent(NetManager::onEvent);

  delay(500);

  if (!ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN)) {
    logger_.error("ETH.begin() failed");
    return false;
  }

  ETH.setHostname(hostname_.c_str());

  if (settings.mode == NetworkMode::STATIC) {
    if (!ETH.config(settings.ip, settings.gateway, settings.subnet, settings.dns1, settings.dns2)) {
      logger_.error("ETH static IP config failed");
      return false;
    }
    logger_.info(
      "Ethernet static IP configured: " + settings.ip.toString() +
      " gw=" + settings.gateway.toString() +
      " mask=" + settings.subnet.toString()
    );
  } else {
    logger_.info("Ethernet DHCP mode enabled");
  }

  logger_.info("Ethernet init started");
  return true;
}

bool NetManager::isConnected() const {
  return ethConnected_ && ETH.linkUp();
}

IPAddress NetManager::localIP() const {
  return ETH.localIP();
}

void NetManager::loop() {
}

void NetManager::onEvent(WiFiEvent_t event) {
  if (!instance_) return;

  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      instance_->logger_.info("ETH START");
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      instance_->logger_.info("ETH CONNECTED");
      instance_->ethConnected_ = true;
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      instance_->logger_.info("ETH GOT IP: " + ETH.localIP().toString());
      instance_->ethConnected_ = true;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      instance_->logger_.warn("ETH DISCONNECTED");
      instance_->ethConnected_ = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      instance_->logger_.warn("ETH STOP");
      instance_->ethConnected_ = false;
      break;

    default:
      break;
  }
}
