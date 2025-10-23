#include "wifi_manager.h"
#include "debug.h"

WiFiManager::WiFiManager() : _lastCheck(0), _checkInterval(10000), _wasConnected(false) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);  // Explicitly disable all power saving
  WiFi.setAutoReconnect(true);  // Enable auto-reconnect
  WiFi.persistent(false);  // Don't wear out flash with frequent writes
}

void WiFiManager::addAP(const char* ssid, const char* password) {
  _wifiMulti.addAP(ssid, password);
  DEBUG_PRINTF(WIFI, "Added AP: %s\n", ssid);
}

bool WiFiManager::connect() {
  uint8_t status = _wifiMulti.run();
  
  if (status == WL_CONNECTED) {
    DEBUG_PRINTLN(WIFI, "Connected to WiFi");
    DEBUG_PRINTF(WIFI, "IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  
  return false;
}

void WiFiManager::maintain() {
  uint32_t now = millis();
  
  // Check connection periodically
  if (now - _lastCheck >= _checkInterval) {
    _lastCheck = now;
    
    bool connected = isConnected();
    
    // Only print when state changes to reduce spam
    if (connected != _wasConnected) {
      _wasConnected = connected;
      if (connected) {
        DEBUG_PRINTLN(WIFI, "Connected");
      } else {
        DEBUG_PRINTLN(WIFI, "Disconnected - reconnecting...");
      }
    }
    
    if (!connected) {
      // Force reconnect
      WiFi.disconnect();
      delay(100);
      _wifiMulti.run();
    }
  }
}

void WiFiManager::setCheckInterval(uint32_t intervalMs) {
  _checkInterval = intervalMs;
}

void WiFiManager::disconnect() {
  WiFi.disconnect(true);
  DEBUG_PRINTLN(WIFI, "Disconnected");
}

bool WiFiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiManager::getLocalIP() const {
  return WiFi.localIP();
}

int8_t WiFiManager::getRSSI() const {
  return WiFi.RSSI();
}

String WiFiManager::getSSID() const {
  return WiFi.SSID();
}