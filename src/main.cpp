#include <Arduino.h>
#include "debug.h"
#include "wifi_manager.h"
#include "led_controller.h"
#include "secrets.h"

// Hardware configuration
#define LED_PIN 2

// Global instances
WiFiManager* wifiManager = nullptr;
LEDController* statusLED = nullptr;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=== ESP32 Starting ===");
  
  // Print reset reason
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("Reset reason: %d\n", reason);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  // Create instances
  statusLED = new LEDController(LED_PIN);
  wifiManager = new WiFiManager();
  
  // Initialize LED
  statusLED->begin();
  
  // Setup WiFi
  wifiManager->addAP(WIFI_SSID, WIFI_PASSWORD);
  
  // Connect to WiFi (blocking loop like original code)
  Serial.println("Connecting to WiFi...");
  while (!wifiManager->connect()) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("\n=== Setup Complete ===");
  Serial.printf("Free heap after setup: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  static uint32_t loopCounter = 0;
  static uint32_t lastHeapPrint = 0;
  
  loopCounter++;
  
  // Feed the watchdog
  yield();
  
  // Print heartbeat every 30 seconds to know ESP is alive
  if (millis() - lastHeapPrint >= 30000) {
    lastHeapPrint = millis();
    Serial.printf("[MAIN] Alive - Loops: %lu, Heap: %d, WiFi: %s\n", 
                  loopCounter, ESP.getFreeHeap(), 
                  wifiManager->isConnected() ? "OK" : "LOST");
  }
  
  // Maintain WiFi connection
  wifiManager->maintain();
  
  // Update LED based on WiFi status
  if (statusLED && wifiManager) {
    bool connected = wifiManager->isConnected();
    statusLED->setState(connected);
    
    // Print to serial if connection state changes
    static bool lastState = true;
    if (connected != lastState) {
      lastState = connected;
      Serial.printf("[MAIN] WiFi %s\n", connected ? "connected" : "disconnected!");
    }
  }
  
  delay(100);  // Increased delay to reduce CPU load
}