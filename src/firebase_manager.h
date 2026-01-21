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
  
  // Increment usage counter in Firestore
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
  String getCurrentTimestamp();
  int createFirestoreDocument(const String& collection, const String& documentId, const String& jsonData);
  int getFirestoreDocument(const String& collection, const String& documentId, String& response);
  int patchFirestoreDocument(const String& collection, const String& documentId, const String& jsonData, const String& updateMask);
  String buildFirestoreCreateUrl(const String& collection, const String& documentId) const;
  String buildFirestoreDocumentUrl(const String& collection, const String& documentId) const;
  String buildFirestoreDocumentUrlWithMask(const String& collection, const String& documentId, const String& updateMask) const;
  String buildFirestoreDocumentPath(const String& collection, const String& documentId) const;
  
  // Internal state
  bool _isSending;  // Prevent concurrent sends
  uint32_t _lastSendAttempt;  // Track last send time
  const uint32_t _minSendInterval = 5000;  // Minimum 5 seconds between sends
};

#endif // FIREBASE_MANAGER_H
