#ifndef SIGNAL_ENGINE_H
#define SIGNAL_ENGINE_H

#include "PulseGenerator.h"
#include "SignalState.h"
#include "SignalPersistence.h"
#include "TimingController.h"
#include "SignalEventPublisher.h"
#include "CommandDispatcher.h"
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
    bool begin(); // Initialize engine - returns false on critical failure
    void loop(); // For periodic tasks if needed (e.g., heartbeat)
    bool sendCommand(const SignalCmd& cmd); // Send command to the engine's queue

    // --- Status Getters ---
    bool isInitialized() const; // Check if initialization succeeded
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

    // --- Channel Configuration Getters ---
    bool getChannelConfig(uint8_t channel_id, PulseChannelConfig_t& config) const;
    float getChannelPhaseOffset(uint8_t channel_id) const;
    SignalPolarity getChannelPolarity(uint8_t channel_id) const;

    // --- Indicator Pin Getter ---
    uint8_t getIndicatorPin() const;

private:
    // Multi-channel pulse generator (MCPWM-based)
    PulseGenerator pulseGen;

    // Centralized state management with thread safety
    SignalState _state;

    // Persistent storage management
    SignalPersistence _persistence;

    // Timing and pulse counting management
    TimingController _timingController;

    // ESP event publishing
    SignalEventPublisher _eventPublisher;

    // Command queue and dispatcher
    CommandDispatcher _commandDispatcher;

    // Initialization state
    bool _initialized;
}; // End of SignalEngine class definition

#endif // SIGNAL_ENGINE_H 