#ifndef FIREBASE_MANAGER_H
#define FIREBASE_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class FirebaseManager {
public:
  FirebaseManager(const char* projectId, const char* apiKey, const char* deviceId);
  
  // Initialize Firebase manager
  void begin();
  
  // Send usage log to Firestore
  bool sendUsageLog(uint32_t usesSent);
  
  // Get connection status
  bool isReady() const;
  
  // Get last error message
  String getLastError() const;
  
  // Get statistics
  uint32_t getTotalLogsSent() const;
  uint32_t getLastLogTimestamp() const;

private:
  // Configuration
  const char* _projectId;
  const char* _apiKey;
  const char* _deviceId;
  
  // Statistics
  uint32_t _totalLogsSent;
  uint32_t _lastLogTimestamp;
  String _lastError;
  
  // Helper functions
  String generateLogId();
  String getCurrentTimestamp();
  bool sendToFirestore(const String& collection, const String& documentId, const String& jsonData);
  String buildFirestoreUrl(const String& collection, const String& documentId);
  
  // Internal state
  bool _isSending;  // Prevent concurrent sends
  uint32_t _lastSendAttempt;  // Track last send time
  const uint32_t _minSendInterval = 5000;  // Minimum 5 seconds between sends
};

#endif // FIREBASE_MANAGER_H