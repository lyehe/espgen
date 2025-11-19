#ifndef MOCKS_H
#define MOCKS_H

// Mock implementations for ESP32-specific dependencies for native testing

#include <stdint.h>
#include <string.h>
#include <cmath>
#include <functional>
#include <map>
#include <string>

// Make sure math functions are available
// Note: isfinite and fmodf are in global namespace in C, brought into std:: in C++
// For compatibility, we'll ensure they're available in global namespace
#ifndef isfinite
using std::isfinite;
#endif

// fmodf is the float version, fmod is double - just include it from global namespace
#include <math.h>

// ==================== Mock Arduino Types ====================

#ifdef NATIVE_TEST

// Mock Serial
class MockSerial {
public:
    void begin(unsigned long baud) {}
    void println(const char* str) {}
    void printf(const char* format, ...) {}
    operator bool() const { return true; }
};

extern MockSerial Serial;

// Mock delay
inline void delay(unsigned long ms) {}
inline void vTaskDelay(unsigned long ticks) {}
inline unsigned long millis() { return 0; }

#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFF

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t StackType_t;

#define ESP_OK 0
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_STATE 0x103

typedef int esp_err_t;

inline const char* esp_err_to_name(esp_err_t err) {
    switch(err) {
        case ESP_OK: return "ESP_OK";
        case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
        case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
        default: return "UNKNOWN_ERROR";
    }
}

// Mock FreeRTOS functions
inline QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize) { return (QueueHandle_t)1; }
inline BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait) { return pdPASS; }
inline BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t wait) { return pdFALSE; }
inline void vQueueDelete(QueueHandle_t queue) {}

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait) { return pdTRUE; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) { return pdTRUE; }
inline void vSemaphoreDelete(SemaphoreHandle_t sem) {}

inline BaseType_t xTaskCreate(void (*taskFunc)(void*), const char* name, uint32_t stackSize,
                               void* params, UBaseType_t priority, TaskHandle_t* handle) {
    *handle = (TaskHandle_t)1;
    return pdPASS;
}
inline void vTaskDelete(TaskHandle_t task) {}
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) { return 1024; }

// Mock ESP timer
// Note: Tests can override this by defining their own esp_timer_get_time()
// before including this header, or by declaring it extern and defining it
// in the test file.
uint64_t esp_timer_get_time();

// Mock ESP event system
typedef void* esp_event_loop_handle_t;
typedef const char* esp_event_base_t;  // Changed to match real ESP-IDF

#define ESP_EVENT_DECLARE_BASE(name) extern esp_event_base_t name
#define ESP_EVENT_DEFINE_BASE(name) esp_event_base_t name = #name

inline esp_err_t esp_event_loop_create_default() { return ESP_OK; }

inline esp_err_t esp_event_post(esp_event_base_t event_base, int32_t event_id,
                                 const void* event_data, size_t event_data_size,
                                 TickType_t ticks_to_wait) {
    return ESP_OK;
}

inline esp_err_t esp_event_handler_register(esp_event_base_t event_base, int32_t event_id,
                                             void* handler, void* arg) {
    return ESP_OK;
}

// Mock Preferences (NVS)
class MockPreferences {
public:
    bool begin(const char* name, bool readOnly) {
        currentNamespace = name;
        return nvs_accessible;
    }

    void end() {
        currentNamespace = "";
    }

    double getDouble(const char* key, double defaultValue) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        if (doubleStore.find(fullKey) != doubleStore.end()) {
            return doubleStore[fullKey];
        }
        return defaultValue;
    }

    float getFloat(const char* key, float defaultValue) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        if (floatStore.find(fullKey) != floatStore.end()) {
            return floatStore[fullKey];
        }
        return defaultValue;
    }

    uint8_t getUChar(const char* key, uint8_t defaultValue) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        if (ucharStore.find(fullKey) != ucharStore.end()) {
            return ucharStore[fullKey];
        }
        return defaultValue;
    }

    size_t putDouble(const char* key, double value) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        doubleStore[fullKey] = value;
        return sizeof(double);
    }

    size_t putFloat(const char* key, float value) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        floatStore[fullKey] = value;
        return sizeof(float);
    }

    size_t putUChar(const char* key, uint8_t value) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        ucharStore[fullKey] = value;
        return sizeof(uint8_t);
    }

    bool remove(const char* key) {
        std::string fullKey = std::string(currentNamespace) + ":" + key;
        doubleStore.erase(fullKey);
        floatStore.erase(fullKey);
        ucharStore.erase(fullKey);
        return true;
    }

    size_t clear() {
        // Clear only current namespace
        for (auto it = doubleStore.begin(); it != doubleStore.end(); ) {
            if (it->first.find(currentNamespace + ":") == 0) {
                it = doubleStore.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = floatStore.begin(); it != floatStore.end(); ) {
            if (it->first.find(currentNamespace + ":") == 0) {
                it = floatStore.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = ucharStore.begin(); it != ucharStore.end(); ) {
            if (it->first.find(currentNamespace + ":") == 0) {
                it = ucharStore.erase(it);
            } else {
                ++it;
            }
        }
        return 0;
    }

    // Test helpers
    static void setNvsAccessible(bool accessible) { nvs_accessible = accessible; }
    static void clearAllStores() {
        doubleStore.clear();
        floatStore.clear();
        ucharStore.clear();
    }

private:
    std::string currentNamespace;
    static std::map<std::string, double> doubleStore;
    static std::map<std::string, float> floatStore;
    static std::map<std::string, uint8_t> ucharStore;
    static bool nvs_accessible;
};

typedef MockPreferences Preferences;

#endif // NATIVE_TEST

// ==================== Mock PulseGenerator ====================

#include "signal_iface.h"

// Define MAX_PULSE_CHANNELS if not already defined
#ifndef MAX_PULSE_CHANNELS
#define MAX_PULSE_CHANNELS 6
#endif

// Only define these if we're in native test mode and they haven't been defined yet
#ifndef PULSE_GENERATOR_H

// Channel configuration structure (from PulseGenerator.h)
typedef struct {
    uint8_t gpio_pin;           // GPIO pin for this channel
    float phase_offset_deg;     // Phase offset in degrees (0-360) relative to master
    SignalPolarity polarity;    // Signal polarity
    bool enabled;               // Enable/disable (master channel is always enabled)
    uint8_t skip_count;         // Skip N pulses (0 = no skip, future feature)
} PulseChannelConfig_t;

class MockPulseGenerator {
public:
    MockPulseGenerator() :
        _initialized(false),
        _running(false),
        _frequency(0),
        _dutyCycle{0},
        _phaseOffset{0},
        _polarity{POLARITY_ACTIVE_HIGH},
        _channelEnabled{false},
        _masterPin(0),
        _indicatorPin(0),
        _syncCalled(false)
    {
        for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
            _dutyCycle[i] = 0;
            _phaseOffset[i] = 0;
            _polarity[i] = POLARITY_ACTIVE_HIGH;
            _channelEnabled[i] = false;
        }
    }

    bool begin() {
        _initialized = true;
        return true;
    }

    void configureMaster(uint8_t pin) {
        _masterPin = pin;
        _channelEnabled[0] = true;
    }

    void configureSlave(uint8_t channel, uint8_t pin, float phaseOffset, bool enabled) {
        if (channel < MAX_PULSE_CHANNELS) {
            _phaseOffset[channel] = phaseOffset;
            _channelEnabled[channel] = enabled;
        }
    }

    void setFrequency(double freq) {
        _frequency = freq;
    }

    void setPeriod(uint32_t periodUs) {
        if (periodUs > 0) {
            _frequency = 1000000.0 / periodUs;
        }
    }

    void setDutyCycle(uint8_t channel, float duty) {
        if (channel < MAX_PULSE_CHANNELS) {
            _dutyCycle[channel] = duty;
        }
    }

    void setPulseWidth(uint8_t channel, uint32_t widthUs) {
        // Not fully implemented in mock
    }

    void setPolarity(uint8_t channel, SignalPolarity polarity) {
        if (channel < MAX_PULSE_CHANNELS) {
            _polarity[channel] = polarity;
        }
    }

    void enableChannel(uint8_t channel, bool enabled) {
        if (channel < MAX_PULSE_CHANNELS) {
            _channelEnabled[channel] = enabled;
        }
    }

    void start() {
        _running = true;
    }

    void stop() {
        _running = false;
    }

    void triggerSync() {
        _syncCalled = true;
    }

    void setIndicatorPin(uint8_t pin) {
        _indicatorPin = pin;
    }

    bool getChannelConfig(uint8_t channel_id, PulseChannelConfig_t& config) const {
        if (channel_id >= MAX_PULSE_CHANNELS) return false;
        config.enabled = _channelEnabled[channel_id];
        config.phase_offset_deg = _phaseOffset[channel_id];
        config.polarity = _polarity[channel_id];
        config.gpio_pin = (channel_id == 0) ? _masterPin : 0; // Simplified
        config.skip_count = 0;
        return true;
    }

    float getPhaseOffset(uint8_t channel_id) const {
        if (channel_id >= MAX_PULSE_CHANNELS) return 0;
        return _phaseOffset[channel_id];
    }

    SignalPolarity getPolarity(uint8_t channel_id) const {
        if (channel_id >= MAX_PULSE_CHANNELS) return POLARITY_ACTIVE_HIGH;
        return _polarity[channel_id];
    }

    uint32_t getPeriodUs() const {
        if (_frequency > 0) {
            return (uint32_t)(1000000.0 / _frequency);
        }
        return 0;
    }

    // Test accessors
    bool isInitialized() const { return _initialized; }
    bool isRunning() const { return _running; }
    double getFrequency() const { return _frequency; }
    float getDutyCycle(uint8_t channel) const {
        return (channel < MAX_PULSE_CHANNELS) ? _dutyCycle[channel] : 0;
    }
    uint8_t getMasterPin() const { return _masterPin; }
    bool wasSyncCalled() const { return _syncCalled; }
    void resetSyncCalled() { _syncCalled = false; }

private:
    bool _initialized;
    bool _running;
    double _frequency;
    float _dutyCycle[MAX_PULSE_CHANNELS];
    float _phaseOffset[MAX_PULSE_CHANNELS];
    SignalPolarity _polarity[MAX_PULSE_CHANNELS];
    bool _channelEnabled[MAX_PULSE_CHANNELS];
    uint8_t _masterPin;
    uint8_t _indicatorPin;
    bool _syncCalled;
};

// Alias for tests (only if real PulseGenerator hasn't been defined)
typedef MockPulseGenerator PulseGenerator;

#endif // PULSE_GENERATOR_H guard

#endif // MOCKS_H
