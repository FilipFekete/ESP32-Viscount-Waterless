# ESP32 Urinal Usage Monitor

An IoT solution for monitoring waterless urinal usage with real-time data logging to Firebase Firestore. Built for ESP32 microcontrollers with automatic WiFi reconnection and cloud synchronization.

## 🎯 Overview

This project monitors urinal usage in commercial and public facilities, automatically logging usage data to a cloud database. The system is designed for waterless urinals to track maintenance schedules and optimize cleaning operations.

### How It Works

```
┌─────────────┐
│   Sensor    │  Detects usage
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  ESP32 MCU  │  Counts usage (threshold: 100)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   WiFi      │  Connects to network
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Firebase   │  Stores usage logs
│  Firestore  │
└─────────────┘
```


### Application Workflow

```
┌─────────────────────────────────────────────────────────────┐
│                         STARTUP                              │
│  1. Initialize Serial (115200 baud)                         │
│  2. Initialize LED Controller                               │
│  3. Connect to WiFi                                          │
│  4. Sync time via NTP                                        │
│  5. Initialize Firebase Manager                             │
│  6. Initialize Usage Counter                                │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                      MAIN LOOP                               │
│  ┌────────────────────────────────────────────────────┐    │
│  │ 1. Maintain WiFi Connection                        │    │
│  │    - Check every 10 seconds                        │    │
│  │    - Auto-reconnect if disconnected                │    │
│  └────────────────────────────────────────────────────┘    │
│                      │                                       │
│                      ▼                                       │
│  ┌────────────────────────────────────────────────────┐    │
│  │ 2. Update Usage Counter                            │    │
│  │    - Read sensor (currently mocked every 5s)       │    │
│  │    - Increment count                               │    │
│  │    - Check threshold (100 uses)                    │    │
│  └────────────────────────────────────────────────────┘    │
│                      │                                       │
│                      ▼                                       │
│  ┌────────────────────────────────────────────────────┐    │
│  │ 3. When Threshold Reached:                         │    │
│  │    a. Generate unique log ID                       │    │
│  │    b. Create timestamp (ISO 8601)                  │    │
│  │    c. Build JSON payload                           │    │
│  │    d. Send to Firestore                            │    │
│  │    e. Reset counter                                │    │
│  └────────────────────────────────────────────────────┘    │
│                      │                                       │
│                      ▼                                       │
│  ┌────────────────────────────────────────────────────┐    │
│  │ 4. Update Status LED                               │    │
│  │    - ON = WiFi Connected                           │    │
│  │    - OFF = WiFi Disconnected                       │    │
│  └────────────────────────────────────────────────────┘    │
│                      │                                       │
│                      ▼                                       │
│  ┌────────────────────────────────────────────────────┐    │
│  │ 5. Heartbeat Monitor (every 30s)                   │    │
│  │    - Print system stats                            │    │
│  │    - Check memory usage                            │    │
│  │    - Verify connectivity                           │    │
│  └────────────────────────────────────────────────────┘    │
│                      │                                       │
│                      └───────────────┐                       │
│                                      │                       │
└──────────────────────────────────────┼───────────────────────┘
                                       │
                                       └──► Loop repeats
```

### Component Breakdown

#### 1. **WiFi Manager** (`wifi_manager.h/cpp`)
- Manages WiFi connections with auto-reconnect
- Uses WiFiMulti for multiple AP support
- Checks connection every 10 seconds
- Disables power saving for reliability

#### 2. **LED Controller** (`led_controller.h/cpp`)
- Simple GPIO control for status LED
- Methods: `on()`, `off()`, `toggle()`, `setState()`
- Visual feedback for system state

#### 3. **Firebase Manager** (`firebase_manager.h/cpp`)
- Handles all Firebase Firestore communication
- Generates unique log IDs: `log_{device_id}_{timestamp}_{random}`
- Creates ISO 8601 timestamps via NTP
- Uses Firestore REST API
- Tracks statistics (total logs sent, success rate)

#### 4. **Usage Counter** (`usage_counter.h/cpp`)
- Monitors sensor input (currently mocked)
- Counts usage events
- Triggers callback at threshold (100 uses)
- Tracks total and current counts

#### 5. **Debug System** (`debug.h`)
- Module-specific debug flags
- Conditional compilation
- Reduces serial spam in production


### Pin Configuration

| Component | GPIO Pin | Notes |
|-----------|----------|-------|
| Status LED | GPIO 2 | Built-in on most ESP32 boards |
| Sensor Input | TBD | Configure based on sensor type |

## ⚙️ Configuration

### secrets.h Parameters

| Parameter | Description | Example |
|-----------|-------------|---------|
| `WIFI_SSID` | Your WiFi network name | `"MyNetwork"` |
| `WIFI_PASSWORD` | Your WiFi password | `"MyPassword123"` |
| `FIREBASE_PROJECT_ID` | Firebase project ID | `"my-project-id"` |
| `FIREBASE_API_KEY` | Firebase Web API key | `"AIza..."` |
| `DEVICE_ID` | Unique device identifier | `"device_001"` |
| `USAGE_THRESHOLD` | Uses before sending log | `100` |

### Debug Configuration

Edit `debug.h` to enable/disable debug output:

```cpp
#define DEBUG_ENABLED true    // Master switch
#define DEBUG_WIFI true       // WiFi debug
#define DEBUG_LED true        // LED debug
#define DEBUG_MAIN true       // Main loop debug
```

## 🚀 Usage

### Normal Operation

1. **Power on** the ESP32
2. **Wait for WiFi connection** (LED turns solid)
3. **Device monitors usage** automatically
4. **Every 100 uses**, data uploads to Firebase
5. **Check Firebase Console** to view logs

### Serial Monitor Output

```
=== ESP32 Urinal Monitor Starting ===
Reset reason: 1
Free heap: 295092 bytes
[LED] LED initialized on pin 2
[WIFI] Added AP: MyNetwork
Connecting to WiFi...
WiFi Connected!
IP Address: 192.168.1.100
[MAIN] Firebase Manager initialized
[MAIN] NTP time synchronized
[MAIN] Usage counter initialized (threshold: 100)

=== Setup Complete ===
Device ID: device_001
Usage threshold: 100
Free heap after setup: 270456 bytes

Monitoring usage...

[MAIN] Usage detected! Count: 1/100 (Total: 1)
[MAIN] Usage detected! Count: 2/100 (Total: 2)
...
[MAIN] Usage detected! Count: 100/100 (Total: 100)
[CALLBACK] Threshold reached! Sending 100 uses to Firebase...
[MAIN] Sending usage log: 100 uses
[MAIN] HTTP Response code: 200
[CALLBACK] Usage log sent successfully!
```

## 🔍 Troubleshooting

### WiFi Connection Issues

**Symptom**: Device can't connect to WiFi

**Solutions**:
- Verify SSID and password in `secrets.h`
- Check WiFi signal strength
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check router firewall settings

### Firebase Upload Failures

**Symptom**: "HTTP error: 403" or "Connection failed"

**Solutions**:
- Verify Firebase API key is correct
- Check Firestore security rules
- Ensure internet connectivity
- Verify project ID matches Firebase Console

### Time Sync Issues

**Symptom**: "NTP sync failed" warning

**Solutions**:
- Check internet connection
- Wait longer for NTP sync (up to 10 seconds)
- Timestamps will use epoch time if NTP fails

### Memory Issues

**Symptom**: Device crashes or reboots randomly

**Solutions**:
- Monitor free heap in serial output
- Reduce debug output
- Check for memory leaks
- Increase watchdog timeout
