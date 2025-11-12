#include "SignalEngine.h"
#include <Arduino.h>
#include "build_opts.h"
#include "signal_iface.h"
#include "param_helpers.h"
#include "esp_event.h"
#include "esp_timer.h"

// Define the event base we declared in signal_iface.h
ESP_EVENT_DEFINE_BASE(SIGNAL_EVENTS);

// Heartbeat task for debugging
void heartbeat_task(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        Serial.println("alive");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Print "alive" every 5 seconds
    }
}

SignalEngine::SignalEngine() :
    pulseGen(),
    _state(),
    _persistence(),
    _timingController(),
    _eventPublisher(),
    _commandDispatcher(pulseGen, _state, _persistence, _timingController, _eventPublisher),
    _initialized(false)
{
    // State initialization is handled by SignalState constructor
    // Persistence initialization is handled by SignalPersistence constructor
    // Timing initialization is handled by TimingController constructor
    // Event publishing initialization is handled by SignalEventPublisher constructor
    // Command dispatcher initialization with injected dependencies
}

bool SignalEngine::begin() {
    Serial.println("SignalEngine: Initializing...");
    _initialized = false; // Clear initialization flag

    // Initialize SignalState (creates mutex for thread safety)
    if (!_state.begin()) {
        Serial.println("SignalEngine: CRITICAL - Failed to initialize SignalState!");
        Serial.println("SignalEngine: Initialization FAILED");
        return false;
    }

    // Reset timing state
    _state.setStartTimeMicros(0);
    _state.resetAccumulatedTicks();
    _state.setRequestedDurationSec(0.0f);
    _state.setDurationStartTimeMicros(0);

    // --- Load Settings from NVS using SignalPersistence ---
    Serial.println("SignalEngine: Loading settings from NVS...");
    SignalSettings settings = _persistence.loadSettings();

    // If no valid settings were loaded, use defaults (already set by loadSettings)
    if (!settings.valid) {
        Serial.println("SignalEngine: WARNING - Using default settings (NVS not accessible)");
    }

    // Apply loaded/default settings to internal state via SignalState
    _state.setCurrentFrequencyHz(settings.frequencyHz);
    _state.setCurrentDutyCycle(settings.dutyCycle);
    _state.setLastAppliedFrequencyHz(settings.frequencyHz);
    _state.setLastAppliedDutyCycle(settings.dutyCycle);
    _state.setLastAppliedDurationSec(settings.durationSec);
    _state.setOutputPin(settings.outputPin);

    // _isRunning remains false initially (SignalState default)
    // _requestedDurationSec will be set by START command
    // --- End Load Settings --- 

    // Initialize the PulseGenerator
    if (!pulseGen.begin()) {
        Serial.println("SignalEngine: CRITICAL - Failed to initialize PulseGenerator!");
        Serial.println("SignalEngine: Initialization FAILED");
        return false;
    }

    // Configure master channel (Channel 0) with loaded settings
    pulseGen.configureMaster((uint8_t)_state.getOutputPin());

    // Set frequency and duty cycle
    pulseGen.setFrequency(_state.getCurrentFrequencyHz());
    pulseGen.setDutyCycle(0, _state.getCurrentDutyCycle());

    // Running state already set to false by SignalState constructor

    Serial.printf("SignalEngine: Initialized (Stopped). Master channel on Pin %d, F=%.2f Hz, D=%.2f%%\n",
                  _state.getOutputPin(), _state.getCurrentFrequencyHz(), _state.getCurrentDutyCycle() * 100.0);

    // Initialize CommandDispatcher (creates queue and task)
    if (!_commandDispatcher.begin()) {
        Serial.println("SignalEngine: CRITICAL - Failed to initialize CommandDispatcher!");
        Serial.println("SignalEngine: Initialization FAILED");
        return false;
    }

    // Create the heartbeat task (optional, non-critical)
    BaseType_t heartbeatCreated = xTaskCreate(
        heartbeat_task,
        "HeartbeatTask",
        1024,
        NULL,
        1,
        NULL
    );

    if (heartbeatCreated != pdPASS) {
        Serial.println("SignalEngine: WARNING - Failed to create heartbeat task (non-critical)");
    } else {
        Serial.println("SignalEngine: Heartbeat task started.");
    }

    _initialized = true; // Mark as successfully initialized
    Serial.println("SignalEngine: Initialization complete - READY");
    return true;
}

void SignalEngine::loop() {
    // This loop is called from the main application loop.
    // Check for auto-stop conditions using TimingController

    // TimingController handles all timeout logic and state updates
    bool timeoutTriggered = _timingController.checkTimeouts(_state, [this]() {
        // Callback invoked when timeout detected - send STOP command
        SignalCmd stopCmd = { .type = SIG_CMD_STOP };
        sendCommand(stopCmd);
    });

    // Other non-blocking checks can go here later.
}

// --- Public Methods ---

bool SignalEngine::isInitialized() const {
    return _initialized;
}

bool SignalEngine::sendCommand(const SignalCmd& cmd) {
    if (!_initialized) {
        Serial.println("SignalEngine::sendCommand - ERROR: Engine not initialized!");
        return false;
    }

    // Delegate to CommandDispatcher
    return _commandDispatcher.sendCommand(cmd);
}

// --- Status Getters Implementation ---
// All getters now delegate to SignalState which handles thread safety

double SignalEngine::getCurrentFrequencyHz() const {
    return _state.getCurrentFrequencyHz();
}

float SignalEngine::getCurrentDutyCycle() const {
    return _state.getCurrentDutyCycle();
}

bool SignalEngine::isRunning() const {
    return _state.isRunning();
}

SignalError SignalEngine::getCurrentStatus(SignalStatus_t& status) {
    // Use atomicUpdate to read multiple state values consistently
    _state.atomicUpdate([this, &status]() {
        status.channel = DEFAULT_LEDC_CHANNEL;
        status.frequency = _state.getCurrentFrequencyHz_nolock();
        status.dutyCycle = _state.getCurrentDutyCycle_nolock();
        status.isRunning = _state.isRunning_nolock();
        status.lastAppliedDurationSec = _state.getLastAppliedDurationSec_nolock();
    });

    return SIG_OK;
}

uint64_t SignalEngine::getEstimatedCycleCount() const {
    // Delegate to TimingController
    return _timingController.getEstimatedCycleCount(_state);
}

// --- Last Applied Parameter Getters Implementation ---
double SignalEngine::getLastAppliedFrequencyHz() const {
    return _state.getLastAppliedFrequencyHz();
}

float SignalEngine::getLastAppliedDutyCycle() const {
    return _state.getLastAppliedDutyCycle();
}

float SignalEngine::getLastAppliedDurationSec() const {
    return _state.getLastAppliedDurationSec();
}

// --- Getter for Output Pin ---
int SignalEngine::getOutputPin() const {
    return _state.getOutputPin();
}

// --- Channel Configuration Getters ---
bool SignalEngine::getChannelConfig(uint8_t channel_id, PulseChannelConfig_t& config) const {
    return pulseGen.getChannelConfig(channel_id, config);
}

float SignalEngine::getChannelPhaseOffset(uint8_t channel_id) const {
    return pulseGen.getPhaseOffset(channel_id);
}

SignalPolarity SignalEngine::getChannelPolarity(uint8_t channel_id) const {
    return pulseGen.getPolarity(channel_id);
}

// --- Indicator Pin Getter ---
uint8_t SignalEngine::getIndicatorPin() const {
    return pulseGen.getIndicatorPin();
}

