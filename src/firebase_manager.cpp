#include "firebase_manager.h"
#include "debug.h"
#include <time.h>
#include <sys/time.h>

FirebaseManager::FirebaseManager(const char* projectId, const char* apiKey, const char* deviceId)
  : _projectId(projectId),
    _apiKey(apiKey),
    _deviceId(deviceId),
    _totalLogsSent(0),
    _lastLogTimestamp(0),
    _lastError("") {
}

void FirebaseManager::begin() {
  // Configure NTP for timestamp generation
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  DEBUG_PRINTLN(MAIN, "Firebase Manager initialized");
  DEBUG_PRINTF(MAIN, "Project ID: %s\n", _projectId);
  DEBUG_PRINTF(MAIN, "Device ID: %s\n", _deviceId);
  
  // Wait for time sync (with timeout)
  int retries = 0;
  while (time(nullptr) < 1000000000 && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (time(nullptr) < 1000000000) {
    DEBUG_PRINTLN(MAIN, "Warning: NTP sync failed, timestamps may be inaccurate");
  } else {
    DEBUG_PRINTLN(MAIN, "NTP time synchronized");
  }
}

bool FirebaseManager::sendUsageLog(uint32_t usesSent) {
  if (!isReady()) {
    _lastError = "Firebase not ready - check WiFi connection";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return false;
  }
  
  DEBUG_PRINTF(MAIN, "Sending usage log: %lu uses\n", usesSent);
  
  // Generate unique log ID
  String logId = generateLogId();
  
  // Get current timestamp in ISO 8601 format
  String timestamp = getCurrentTimestamp();
  
  // Create JSON document for Firestore
  // Firestore REST API expects fields in a specific format
  StaticJsonDocument<512> doc;
  
  doc["fields"]["device_id"]["stringValue"] = _deviceId;
  doc["fields"]["uses_sent"]["integerValue"] = String(usesSent);
  doc["fields"]["timestamp"]["timestampValue"] = timestamp;
  
  // Serialize JSON
  String jsonData;
  serializeJson(doc, jsonData);
  
  DEBUG_PRINTF(MAIN, "JSON payload: %s\n", jsonData.c_str());
  
  // Send to Firestore
  bool success = sendToFirestore("usage_logs", logId, jsonData);
  
  if (success) {
    _totalLogsSent++;
    _lastLogTimestamp = millis();
    DEBUG_PRINTF(MAIN, "Usage log sent successfully! Total logs: %lu\n", _totalLogsSent);
  } else {
    DEBUG_PRINTF(MAIN, "Failed to send usage log: %s\n", _lastError.c_str());
  }
  
  return success;
}

bool FirebaseManager::isReady() const {
  return WiFi.status() == WL_CONNECTED;
}

String FirebaseManager::getLastError() const {
  return _lastError;
}

uint32_t FirebaseManager::getTotalLogsSent() const {
  return _totalLogsSent;
}

uint32_t FirebaseManager::getLastLogTimestamp() const {
  return _lastLogTimestamp;
}

String FirebaseManager::generateLogId() {
  // Generate unique ID using device ID + timestamp + random
  char logId[64];
  snprintf(logId, sizeof(logId), "log_%s_%lu_%04X", 
           _deviceId, 
           millis(), 
           random(0xFFFF));
  return String(logId);
}

String FirebaseManager::getCurrentTimestamp() {
  // Get current time
  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  
  // Format as ISO 8601 (RFC 3339) timestamp
  // Format: YYYY-MM-DDTHH:MM:SSZ
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  
  return String(timestamp);
}

bool FirebaseManager::sendToFirestore(const String& collection, const String& documentId, const String& jsonData) {
  HTTPClient http;
  
  // Build Firestore REST API URL
  String url = buildFirestoreUrl(collection, documentId);
  
  DEBUG_PRINTF(MAIN, "Firestore URL: %s\n", url.c_str());
  
  // Begin HTTP connection
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Send POST request (creates or updates document)
  int httpCode = http.POST(jsonData);
  
  // Check response
  if (httpCode > 0) {
    DEBUG_PRINTF(MAIN, "HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == 200) {
      String response = http.getString();
      DEBUG_PRINTF(MAIN, "Response: %s\n", response.c_str());
      http.end();
      return true;
    } else {
      _lastError = "HTTP error: " + String(httpCode);
      String response = http.getString();
      DEBUG_PRINTF(MAIN, "Error response: %s\n", response.c_str());
    }
  } else {
    _lastError = "Connection failed: " + http.errorToString(httpCode);
    DEBUG_PRINTF(MAIN, "HTTP Error: %s\n", _lastError.c_str());
  }
  
  http.end();
  return false;
}

String FirebaseManager::buildFirestoreUrl(const String& collection, const String& documentId) {
  // Firestore REST API endpoint for europe-west1 region
  // Format: https://firestore.googleapis.com/v1/projects/{projectId}/databases/(default)/documents/{collection}/{documentId}?key={apiKey}
  
  String url = "https://firestore.googleapis.com/v1/projects/";
  url += _projectId;
  url += "/databases/(default)/documents/";
  url += collection;
  url += "/";
  url += documentId;
  url += "?key=";
  url += _apiKey;
  
  return url;
}