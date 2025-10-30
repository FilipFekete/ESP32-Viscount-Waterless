#include <Arduino.h>
#include "debug.h"
#include "wifi_manager.h"
#include "led_controller.h"
#include "firebase_manager.h"
#include "usage_counter.h"
#include "secrets.h"

// Hardware configuration
#define LED_PIN 2

// Global instances
WiFiManager* wifiManager = nullptr;
LEDController* statusLED = nullptr;
FirebaseManager* firebaseManager = nullptr;
UsageCounter* usageCounter = nullptr;

// Callback function for when usage threshold is reached
void onUsageThresholdReached(uint32_t uses) {
  Serial.printf("\n[CALLBACK] Threshold reached! Sending %lu uses to Firebase...\n", uses);
  
  // Send usage log to Firebase
  if (firebaseManager->sendUsageLog(uses)) {
    Serial.println("[CALLBACK] Usage log sent successfully!");
    Serial.printf("[CALLBACK] Total logs sent: %lu\n", firebaseManager->getTotalLogsSent());
  } else {
    Serial.println("[CALLBACK] Failed to send usage log!");
    Serial.printf("[CALLBACK] Error: %s\n", firebaseManager->getLastError().c_str());
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=== ESP32 Urinal Monitor Starting ===");
  
  // Print reset reason
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("Reset reason: %d\n", reason);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  // Create instances
  statusLED = new LEDController(LED_PIN);
  wifiManager = new WiFiManager();
  firebaseManager = new FirebaseManager(FIREBASE_PROJECT_ID, FIREBASE_API_KEY, DEVICE_ID);
  usageCounter = new UsageCounter(USAGE_THRESHOLD);
  
  // Initialize LED
  statusLED->begin();
  
  // Setup WiFi
  wifiManager->addAP(WIFI_SSID, WIFI_PASSWORD);
  
  // Connect to WiFi (blocking loop like original code)
  Serial.println("Connecting to WiFi...");
  statusLED->on();  // LED on while connecting
  
  while (!wifiManager->connect()) {
    Serial.print(".");
    statusLED->toggle();
    delay(500);
  }
  
  statusLED->on();  // LED solid when connected
  Serial.println("\nWiFi Connected!");
  Serial.printf("IP Address: %s\n", wifiManager->getLocalIP().toString().c_str());
  
  // Initialize Firebase
  firebaseManager->begin();
  
  // Initialize usage counter and register callback
  usageCounter->begin();
  usageCounter->onThresholdReached(onUsageThresholdReached);
  
  Serial.println("\n=== Setup Complete ===");
  Serial.printf("Device ID: %s\n", DEVICE_ID);
  Serial.printf("Usage threshold: %d\n", USAGE_THRESHOLD);
  Serial.printf("Free heap after setup: %d bytes\n", ESP.getFreeHeap());
  Serial.println("\nMonitoring usage...\n");
}

void loop() {
  static uint32_t loopCounter = 0;
  static uint32_t lastHeapPrint = 0;
  
  loopCounter++;
  
  // Feed the watchdog
  yield();
  
  // Print heartbeat every 30 seconds
  if (millis() - lastHeapPrint >= 30000) {
    lastHeapPrint = millis();
    Serial.printf("[MAIN] Alive - Loops: %lu, Heap: %d, WiFi: %s\n", 
                  loopCounter, 
                  ESP.getFreeHeap(), 
                  wifiManager->isConnected() ? "OK" : "LOST");
    Serial.printf("[MAIN] Usage: %lu/%lu (Total: %lu), Logs sent: %lu\n",
                  usageCounter->getCount(),
                  usageCounter->getThreshold(),
                  usageCounter->getTotalCount(),
                  firebaseManager->getTotalLogsSent());
  }
  
  // Maintain WiFi connection
  wifiManager->maintain();
  
  // Update usage counter (reads sensor)
  usageCounter->update();
  
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
  
  delay(100);  // Reduced delay for more responsive sensor reading
}