# Configuration and Event Handler Robustness Improvements

## Current Critical Issues

### 1. ⚠️ ESP Event Loop Not Initialized
**File:** `apps/production_fw/main.cpp`
**Issue:** The default event loop is NEVER created, yet we're posting events to it.

**Current Code:**
```cpp
void setup() {
    Serial.begin(115200);
    signalEngine.begin();  // ❌ Posts to event loop that doesn't exist!
    webFacade.begin();     // ❌ Registers handlers for non-existent loop!
}
```

**Impact:**
- ❌ Event posting silently fails (returns ESP_ERR_INVALID_STATE)
- ❌ WebSocket updates never happen
- ❌ No error is caught because we don't check return values

**Fix:**
```cpp
void setup() {
    Serial.begin(115200);

    // Initialize ESP-IDF event loop FIRST
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        Serial.printf("CRITICAL: Failed to create event loop: %s\n",
                     esp_err_to_name(err));
        while(1) { delay(1000); } // Halt system
    }
    Serial.println("Event loop initialized");

    signalEngine.begin();
    webFacade.begin();
}
```

---

### 2. ⚠️ Invalid Pin Validation (SignalEngine.cpp:72)
**Issue:** Pin validation still uses old range (12-19) instead of (0-33)

**Current Code:**
```cpp
if (_outputPin < 12 || _outputPin > 19) {  // ❌ WRONG!
    Serial.printf("SignalEngine: Warning - Invalid pin %d loaded from NVS. Using default %d.\n",
                  _outputPin, DEFAULT_OUTPUT_PIN);
    _outputPin = DEFAULT_OUTPUT_PIN;
}
```

**Fix:**
```cpp
// Valid ESP32 GPIO pins (not all are safe, but these are common)
// Exclude: 6-11 (flash), 1/3 (UART), 0 (boot strapping)
const uint8_t VALID_PINS[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};

bool isValidOutputPin(uint8_t pin) {
    for (uint8_t valid : VALID_PINS) {
        if (pin == valid) return true;
    }
    return false;
}

// In begin():
if (!isValidOutputPin(_outputPin)) {
    Serial.printf("SignalEngine: ERROR - Invalid pin %d loaded from NVS. Using default %d.\n",
                  _outputPin, DEFAULT_OUTPUT_PIN);
    _outputPin = DEFAULT_OUTPUT_PIN;

    // Save corrected pin back to NVS
    preferences.begin(DEVICE_CFG_NAMESPACE, false);
    preferences.putUChar(OUTPUT_PIN_KEY, _outputPin);
    preferences.end();
}
```

---

### 3. ⚠️ No NVS Error Handling
**Issue:** `preferences.begin()` can fail, but we never check

**Current Code:**
```cpp
Preferences preferences;
preferences.begin(NVS_NAMESPACE, false); // ❌ Unchecked - might fail!
double loadedFreq = preferences.getDouble(NVS_KEY_FREQ, DEFAULT_FREQUENCY_HZ);
```

**Fix:**
```cpp
Preferences preferences;
if (!preferences.begin(NVS_NAMESPACE, false)) {
    Serial.println("SignalEngine: ERROR - Failed to open NVS namespace!");
    Serial.println("SignalEngine: Using default values");
    // Use compile-time defaults
    _currentFrequencyHz = DEFAULT_FREQUENCY_HZ;
    _currentDutyCycle = DEFAULT_DUTY_CYCLE;
    _outputPin = DEFAULT_OUTPUT_PIN;
} else {
    // Load values with validation
    double loadedFreq = preferences.getDouble(NVS_KEY_FREQ, DEFAULT_FREQUENCY_HZ);
    float loadedDuty = preferences.getFloat(NVS_KEY_DUTY, DEFAULT_DUTY_CYCLE);

    // Validate loaded values
    if (loadedFreq <= 0 || loadedFreq > 8000000) {
        Serial.printf("WARNING: Invalid frequency %.2f Hz in NVS, using default\n", loadedFreq);
        loadedFreq = DEFAULT_FREQUENCY_HZ;
    }

    if (loadedDuty < 0.0f || loadedDuty > 1.0f) {
        Serial.printf("WARNING: Invalid duty cycle %.2f in NVS, using default\n", loadedDuty);
        loadedDuty = DEFAULT_DUTY_CYCLE;
    }

    _currentFrequencyHz = loadedFreq;
    _currentDutyCycle = loadedDuty;

    preferences.end();
}
```

---

### 4. ⚠️ Critical Resource Failures Not Handled
**Issue:** Queue/mutex creation failures don't halt the system properly

**Current Code:**
```cpp
xQueueCmd = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
if (xQueueCmd == NULL) {
    Serial.println("SignalEngine: Error creating command queue!");
    return;  // ❌ Continues initialization anyway!
}
```

**Problem:** After `return`, main.cpp still tries to use SignalEngine!

**Fix Option 1 - Halt System:**
```cpp
xQueueCmd = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
if (xQueueCmd == NULL) {
    Serial.println("CRITICAL: Failed to create command queue!");
    Serial.println("System halted - reboot required");
    while(1) { delay(1000); } // Halt forever
}
```

**Fix Option 2 - Add Initialization State:**
```cpp
// In SignalEngine.h:
private:
    bool _initialized;

// In SignalEngine.cpp:
bool SignalEngine::begin() {
    _initialized = false;

    xQueueCmd = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
    if (xQueueCmd == NULL) {
        Serial.println("CRITICAL: Failed to create command queue!");
        return false;
    }

    // ... rest of initialization ...

    _initialized = true;
    return true;
}

bool SignalEngine::sendCommand(const SignalCmd& cmd) {
    if (!_initialized) {
        Serial.println("ERROR: SignalEngine not initialized!");
        return false;
    }
    // ... rest of method ...
}
```

**In main.cpp:**
```cpp
void setup() {
    // ...
    if (!signalEngine.begin()) {
        Serial.println("FATAL: SignalEngine initialization failed!");
        while(1) { delay(1000); }
    }
    // ...
}
```

---

### 5. ⚠️ Event Handler Registration Failures Not Checked
**Issue:** Event handler registration can fail silently

**Current Code (WebSocketHub.cpp:29):**
```cpp
esp_err_t reg_err = esp_event_handler_register(SIGNAL_EVENTS, ESP_EVENT_ANY_ID,
                                                &espEventHandler, this);
if (reg_err != ESP_OK) {
    Serial.printf("WebSocketHub: Error registering ESP event handler: %s\n",
                  esp_err_to_name(reg_err));
    // ❌ Continues anyway - WebSocket updates will never work!
}
```

**Fix - Make Registration Mandatory:**
```cpp
bool WebSocketHub::begin() {
    // ... WebSocket setup ...

    // Register event handler - CRITICAL for functionality
    esp_err_t reg_err = esp_event_handler_register(SIGNAL_EVENTS, ESP_EVENT_ANY_ID,
                                                    &espEventHandler, this);
    if (reg_err != ESP_OK) {
        Serial.printf("CRITICAL: Event handler registration failed: %s\n",
                     esp_err_to_name(reg_err));

        if (reg_err == ESP_ERR_INVALID_STATE) {
            Serial.println("  Likely cause: Event loop not created!");
            Serial.println("  Call esp_event_loop_create_default() in setup()");
        }
        return false; // Signal failure
    }

    Serial.println("WebSocketHub: Event handler registered successfully");
    return true;
}
```

**In WebFacade.cpp:**
```cpp
bool WebFacade::begin() {
    // ... WiFi setup ...

    if (!_wsHub.begin()) {
        Serial.println("ERROR: WebSocket hub initialization failed!");
        Serial.println("WebSocket updates will not work!");
        // Decide: continue without WebSocket, or halt?
    }

    // ...
}
```

---

### 6. ⚠️ Event Posting Without Timeout
**Issue:** Event posting uses `portMAX_DELAY` which can block forever

**Current Code (SignalEngine.cpp:911):**
```cpp
esp_err_t post_err = esp_event_post(SIGNAL_EVENTS, eventId, &eventData,
                                    sizeof(eventData), portMAX_DELAY);
```

**Problem:** If event queue is full, this blocks the command dispatcher task forever!

**Fix:**
```cpp
const TickType_t EVENT_POST_TIMEOUT_MS = 100; // 100ms timeout

esp_err_t post_err = esp_event_post(SIGNAL_EVENTS, eventId, &eventData,
                                    sizeof(eventData),
                                    pdMS_TO_TICKS(EVENT_POST_TIMEOUT_MS));
if (post_err == ESP_ERR_TIMEOUT) {
    Serial.printf("WARNING: Event queue full, event %d dropped\n", eventId);
} else if (post_err != ESP_OK) {
    Serial.printf("ERROR: Failed to post event %d: %s\n",
                 eventId, esp_err_to_name(post_err));
}
```

---

### 7. ⚠️ No Event Handler Exception Safety
**Issue:** If a handler crashes, it kills the entire event loop

**Fix - Add Watchdog and Error Isolation:**
```cpp
void WebSocketHub::espEventHandler(void* handler_arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
    if (event_base != SIGNAL_EVENTS) return;

    WebSocketHub* hub = static_cast<WebSocketHub*>(handler_arg);
    if (!hub) {
        Serial.println("ERROR: NULL hub pointer in event handler!");
        return;
    }

    // Isolate handler execution
    try {
        SignalEvtData* data = static_cast<SignalEvtData*>(event_data);
        if (!data) {
            Serial.println("ERROR: NULL event data in handler!");
            return;
        }

        hub->broadcastStatus(event_id, *data);

    } catch (...) {
        Serial.printf("EXCEPTION in WebSocketHub event handler for event %ld\n",
                     event_id);
        // Don't rethrow - isolate the error
    }
}
```

---

## Configuration Robustness Improvements

### 8. Add Configuration Versioning

**Create config_version.h:**
```cpp
#pragma once

#define CONFIG_VERSION 1

typedef struct {
    uint8_t version;
    uint8_t outputPin;
    double frequencyHz;
    float dutyCycle;
    float durationSec;
    uint32_t crc32; // Checksum for corruption detection
} ConfigData_t;

uint32_t calculateCRC32(const uint8_t* data, size_t length);
bool saveConfig(const ConfigData_t* config);
bool loadConfig(ConfigData_t* config);
```

**Benefits:**
- Detect NVS corruption with CRC
- Migrate old configs to new format
- Atomic config updates

---

### 9. Add Configuration Bounds Checking Helper

**In param_helpers.h:**
```cpp
// Validate configuration values
static inline bool isValidFrequency(double freq) {
    return freq > 0.0 && freq <= 8000000.0; // 0-8MHz for ESP32 MCPWM
}

static inline bool isValidDutyCycle(float duty) {
    return duty >= 0.0f && duty <= 1.0f;
}

static inline bool isValidDuration(float duration) {
    return duration >= 0.0f; // 0 = infinite
}

static inline bool isValidPin(uint8_t pin) {
    const uint8_t VALID_PINS[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19,
                                  21, 22, 23, 25, 26, 27, 32, 33};
    for (uint8_t valid : VALID_PINS) {
        if (pin == valid) return true;
    }
    return false;
}
```

---

### 10. Add System Health Monitor

**Create SystemHealth.h:**
```cpp
#pragma once

class SystemHealth {
public:
    static void checkCriticalResources() {
        // Check heap
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 10000) {
            Serial.printf("WARNING: Low heap memory: %u bytes\n", freeHeap);
        }

        // Check tasks
        UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
        if (highWaterMark < 512) {
            Serial.printf("WARNING: Low stack: %u bytes remaining\n",
                         highWaterMark);
        }
    }

    static void logEventQueueStatus() {
        // Log event queue depth (requires custom event loop with monitoring)
    }
};
```

---

## Summary of Required Changes

### Priority 1 - Critical (System Won't Work Without These)
1. ✅ Initialize ESP event loop in main.cpp setup()
2. ✅ Fix pin validation (0-33, not 12-19)
3. ✅ Add return value checks for all critical resources
4. ✅ Handle event handler registration failures

### Priority 2 - High (Prevents Silent Failures)
5. ✅ Add NVS error handling and validation
6. ✅ Add event posting timeout (prevent blocking)
7. ✅ Add initialization state tracking

### Priority 3 - Medium (Improves Reliability)
8. ✅ Add configuration versioning and CRC
9. ✅ Add exception safety to event handlers
10. ✅ Add system health monitoring

---

## Implementation Checklist

- [ ] Add esp_event_loop_create_default() to main.cpp
- [ ] Fix pin validation in SignalEngine.cpp:72
- [ ] Add NVS error handling in SignalEngine::begin()
- [ ] Convert SignalEngine::begin() to return bool
- [ ] Add initialization state tracking (_initialized flag)
- [ ] Fix event handler registration error handling
- [ ] Replace portMAX_DELAY with timeout in event posting
- [ ] Add exception safety to all event handlers
- [ ] Create config version and CRC checking
- [ ] Add parameter validation helpers
- [ ] Add system health monitoring
- [ ] Update all dependent code to check return values

---

## Testing Strategy

After implementing fixes:

1. **Power Cycle Test**: Reboot 100 times, verify settings persist
2. **NVS Corruption Test**: Manually corrupt NVS, verify recovery
3. **Event Stress Test**: Post 1000 events/sec, verify no blocking
4. **Handler Crash Test**: Force exception in handler, verify isolation
5. **Resource Exhaustion**: Fill heap/queue, verify graceful degradation
6. **Pin Configuration Test**: Try all valid/invalid pins
