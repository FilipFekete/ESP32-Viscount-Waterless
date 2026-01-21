#include "firebase_manager.h"
#include "debug.h"
#include <WiFiClientSecure.h>
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
  
  DEBUG_PRINTF(MAIN, "Incrementing device uses by %lu\n", usesSent);
  DEBUG_PRINTF(MAIN, "Free heap before send: %d bytes\n", ESP.getFreeHeap());
  
  yield();  // Feed watchdog
  
  bool success = false;
  const char* collection = "devices";
  const char* documentId = "device_001";
  String documentPath = buildFirestoreDocumentPath(collection, documentId);
  DEBUG_PRINTF(MAIN, "Device document: %s\n", documentPath.c_str());
  
  // Use try-catch style error handling
  do {
    String getResponse;
    int getCode = getFirestoreDocument(collection, documentId, getResponse);
    int64_t currentUses = 0;
    
    if (getCode == HTTP_CODE_OK || getCode == 200) {
      DynamicJsonDocument* getDoc = new DynamicJsonDocument(768);
      if (!getDoc) {
        _lastError = "Failed to allocate get document";
        DEBUG_PRINTLN(MAIN, _lastError.c_str());
        break;
      }
      
      DeserializationError err = deserializeJson(*getDoc, getResponse);
      if (err) {
        _lastError = "Failed to parse Firestore response";
        DEBUG_PRINTLN(MAIN, _lastError.c_str());
        delete getDoc;
        break;
      }
      
      const char* usesValue = (*getDoc)["fields"]["uses"]["integerValue"] | "0";
      currentUses = atoll(usesValue);
      
      delete getDoc;
      getDoc = nullptr;
    } else if (getCode == HTTP_CODE_NOT_FOUND || getCode == 404) {
      DynamicJsonDocument* createDoc = new DynamicJsonDocument(256);
      if (!createDoc) {
        _lastError = "Failed to allocate create document";
        DEBUG_PRINTLN(MAIN, _lastError.c_str());
        break;
      }
      
      (*createDoc)["fields"]["uses"]["integerValue"] = String(usesSent);
      
      String createJson;
      serializeJson(*createDoc, createJson);
      
      delete createDoc;
      createDoc = nullptr;
      
      int createCode = createFirestoreDocument(collection, documentId, createJson);
      if (createCode == HTTP_CODE_OK || createCode == HTTP_CODE_CREATED || createCode == 200 || createCode == 201) {
        success = true;
      }
      break;
    } else {
      _lastError = "HTTP error: " + String(getCode);
      break;
    }
    
    int64_t newUses = currentUses + usesSent;
    
    DynamicJsonDocument* doc = new DynamicJsonDocument(256);
    if (!doc) {
      _lastError = "Failed to allocate JSON document";
      DEBUG_PRINTLN(MAIN, _lastError.c_str());
      break;
    }
    
    (*doc)["fields"]["uses"]["integerValue"] = String(newUses);
    
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
    
    // Update uses in Firestore
    int httpCode = patchFirestoreDocument(collection, documentId, jsonData, "uses");
    if (httpCode == HTTP_CODE_OK || httpCode == 200) {
      success = true;
      break;
    }
    
    // If document doesn't exist yet, create it with the initial count
    if (httpCode == HTTP_CODE_NOT_FOUND || httpCode == 404) {
      DynamicJsonDocument* createDoc = new DynamicJsonDocument(256);
      if (!createDoc) {
        _lastError = "Failed to allocate create document";
        DEBUG_PRINTLN(MAIN, _lastError.c_str());
        break;
      }
      
      (*createDoc)["fields"]["uses"]["integerValue"] = String(newUses);
      
      String createJson;
      serializeJson(*createDoc, createJson);
      
      delete createDoc;
      createDoc = nullptr;
      
      int createCode = createFirestoreDocument(collection, documentId, createJson);
      if (createCode == HTTP_CODE_OK || createCode == HTTP_CODE_CREATED || createCode == 200 || createCode == 201) {
        success = true;
      }
    }
    
  } while(false);  // Single-iteration loop for break-based error handling
  
  yield();  // Feed watchdog
  
  DEBUG_PRINTF(MAIN, "Free heap after send: %d bytes\n", ESP.getFreeHeap());
  
  if (success) {
    _totalLogsSent++;
    _lastLogTimestamp = millis();
    DEBUG_PRINTF(MAIN, "Usage counter updated! Total sends: %lu\n", _totalLogsSent);
  } else {
    DEBUG_PRINTF(MAIN, "Failed to update usage counter: %s\n", _lastError.c_str());
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

int FirebaseManager::getFirestoreDocument(const String& collection, const String& documentId, String& response) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);
  
  // Set timeout to prevent hanging
  http.setTimeout(10000);  // 10 second timeout
  
  String url = buildFirestoreDocumentUrl(collection, documentId);
  DEBUG_PRINTF(MAIN, "Firestore get URL: %s\n", url.c_str());
  
  // Begin HTTP connection
  bool beginResult = http.begin(client, url);
  if (!beginResult) {
    _lastError = "Failed to begin HTTP connection";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return -1;
  }
  http.setReuse(false);
  
  yield();  // Feed watchdog before GET
  
  // Send GET request
  int httpCode = http.GET();
  
  yield();  // Feed watchdog after GET
  
  // Check response
  if (httpCode > 0) {
    DEBUG_PRINTF(MAIN, "HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == 200 || httpCode == HTTP_CODE_NOT_FOUND || httpCode == 404) {
      response = http.getString();
      DEBUG_PRINTF(MAIN, "Response length: %d bytes\n", response.length());
      http.end();
      return httpCode;
    }
    
    _lastError = "HTTP error: " + String(httpCode);
    String response = http.getString();
    DEBUG_PRINTF(MAIN, "Error response: %s\n", response.c_str());
  } else {
    _lastError = "Connection failed: " + http.errorToString(httpCode);
    DEBUG_PRINTF(MAIN, "HTTP Error: %s\n", _lastError.c_str());
  }
  
  http.end();
  return httpCode;
}

int FirebaseManager::patchFirestoreDocument(const String& collection, const String& documentId, const String& jsonData, const String& updateMask) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);
  
  // Set timeout to prevent hanging
  http.setTimeout(10000);  // 10 second timeout
  
  String url = buildFirestoreDocumentUrlWithMask(collection, documentId, updateMask);
  DEBUG_PRINTF(MAIN, "Firestore patch URL: %s\n", url.c_str());
  
  // Begin HTTP connection
  bool beginResult = http.begin(client, url);
  if (!beginResult) {
    _lastError = "Failed to begin HTTP connection";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return -1;
  }
  http.setReuse(false);
  
  http.addHeader("Content-Type", "application/json");
  
  yield();  // Feed watchdog before PATCH
  
  int httpCode = http.sendRequest("PATCH", jsonData);
  
  yield();  // Feed watchdog after PATCH
  
  // Check response
  if (httpCode > 0) {
    DEBUG_PRINTF(MAIN, "HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == 200 || httpCode == HTTP_CODE_NOT_FOUND || httpCode == 404) {
      String response = http.getString();
      DEBUG_PRINTF(MAIN, "Response length: %d bytes\n", response.length());
      http.end();
      return httpCode;
    }
    
    _lastError = "HTTP error: " + String(httpCode);
    String response = http.getString();
    DEBUG_PRINTF(MAIN, "Error response: %s\n", response.c_str());
  } else {
    _lastError = "Connection failed: " + http.errorToString(httpCode);
    DEBUG_PRINTF(MAIN, "HTTP Error: %s\n", _lastError.c_str());
  }
  
  http.end();
  return httpCode;
}

int FirebaseManager::createFirestoreDocument(const String& collection, const String& documentId, const String& jsonData) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);
  
  // Set timeout to prevent hanging
  http.setTimeout(10000);  // 10 second timeout
  
  String url = buildFirestoreCreateUrl(collection, documentId);
  DEBUG_PRINTF(MAIN, "Firestore create URL: %s\n", url.c_str());
  
  // Begin HTTP connection
  bool beginResult = http.begin(client, url);
  if (!beginResult) {
    _lastError = "Failed to begin HTTP connection";
    DEBUG_PRINTLN(MAIN, _lastError.c_str());
    return -1;
  }
  http.setReuse(false);
  
  http.addHeader("Content-Type", "application/json");
  
  yield();  // Feed watchdog before POST
  
  // Send POST request (create document)
  int httpCode = http.POST(jsonData);
  
  yield();  // Feed watchdog after POST
  
  // Check response
  if (httpCode > 0) {
    DEBUG_PRINTF(MAIN, "HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == 200 || httpCode == 201) {
      String response = http.getString();
      DEBUG_PRINTF(MAIN, "Response length: %d bytes\n", response.length());
      http.end();
      return httpCode;
    }
    
    _lastError = "HTTP error: " + String(httpCode);
    String response = http.getString();
    DEBUG_PRINTF(MAIN, "Error response: %s\n", response.c_str());
  } else {
    _lastError = "Connection failed: " + http.errorToString(httpCode);
    DEBUG_PRINTF(MAIN, "HTTP Error: %s\n", _lastError.c_str());
  }
  
  http.end();
  return httpCode;
}

String FirebaseManager::buildFirestoreDocumentUrl(const String& collection, const String& documentId) const {
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

String FirebaseManager::buildFirestoreDocumentUrlWithMask(const String& collection, const String& documentId, const String& updateMask) const {
  String url = buildFirestoreDocumentUrl(collection, documentId);
  if (updateMask.length() > 0) {
    url += "&updateMask.fieldPaths=";
    url += updateMask;
  }
  return url;
}

String FirebaseManager::buildFirestoreCreateUrl(const String& collection, const String& documentId) const {
  // Firestore REST API endpoint format:
  // To create with custom ID: POST https://firestore.googleapis.com/v1/projects/{projectId}/databases/(default)/documents/{collection}?documentId={documentId}
  
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

String FirebaseManager::buildFirestoreDocumentPath(const String& collection, const String& documentId) const {
  String path = "projects/";
  path += _projectId;
  path += "/databases/(default)/documents/";
  path += collection;
  path += "/";
  path += documentId;
  
  return path;
}
