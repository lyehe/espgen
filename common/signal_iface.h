// common/signal_iface.h
#pragma once

#include <stdint.h>
#include "esp_event.h" // Include for ESP Event Loop types

// Declare an event base for SignalEngine events
ESP_EVENT_DECLARE_BASE(SIGNAL_EVENTS);

// Forward declarations or basic types can go here

// Command Type Enum
typedef enum {
    SIG_CMD_START,           // Start generating signal with current/default parameters
    SIG_CMD_STOP,            // Stop generating signal
    SIG_CMD_UPDATE_FREQ,     // Update frequency only (all channels)
    SIG_CMD_UPDATE_DUTY,     // Update duty cycle only (single or all channels)
    SIG_CMD_UPDATE_ALL,      // Update both frequency and duty cycle
    SIG_CMD_SET_PIN,         // Set the output GPIO pin for a channel
    SIG_CMD_SET_INDICATOR,   // Set the status indicator GPIO pin
    SIG_CMD_CONFIG_CHANNEL,  // Configure channel (pin, phase offset, enable)
    SIG_CMD_ENABLE_CHANNEL,  // Enable/disable a specific channel
    SIG_CMD_SYNC,            // Trigger synchronization
    SIG_CMD_SET_PARAMS,      // Set exact pulse parameters (new comprehensive command)
    SIG_CMD_SWEEP            // PLACEHOLDER for future sweep/modulation commands
} SigCmdType;

// Parameter specification modes (bitflags)
typedef enum {
    PARAM_USE_FREQUENCY     = 0x01,  // Use frequencyHz field
    PARAM_USE_PERIOD        = 0x02,  // Use periodUs field instead
    PARAM_USE_DUTY_CYCLE    = 0x04,  // Use dutyCycle field (0.0-1.0)
    PARAM_USE_PULSE_WIDTH   = 0x08,  // Use pulseWidthUs field instead
    PARAM_USE_DURATION      = 0x10,  // Use durationSec field
    PARAM_USE_PULSE_COUNT   = 0x20,  // Use pulseCount field instead
    PARAM_USE_PHASE_DEGREES = 0x40,  // Use phaseOffset field (degrees)
    PARAM_USE_PHASE_TIME    = 0x80,  // Use phaseDelayUs field instead
} ParamMode;

// Polarity options
typedef enum {
    POLARITY_ACTIVE_HIGH = 0,   // Normal: pulse is high
    POLARITY_ACTIVE_LOW = 1     // Inverted: pulse is low
} SignalPolarity;

// Command Structure - Extended with comprehensive parameters
typedef struct {
    SigCmdType type;
    uint8_t channel;         // Channel number (0-5 for multi-channel support)
    uint8_t pin;             // GPIO pin number (used for SIG_CMD_SET_PIN, SIG_CMD_CONFIG_CHANNEL)

    // Timing: Frequency OR Period (use paramMode to select)
    union {
        double frequencyHz;      // Frequency in Hz
        uint32_t periodUs;       // Period in microseconds (alternative to frequency)
    };

    // Pulse width: Duty Cycle OR Pulse Width (use paramMode to select)
    union {
        float dutyCycle;         // Duty cycle 0.0 to 1.0
        uint32_t pulseWidthUs;   // Pulse width in microseconds (alternative to duty cycle)
    };

    // Run time: Duration OR Pulse Count (use paramMode to select)
    union {
        float durationSec;       // Duration in seconds (0=infinite)
        uint64_t pulseCount;     // Exact number of pulses (alternative to duration, 0=infinite)
    };

    // Phase: Degrees OR Time Delay (use paramMode to select)
    union {
        float phaseOffset;       // Phase offset in degrees
        uint32_t phaseDelayUs;   // Phase delay in microseconds (alternative to degrees)
    };

    // Additional timing
    uint32_t startDelayUs;   // Delay before first pulse (microseconds, future feature)

    // Control flags
    uint16_t paramMode;      // Bitfield of ParamMode flags indicating which params to use
    SignalPolarity polarity; // Signal polarity (active high/low)
    bool enabled;            // Enable/disable flag (used for SIG_CMD_ENABLE_CHANNEL)

    // Advanced features (future expansion)
    uint8_t burstCount;      // Pulses per burst (0=continuous, future feature)
    uint32_t burstPeriodUs;  // Time between bursts (future feature)
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
    uint8_t channel;        // The channel number
    float frequency;        // Current frequency in Hz
    float dutyCycle;        // Current duty cycle (0.0 to 1.0)
    bool isRunning;         // True if the channel is actively generating a signal
    float lastAppliedDurationSec; // Last applied duration
    uint32_t periodUs;      // Current period in microseconds
    uint32_t pulseWidthUs;  // Current pulse width in microseconds
} SignalStatus_t;

/**
 * @brief Comprehensive pulse parameters structure
 *
 * Allows specifying pulse characteristics in multiple ways:
 * - Frequency OR Period
 * - Duty Cycle OR Pulse Width
 * - Duration OR Pulse Count
 * - Phase in Degrees OR Time Delay
 */
typedef struct {
    // Timing specification (use one of each pair)
    union {
        double frequencyHz;     // Frequency in Hz (0.001 Hz to 40 MHz)
        uint32_t periodUs;      // Period in microseconds
    };

    union {
        float dutyCycle;        // Duty cycle 0.0 to 1.0
        uint32_t pulseWidthUs;  // Pulse width in microseconds
    };

    union {
        float durationSec;      // Duration in seconds (0 = infinite)
        uint64_t pulseCount;    // Exact number of pulses (0 = infinite)
    };

    union {
        float phaseOffsetDeg;   // Phase offset in degrees (0-360)
        uint32_t phaseDelayUs;  // Phase delay in microseconds
    };

    // Additional timing
    uint32_t startDelayUs;          // Delay before first pulse starts

    // Control
    uint16_t paramMode;             // ParamMode bitflags
    SignalPolarity polarity;        // Active high or low

    // Advanced (future)
    uint8_t burstCount;             // Pulses per burst
    uint32_t burstPeriodUs;         // Period between bursts
} PulseParams_t;

// Helper functions for parameter conversion
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert frequency to period
 * @param freqHz Frequency in Hz
 * @return Period in microseconds
 */
static inline uint32_t freqToPeriodUs(double freqHz) {
    if (freqHz <= 0) return 0;
    return (uint32_t)(1000000.0 / freqHz);
}

/**
 * @brief Convert period to frequency
 * @param periodUs Period in microseconds
 * @return Frequency in Hz
 */
static inline double periodToFreqHz(uint32_t periodUs) {
    if (periodUs == 0) return 0;
    return 1000000.0 / (double)periodUs;
}

/**
 * @brief Convert duty cycle and period to pulse width
 * @param dutyCycle Duty cycle (0.0 to 1.0)
 * @param periodUs Period in microseconds
 * @return Pulse width in microseconds
 */
static inline uint32_t dutyToPulseWidthUs(float dutyCycle, uint32_t periodUs) {
    if (dutyCycle < 0.0f) dutyCycle = 0.0f;
    if (dutyCycle > 1.0f) dutyCycle = 1.0f;
    return (uint32_t)(dutyCycle * periodUs);
}

/**
 * @brief Convert pulse width and period to duty cycle
 * @param pulseWidthUs Pulse width in microseconds
 * @param periodUs Period in microseconds
 * @return Duty cycle (0.0 to 1.0)
 */
static inline float pulseWidthToDuty(uint32_t pulseWidthUs, uint32_t periodUs) {
    if (periodUs == 0) return 0.0f;
    float duty = (float)pulseWidthUs / (float)periodUs;
    if (duty > 1.0f) duty = 1.0f;
    return duty;
}

/**
 * @brief Convert duration to pulse count
 * @param durationSec Duration in seconds
 * @param freqHz Frequency in Hz
 * @return Number of pulses
 */
static inline uint64_t durationToPulseCount(float durationSec, double freqHz) {
    if (durationSec <= 0 || freqHz <= 0) return 0;
    return (uint64_t)(durationSec * freqHz);
}

/**
 * @brief Convert pulse count to duration
 * @param pulseCount Number of pulses
 * @param freqHz Frequency in Hz
 * @return Duration in seconds
 */
static inline float pulseCountToDuration(uint64_t pulseCount, double freqHz) {
    if (pulseCount == 0 || freqHz <= 0) return 0;
    return (float)pulseCount / (float)freqHz;
}

/**
 * @brief Convert phase degrees to time delay
 * @param phaseDeg Phase in degrees (0-360)
 * @param periodUs Period in microseconds
 * @return Phase delay in microseconds
 */
static inline uint32_t phaseToDelayUs(float phaseDeg, uint32_t periodUs) {
    // Normalize phase to 0-360
    while (phaseDeg < 0) phaseDeg += 360.0f;
    while (phaseDeg >= 360.0f) phaseDeg -= 360.0f;
    return (uint32_t)((phaseDeg / 360.0f) * periodUs);
}

/**
 * @brief Convert phase delay to degrees
 * @param phaseDelayUs Phase delay in microseconds
 * @param periodUs Period in microseconds
 * @return Phase in degrees (0-360)
 */
static inline float delayToPhaseDeg(uint32_t phaseDelayUs, uint32_t periodUs) {
    if (periodUs == 0) return 0;
    return ((float)phaseDelayUs / (float)periodUs) * 360.0f;
}

#ifdef __cplusplus
}
#endif 