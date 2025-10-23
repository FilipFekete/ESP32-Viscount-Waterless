#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>

class WiFiManager {
public:
  WiFiManager();
  
  // Basic operations
  void addAP(const char* ssid, const char* password);
  bool connect();  // Non-blocking, call in loop
  void disconnect();
  void maintain();  // Call in loop to keep connection alive
  
  // Status
  bool isConnected() const;
  IPAddress getLocalIP() const;
  int8_t getRSSI() const;
  String getSSID() const;
  
  // Configuration
  void setCheckInterval(uint32_t intervalMs);

private:
  WiFiMulti _wifiMulti;
  uint32_t _lastCheck;
  uint32_t _checkInterval;
  bool _wasConnected;
};

#endif // WIFI_MANAGER_H