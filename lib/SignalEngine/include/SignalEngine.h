#ifndef SIGNAL_ENGINE_H
#define SIGNAL_ENGINE_H

#include "LedcDriver.h"
#include "signal_iface.h" // Include command/event definitions
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_event.h"
// #include "RmtDriver.h" // Include later when RMT is added
// #include "EngineConfig.h" // Include later for configuration
// #include "signal_iface.h" // Include later for commands/events

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
    // Assuming one LEDC channel for now, based on Phase 1 scope
    int _outputPin; // Store the output pin
    LedcDriver ledcChannel0;

    // Current signal state
    double _currentFrequencyHz;
    float _currentDutyCycle;
    bool _isRunning; // Add a state variable
    uint64_t _startTimeMicros; // Timestamp (us) when current segment started
    uint64_t _accumulatedTicks; // Ticks accumulated before the current segment
    // Duration Tracking
    float _requestedDurationSec;    // Requested duration for current run (0 = infinite)
    uint64_t _durationStartTimeMicros; // Start time for duration measurement (us)

    // State for last applied parameters (used by button)
    double _lastAppliedFrequencyHz;
    float _lastAppliedDutyCycle;
    float _lastAppliedDurationSec;

    // RmtDriver rmtChannelX; // Add RMT driver instance later

    QueueHandle_t xQueueCmd;           // Queue for receiving SignalCmd structs
    TaskHandle_t xCmdDispatcherHandle; // Handle for the command dispatcher task

    // Task function for processing commands
    static void cmdDispatcherTask(void *pvParameters);

    // Add event posting mechanism later
}; // End of SignalEngine class definition

#endif // SIGNAL_ENGINE_H 