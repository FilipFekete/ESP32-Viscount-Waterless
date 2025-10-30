#include "usage_counter.h"
#include "debug.h"

UsageCounter::UsageCounter(uint32_t threshold)
  : _count(0),
    _totalCount(0),
    _threshold(threshold),
    _callback(nullptr),
    _lastTriggerTime(0) {
}

void UsageCounter::begin() {
  // Initialize sensor hardware here
  // For mock: just initialize variables
  _count = 0;
  _totalCount = 0;
  _lastTriggerTime = millis();
  
  DEBUG_PRINTF(MAIN, "Usage counter initialized (threshold: %lu)\n", _threshold);
}

void UsageCounter::update() {
  // MOCK IMPLEMENTATION - Replace with real sensor reading
  // This simulates a usage every _mockInterval milliseconds
  
  if (millis() - _lastTriggerTime >= _mockInterval) {
    _lastTriggerTime = millis();
    increment();
  }
  
  // REAL IMPLEMENTATION WOULD LOOK LIKE:
  // Read sensor (e.g., IR sensor, PIR sensor, etc.)
  // if (digitalRead(SENSOR_PIN) == HIGH) {
  //   // Debounce logic here
  //   increment();
  // }
}

void UsageCounter::increment() {
  _count++;
  _totalCount++;
  
  DEBUG_PRINTF(MAIN, "Usage detected! Count: %lu/%lu (Total: %lu)\n", 
               _count, _threshold, _totalCount);
  
  // Check if threshold reached
  if (_count >= _threshold) {
    DEBUG_PRINTF(MAIN, "Threshold reached! Triggering callback with %lu uses\n", _count);
    
    // Store count before reset
    uint32_t usesToSend = _count;
    
    // Reset counter
    _count = 0;
    
    // Call callback if registered
    if (_callback) {
      _callback(usesToSend);
    }
  }
}

void UsageCounter::reset() {
  _count = 0;
  DEBUG_PRINTLN(MAIN, "Usage counter reset");
}

void UsageCounter::onThresholdReached(UsageCallback callback) {
  _callback = callback;
  DEBUG_PRINTLN(MAIN, "Usage callback registered");
}

uint32_t UsageCounter::getCount() const {
  return _count;
}

uint32_t UsageCounter::getThreshold() const {
  return _threshold;
}

uint32_t UsageCounter::getTotalCount() const {
  return _totalCount;
}

void UsageCounter::setThreshold(uint32_t threshold) {
  _threshold = threshold;
  DEBUG_PRINTF(MAIN, "Threshold updated to %lu\n", threshold);
}
