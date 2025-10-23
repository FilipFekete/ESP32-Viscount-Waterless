#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Global debug flag - set to false to disable all debug output
#define DEBUG_ENABLED true

// Module-specific debug flags
#define DEBUG_WIFI true
#define DEBUG_LED true
#define DEBUG_MAIN true

// Debug macros
#if DEBUG_ENABLED
  #define DEBUG_PRINT(module, msg) \
    do { \
      if (DEBUG_##module) { \
        Serial.print("[" #module "] "); \
        Serial.print(msg); \
      } \
    } while(0)

  #define DEBUG_PRINTLN(module, msg) \
    do { \
      if (DEBUG_##module) { \
        Serial.print("[" #module "] "); \
        Serial.println(msg); \
      } \
    } while(0)

  #define DEBUG_PRINTF(module, fmt, ...) \
    do { \
      if (DEBUG_##module) { \
        Serial.printf("[" #module "] " fmt, ##__VA_ARGS__); \
      } \
    } while(0)
#else
  #define DEBUG_PRINT(module, msg)
  #define DEBUG_PRINTLN(module, msg)
  #define DEBUG_PRINTF(module, fmt, ...)
#endif

#endif // DEBUG_H