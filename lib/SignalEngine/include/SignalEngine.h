#ifndef SIGNAL_ENGINE_H
#define SIGNAL_ENGINE_H

#include "PulseGenerator.h"
#include "signal_iface.h" // Include command/event definitions
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_event.h"

// Declare the event base
ESP_EVENT_DECLARE_BASE(SIGNAL_EVENTS);

class SignalEngine {
public:
    SignalEngine();
    void begin();
    void loop(); // For periodic tasks if needed (e.g., heartbeat)
    bool sendCommand(const SignalCmd& cmd); // Send command to the engine's queue

    // --- Status Getters ---
    double getCurrentFrequencyHz() const;
    float getCurrentDutyCycle() const;
    bool isRunning() const; // Simple check if currently active
    uint64_t getEstimatedCycleCount() const; // Getter for calculated cycle count

    // Public methods to get last applied parameters (for button control, etc.)
    double getLastAppliedFrequencyHz() const;
    float getLastAppliedDutyCycle() const;
    float getLastAppliedDurationSec() const;

    // --- Getter for Output Pin ---
    int getOutputPin() const;

    /**
     * @brief Gets the current status of the signal generator.
     * 
     * @param status Reference to a SignalStatus_t struct to be filled.
     * @return SignalError Returns SIG_ERR_NONE on success, or an error code.
     *         (Currently assumes single channel, may need expansion for multi-channel)
     */
    SignalError getCurrentStatus(SignalStatus_t& status);

private:
    // Multi-channel pulse generator (MCPWM-based)
    PulseGenerator pulseGen;

    // Current signal state
    double _currentFrequencyHz;
    float _currentDutyCycle;
    bool _isRunning;
    uint64_t _startTimeMicros; // Timestamp (us) when current segment started
    uint64_t _accumulatedTicks; // Ticks accumulated before the current segment

    // Duration/Count Tracking (use one or the other)
    float _requestedDurationSec;    // Requested duration for current run (0 = infinite)
    uint64_t _requestedPulseCount;  // Requested pulse count (0 = infinite, overrides duration)
    uint64_t _durationStartTimeMicros; // Start time for duration measurement (us)
    bool _usePulseCount;            // True if using pulse count, false if using duration

    // State for last applied parameters (used by button)
    double _lastAppliedFrequencyHz;
    float _lastAppliedDutyCycle;
    float _lastAppliedDurationSec;
    uint64_t _lastAppliedPulseCount;

    // Legacy: Keep track of primary output pin for compatibility
    int _outputPin;

    QueueHandle_t xQueueCmd;           // Queue for receiving SignalCmd structs
    TaskHandle_t xCmdDispatcherHandle; // Handle for the command dispatcher task

    // Task function for processing commands
    static void cmdDispatcherTask(void *pvParameters);

    // Add event posting mechanism later
}; // End of SignalEngine class definition

#endif // SIGNAL_ENGINE_H 