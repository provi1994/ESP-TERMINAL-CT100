#pragma once

#include <Preferences.h>
#include <AppTypes.h>

class ConfigManager {
 public:
  bool begin();
  DeviceConfig load();
  bool save(const DeviceConfig& config);
  const DeviceConfig& current() const { return config_; }
  void setCurrent(const DeviceConfig& config) { config_ = config; }

  static String ipToString(const IPAddress& ip);
  static IPAddress stringToIp(const String& ipStr);
  static String networkModeToString(NetworkMode mode);
  static NetworkMode networkModeFromString(const String& value);
  static String tcpModeToString(TcpMode mode);
  static TcpMode tcpModeFromString(const String& value);
  static String rfidEncodingToString(RfidEncoding encoding);
  static RfidEncoding rfidEncodingFromString(const String& value);

 private:
  Preferences prefs_;
  DeviceConfig config_;
};
