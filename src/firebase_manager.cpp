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
    _lastError(""),
    _isSending(false),
    _lastSendAttempt(0) {
}

void FirebaseManager::begin() {
  DEBUG_PRINTLN(MAIN, "Firebase Manager initialized");
  DEBUG_PRINTF(MAIN, "Project ID: %s\n", _projectId);
  DEBUG_PRINTF(MAIN, "Device ID: %s\n", _deviceId);
  
  // Configure NTP for timestamp generation with reduced timeout
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Wait for time sync with shorter timeout (5 seconds max)
  int retries = 0;
  while (time(nullptr) < 1000000000 && retries < 10) {
    delay(500);
    Serial.print(".");
    retries++;
    yield();  // Feed watchdog
  }
  Serial.println();
  
  if (time(nullptr) < 1000000000) {
    DEBUG_PRINTLN(MAIN, "Warning: NTP sync failed, using millis-based timestamps");
    _lastError = "NTP sync failed";
  } else {
    time_t now = time(nullptr);
    DEBUG_PRINTF(MAIN, "NTP time synchronized: %lu\n", now);
  }
}

bool FirebaseManager::sendUsageLog(uint32_t usesSent) {
  // Prevent concurrent sends
  if (_isSending) {
    _lastError = "Send already in progress";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return false;
  }
  
  // Rate limiting - prevent sends closer than 5 seconds
  uint32_t now = millis();
  if (now - _lastSendAttempt < _minSendInterval) {
    _lastError = "Rate limited - too soon since last send";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return false;
  }
  _lastSendAttempt = now;
  
  if (!isReady()) {
    _lastError = "Firebase not ready - check WiFi connection";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return false;
  }
  
  // Set sending flag
  _isSending = true;
  
  DEBUG_PRINTF(MAIN, "Sending usage log: %lu uses\n", usesSent);
  DEBUG_PRINTF(MAIN, "Free heap before send: %d bytes\n", ESP.getFreeHeap());
  
  yield();  // Feed watchdog
  
  // Generate unique log ID
  String logId = generateLogId();
  DEBUG_PRINTF(MAIN, "Generated log ID: %s\n", logId.c_str());
  
  yield();  // Feed watchdog
  
  // Get current timestamp in ISO 8601 format
  String timestamp = getCurrentTimestamp();
  DEBUG_PRINTF(MAIN, "Generated timestamp: %s\n", timestamp.c_str());
  
  yield();  // Feed watchdog
  
  bool success = false;
  
  // Use try-catch style error handling
  do {
    // Create JSON document for Firestore with reduced size
    DynamicJsonDocument* doc = new DynamicJsonDocument(384);
    if (!doc) {
      _lastError = "Failed to allocate JSON document";
      DEBUG_PRINTLN(MAIN, _lastError.c_str());
      break;
    }
    
    (*doc)["fields"]["device_id"]["stringValue"] = _deviceId;
    (*doc)["fields"]["uses_sent"]["integerValue"] = String(usesSent);
    (*doc)["fields"]["timestamp"]["timestampValue"] = timestamp;
    
    yield();  // Feed watchdog
    
    // Serialize JSON
    String jsonData;
    size_t jsonSize = serializeJson(*doc, jsonData);
    DEBUG_PRINTF(MAIN, "JSON size: %d bytes\n", jsonSize);
    
    // Clear document to free memory immediately
    delete doc;
    doc = nullptr;
    
    yield();  // Feed watchdog
    
    DEBUG_PRINTF(MAIN, "Free heap after JSON creation: %d bytes\n", ESP.getFreeHeap());
    
    // Send to Firestore
    success = sendToFirestore("usage_logs", logId, jsonData);
    
  } while(false);  // Single-iteration loop for break-based error handling
  
  yield();  // Feed watchdog
  
  DEBUG_PRINTF(MAIN, "Free heap after send: %d bytes\n", ESP.getFreeHeap());
  
  if (success) {
    _totalLogsSent++;
    _lastLogTimestamp = millis();
    DEBUG_PRINTF(MAIN, "Usage log sent successfully! Total logs: %lu\n", _totalLogsSent);
  } else {
    DEBUG_PRINTF(MAIN, "Failed to send usage log: %s\n", _lastError.c_str());
  }
  
  // Clear sending flag
  _isSending = false;
  
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
           (uint16_t)random(0xFFFF));
  return String(logId);
}

String FirebaseManager::getCurrentTimestamp() {
  // Get current time
  time_t now = time(nullptr);
  
  // If NTP failed, create timestamp from millis
  if (now < 1000000000) {
    // Use a base date (2025-01-01) and add milliseconds
    // This is a fallback - timestamps won't be accurate but will be unique
    now = 1735689600 + (millis() / 1000);  // 2025-01-01 00:00:00 UTC
    DEBUG_PRINTLN(MAIN, "Using fallback timestamp (NTP unavailable)");
  }
  
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
  
  // Set timeout to prevent hanging
  http.setTimeout(10000);  // 10 second timeout
  
  // Build Firestore REST API URL
  String url = buildFirestoreUrl(collection, documentId);
  
  DEBUG_PRINTF(MAIN, "Firestore URL: %s\n", url.c_str());
  
  // Begin HTTP connection
  bool beginResult = http.begin(url);
  if (!beginResult) {
    _lastError = "Failed to begin HTTP connection";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return false;
  }
  
  http.addHeader("Content-Type", "application/json");
  
  yield();  // Feed watchdog before POST
  
  // Send POST request (creates or updates document)
  int httpCode = http.POST(jsonData);
  
  yield();  // Feed watchdog after POST
  
  // Check response
  if (httpCode > 0) {
    DEBUG_PRINTF(MAIN, "HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == 200) {
      String response = http.getString();
      DEBUG_PRINTF(MAIN, "Response length: %d bytes\n", response.length());
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
  // Firestore REST API endpoint format:
  // To create with custom ID: PATCH https://firestore.googleapis.com/v1/projects/{projectId}/databases/(default)/documents/{collection}/{documentId}
  // OR use documentId as query parameter
  
  String url = "https://firestore.googleapis.com/v1/projects/";
  url += _projectId;
  url += "/databases/(default)/documents/";
  url += collection;
  url += "?documentId=";
  url += documentId;
  url += "&key=";
  url += _apiKey;
  
  return url;
}