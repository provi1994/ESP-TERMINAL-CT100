#pragma once

#include <Arduino.h>
#include <IPAddress.h>

enum class NetworkMode : uint8_t { DHCP = 0, STATIC = 1 };
enum class TcpMode : uint8_t { CLIENT = 0, HOST = 1, SERVER = 2 };
enum class RfidEncoding : uint8_t { HEX_MODE = 0, DEC_MODE = 1, RAW_MODE = 2 };

struct NetworkSettings {
  String deviceName = "CT-100";
  NetworkMode mode = NetworkMode::DHCP;
  IPAddress ip = IPAddress(192, 168, 1, 100);
  IPAddress gateway = IPAddress(192, 168, 1, 1);
  IPAddress subnet = IPAddress(255, 255, 255, 0);
  IPAddress dns1 = IPAddress(1, 1, 1, 1);
  IPAddress dns2 = IPAddress(8, 8, 8, 8);
};

struct TcpSettings {
  TcpMode mode = TcpMode::CLIENT;
  String serverIp = "192.168.1.10";
  uint16_t serverPort = 5000;
  uint16_t listenPort = 5000;
};

struct SecuritySettings {
  String webUser = "admin";
  String webPassword = "admin";
  String otaPassword = "moje_haslo_ota";
};

struct RfidSettings {
  RfidEncoding encoding = RfidEncoding::HEX_MODE;
  uint32_t baudRate = 9600;
  bool enabled = true;
};

struct DisplaySettings {
  bool enabled = true;
  uint8_t contrast = 180;
};

struct KeypadSettings {
  bool enabled = true;
  uint8_t pcf8574Address = 0x20;
};

struct DeviceConfig {
  NetworkSettings network;
  TcpSettings tcp;
  SecuritySettings security;
  RfidSettings rfid;
  DisplaySettings display;
  KeypadSettings keypad;
};
