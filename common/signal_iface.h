// common/signal_iface.h
#pragma once

#include <stdint.h>
#include "esp_event.h" // Include for ESP Event Loop types

// Declare an event base for SignalEngine events
ESP_EVENT_DECLARE_BASE(SIGNAL_EVENTS);

// Forward declarations or basic types can go here

// Command Type Enum
typedef enum {
    SIG_CMD_START,       // Start generating signal with current/default parameters
    SIG_CMD_STOP,        // Stop generating signal
    SIG_CMD_UPDATE_FREQ, // Update frequency only
    SIG_CMD_UPDATE_DUTY, // Update duty cycle only
    SIG_CMD_UPDATE_ALL,  // Update both frequency and duty cycle
    SIG_CMD_SET_PIN,     // Set the output GPIO pin
    SIG_CMD_SWEEP        // PLACEHOLDER for future sweep/modulation commands
} SigCmdType;

// Command Structure
typedef struct {
    SigCmdType type;
    uint8_t pin;             // GPIO pin number (used for SIG_CMD_SET_PIN)
    // uint8_t channel; // Re-introduce later if multi-channel support is needed
    double frequencyHz;  // Frequency in Hz (used for UPDATE_FREQ, UPDATE_ALL, START)
    float dutyCycle;     // Duty cycle 0.0 to 1.0 (used for UPDATE_DUTY, UPDATE_ALL, START)
    float durationSec;   // Optional duration in seconds for START command (0=infinite)
} SignalCmd;

// Event Type Enum (IDs for events within SIGNAL_EVENTS base)
typedef enum {
    SIG_EVT_STARTED,         // Event ID 0
    SIG_EVT_STOPPED,         // Event ID 1
    SIG_EVT_PARAMS_CHANGED,  // Event ID 2
} SigEvtId; // Renamed from SigEvtType for clarity with ESP-IDF

// Event Data Structure (sent with the event)
typedef struct {
    // SigEvtId type; // Type is now the event_id, not needed in data struct
    uint8_t channel; // Default to 0 for now
    uint32_t current_freq;
    float current_duty;
    float duration_sec; // Last applied/active duration (0 if infinite)
    uint64_t current_ticks; // Number of ticks/updates since started
    uint8_t output_pin; // Current output pin
} SignalEvtData; // Renamed from SignalEvt

// Common Error Codes (optional, can use ESP-IDF codes too)
typedef enum {
    SIG_OK = 0,
    SIG_ERR_INVALID_PARAM = -1,
    SIG_ERR_QUEUE_FULL = -2,
    // ... add more error codes
} SignalError;

/**
 * @brief Represents the current status of a signal channel.
 */
typedef struct {
    uint8_t channel;        // The channel number (e.g., LEDC channel)
    float frequency;      // Current frequency in Hz
    float dutyCycle;      // Current duty cycle (0.0 to 1.0)
    bool isRunning;       // True if the channel is actively generating a signal
    float lastAppliedDurationSec; // Add field for last applied/loaded duration
} SignalStatus_t; 