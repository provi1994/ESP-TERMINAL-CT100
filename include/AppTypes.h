#pragma once
#include <Arduino.h>
#include <IPAddress.h>

enum class NetworkMode : uint8_t { DHCP = 0, STATIC = 1 };
enum class TcpMode : uint8_t { CLIENT = 0, HOST = 1, SERVER = 2 };
enum class RfidEncoding : uint8_t { HEX_MODE = 0, DEC_MODE = 1, RAW_MODE = 2, SCALE_FRAME_MODE = 3 };

struct NetworkSettings {
    String deviceName = "ct100-terminal";
    NetworkMode mode = NetworkMode::DHCP;
    IPAddress ip = IPAddress(192, 168, 1, 112);
    IPAddress gateway = IPAddress(192, 168, 1, 1);
    IPAddress subnet = IPAddress(255, 255, 255, 0);
    IPAddress dns1 = IPAddress(8, 8, 8, 8);
    IPAddress dns2 = IPAddress(1, 1, 1, 1);
};

struct TcpSettings {
    TcpMode mode = TcpMode::CLIENT;
    String serverIp = "192.168.1.10";
    uint16_t serverPort = 7000;
    uint16_t listenPort = 7000;
    bool autoReconnect = true;
    uint16_t reconnectIntervalMs = 5000;
    uint16_t connectTimeoutMs = 350;
};

struct ScaleTcpSettings {
    bool enabled = true;
    TcpMode mode = TcpMode::CLIENT;
    String serverIp = "192.168.1.20";
    uint16_t serverPort = 4001;
    uint16_t listenPort = 4001;
    bool autoReconnect = true;
    uint16_t reconnectIntervalMs = 5000;
    uint16_t connectTimeoutMs = 350;
};

struct SecuritySettings {
    String adminUser = "admin";
    String adminPassword = "admin";
    String serviceUser = "service";
    String servicePassword = "service";
    String otaPassword = "admin123";
};

struct RfidSettings {
    bool enabled = true;
    uint32_t baudRate = 9600;
    RfidEncoding encoding = RfidEncoding::HEX_MODE;
};

struct QrSettings {
    bool enabled = true;
    uint32_t baudRate = 9600;
    bool sendToTcp = true;
    bool publishToWeb = true;
    bool applyStartupCommands = false;
    bool saveToFlashAfterApply = false;
    uint16_t startupCommandDelayMs = 120;
    uint16_t interCommandDelayMs = 80;
    uint16_t maxFrameLength = 256;
    uint16_t frameIdleTimeoutMs = 100;
    bool acceptCr = true;
    bool acceptLf = true;
    String linePrefix = "QR";
    String startupCommandsHex = "";
    bool tcpBridgeEnabled = true;
    uint16_t tcpBridgePort = 4010;
};

struct FlowSettings {
    bool enabled;
    bool remoteTriggerEnabled;
    bool weightTriggerEnabled;
    uint16_t weightThresholdKg;
    uint16_t summaryScreenMs;
    uint16_t resultScreenMs;
    FlowSettings()
        : enabled(true),
          remoteTriggerEnabled(true),
          weightTriggerEnabled(true),
          weightThresholdKg(500),
          summaryScreenMs(2500),
          resultScreenMs(2500) {}
};

struct FlowScreenSettings {
    bool enabled;
    uint8_t order;
    String name;
    String title;
    String line1;
    String line2;
    String hint;
    FlowScreenSettings()
        : enabled(true), order(1), name("Ekran"), title(""), line1(""), line2(""), hint("") {}
    FlowScreenSettings(bool enabled_, uint8_t order_, const String& name_, const String& title_, const String& line1_, const String& line2_, const String& hint_)
        : enabled(enabled_), order(order_), name(name_), title(title_), line1(line1_), line2(line2_), hint(hint_) {}
};

struct DisplaySettings {
    bool enabled = true;
    uint8_t contrast = 180;
    FlowSettings flow;
    FlowScreenSettings screen1 = FlowScreenSettings(true, 1, "Ekran 1", "ODBIJ KARTE", "Zbliz karte RFID", "", "Czekam na karte");
    FlowScreenSettings screen2 = FlowScreenSettings(true, 2, "Ekran 2", "KOD PRODUKTU", "Wprowadz kod", "", "#=OK *=Kasuj");
    FlowScreenSettings screen3 = FlowScreenSettings(true, 3, "Ekran 3", "SKAN QR", "Zeskanuj kod QR", "", "Czekam na skan");
    FlowScreenSettings screen4 = FlowScreenSettings(true, 4, "Ekran 4", "PODSUMOWANIE", "Dane gotowe", "", "Wyslij do systemu");
};

struct KeypadSettings {
    bool enabled = true;
    uint8_t pcf8574Address = 0x20;
};

struct DiscoverySettings {
    bool enabled = true;
    uint16_t udpPort = 40404;
};

struct DeviceConfig {
    NetworkSettings network;
    TcpSettings tcp;
    ScaleTcpSettings scaleTcp;
    SecuritySettings security;
    RfidSettings rfid;
    QrSettings qr;
    DisplaySettings display;
    KeypadSettings keypad;
    DiscoverySettings discovery;
};
