#ifndef USAGE_COUNTER_H
#define USAGE_COUNTER_H

#include <Arduino.h>
#include <functional>

// Callback function type for when usage threshold is reached
typedef std::function<void(uint32_t)> UsageCallback;

class UsageCounter {
public:
  UsageCounter(uint32_t threshold = 100);
  
  // Initialize the sensor
  void begin();
  
  // Update function - call in loop to check sensor
  void update();
  
  // Manual increment (for testing or alternative sensors)
  void increment();
  
  // Reset counter
  void reset();
  
  // Set callback function to be called when threshold is reached
  void onThresholdReached(UsageCallback callback);
  
  // Getters
  uint32_t getCount() const;
  uint32_t getThreshold() const;
  uint32_t getTotalCount() const;  // Total count since boot
  
  // Setters
  void setThreshold(uint32_t threshold);

private:
  uint32_t _count;           // Current count (resets after callback)
  uint32_t _totalCount;      // Total count since boot
  uint32_t _threshold;       // Trigger callback at this count
  UsageCallback _callback;   // Callback function
  
  // Mock sensor variables (replace with real sensor logic)
  uint32_t _lastTriggerTime;
  const uint32_t _mockInterval = 5000;  // Simulate usage every 5 seconds for testing
};

#endif // USAGE_COUNTER_H