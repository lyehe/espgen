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
    _initialized(false)
{
    // State initialization is handled by SignalState constructor
    // Persistence initialization is handled by SignalPersistence constructor
    // Timing initialization is handled by TimingController constructor
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

    // Create the command queue
    xQueueCmd = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
    if (xQueueCmd == NULL) {
        Serial.println("SignalEngine: CRITICAL - Failed to create command queue!");
        Serial.println("SignalEngine: Initialization FAILED");
        return false;
    }
    Serial.printf("SignalEngine: Command queue created (size: %d)\n", SIGNAL_ENGINE_CMD_QUEUE_LEN);

    // Note: State mutex already created by _state.begin() above

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

    // Create the command dispatcher task
    BaseType_t taskCreated = xTaskCreate(
        cmdDispatcherTask,      // Task function
        "CmdDispatchTask",      // Name of the task
        2048,                   // Stack size in words (adjust as needed)
        this,                   // Task input parameter (pass pointer to this SignalEngine instance)
        5,                      // Priority of the task (higher than heartbeat? adjust as needed)
        &xCmdDispatcherHandle   // Task handle
    );

    if (taskCreated != pdPASS) {
        Serial.println("SignalEngine: CRITICAL - Failed to create command dispatcher task!");
        Serial.println("SignalEngine: Initialization FAILED");
        return false;
    }
    Serial.println("SignalEngine: Command dispatcher task started.");

    // Create the heartbeat task (as per Phase 1 requirements)
    // Stack size might need adjustment later. Priority 1 is low.
    // Note: Heartbeat is optional, so we don't fail initialization if it fails
    BaseType_t heartbeatCreated = xTaskCreate(
        heartbeat_task,         // Task function
        "HeartbeatTask",        // Name of the task
        1024,                   // Stack size in words
        NULL,                   // Task input parameter
        1,                      // Priority of the task
        NULL                    // Task handle
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

// Removed SignalEngine::sendCommand implementation

// Removed SignalEngine::cmdDispatcherTask implementation 

// --- Public Methods ---

bool SignalEngine::isInitialized() const {
    return _initialized;
}

bool SignalEngine::sendCommand(const SignalCmd& cmd) {
    if (!_initialized) {
        Serial.println("SignalEngine::sendCommand - ERROR: Engine not initialized!");
        return false;
    }

    if (xQueueCmd == NULL) {
        Serial.println("SignalEngine::sendCommand - ERROR: Queue not initialized.");
        return false;
    }

    // Send the command to the queue. Wait 0 ticks if the queue is full.
    if (xQueueSend(xQueueCmd, &cmd, (TickType_t)0) != pdPASS) {
        Serial.println("SignalEngine::sendCommand - WARNING: Command queue full.");
        return false; // Indicate failure
    }
    return true; // Indicate success
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

// --- Private Static Task Functions ---

void SignalEngine::cmdDispatcherTask(void *pvParameters) {
    SignalEngine* engine = static_cast<SignalEngine*>(pvParameters);
    SignalCmd receivedCmd;
    SignalEvtData eventData; // Structure to hold event data
    // Note: Preferences/NVS now handled by SignalPersistence

    Serial.println("CmdDispatcherTask: Starting loop.");

    for (;;) {
        if (xQueueReceive(engine->xQueueCmd, &receivedCmd, portMAX_DELAY) == pdPASS) {
            Serial.printf("CmdDispatcherTask: Received command type %d\n", receivedCmd.type);

            // VALIDATION: Check command validity before processing
            if (!validateSignalCmd(&receivedCmd)) {
                Serial.println("CmdDispatcherTask: ERROR - Invalid command parameters, ignoring");
                continue; // Skip this command
            }

            bool stateChanged = false;
            SigEvtId eventId = SIG_EVT_PARAMS_CHANGED; // Default event ID

            switch (receivedCmd.type) {
                case SIG_CMD_START:
                    {
                        Serial.println("CmdDispatcherTask: Processing START");

                        // THREAD SAFETY: Use SignalState atomicUpdate
                        engine->_state.atomicUpdate([&]() {
                            // Update frequency/period (check which mode is being used)
                            if (receivedCmd.paramMode & PARAM_USE_FREQUENCY) {
                                engine->_state.setLastAppliedFrequencyHz_nolock(receivedCmd.frequencyHz);
                            } else if (receivedCmd.paramMode & PARAM_USE_PERIOD) {
                                // Convert period to frequency for internal storage
                                engine->_state.setLastAppliedFrequencyHz_nolock(periodToFreqHz(receivedCmd.periodUs));
                            } else {
                                // Default to current frequency if not specified
                                engine->_state.setLastAppliedFrequencyHz_nolock(engine->_state.getCurrentFrequencyHz_nolock());
                            }

                            // Update duty cycle/pulse width (check which mode is being used)
                            if (receivedCmd.paramMode & PARAM_USE_DUTY_CYCLE) {
                                engine->_state.setLastAppliedDutyCycle_nolock(receivedCmd.dutyCycle);
                            } else if (receivedCmd.paramMode & PARAM_USE_PULSE_WIDTH) {
                                // Convert pulse width to duty cycle for internal storage
                                uint32_t currentPeriodUs = engine->pulseGen.getPeriodUs();
                                if (currentPeriodUs > 0) {
                                    engine->_state.setLastAppliedDutyCycle_nolock(pulseWidthToDuty(receivedCmd.pulseWidthUs, currentPeriodUs));
                                }
                            } else {
                                // Default to current duty if not specified
                                engine->_state.setLastAppliedDutyCycle_nolock(engine->_state.getCurrentDutyCycle_nolock());
                            }

                            // Update duration/pulse count (check which mode is being used)
                            if (receivedCmd.paramMode & PARAM_USE_DURATION) {
                                engine->_state.setLastAppliedDurationSec_nolock(receivedCmd.durationSec);
                                engine->_state.setLastAppliedPulseCount_nolock(0); // Clear pulse count when using duration
                            } else if (receivedCmd.paramMode & PARAM_USE_PULSE_COUNT) {
                                engine->_state.setLastAppliedPulseCount_nolock(receivedCmd.pulseCount);
                                engine->_state.setLastAppliedDurationSec_nolock(0.0f); // Clear duration when using pulse count
                            } else {
                                // Default to no duration/count if not specified
                                engine->_state.setLastAppliedDurationSec_nolock(0.0f);
                                engine->_state.setLastAppliedPulseCount_nolock(0);
                            }

                            Serial.printf(" - Stored Last Applied: F=%.2f Hz, D=%.2f%%",
                                          engine->_state.getLastAppliedFrequencyHz_nolock(),
                                          engine->_state.getLastAppliedDutyCycle_nolock() * 100.0);

                            // Determine if using pulse count or duration
                            if (receivedCmd.paramMode & PARAM_USE_PULSE_COUNT) {
                                Serial.printf(", Pulses=%llu\n", engine->_state.getLastAppliedPulseCount_nolock());
                            } else {
                                Serial.printf(", Dur=%.2f s\n", engine->_state.getLastAppliedDurationSec_nolock());
                            }

                            // Note: NVS saving happens outside atomicUpdate to avoid blocking mutex
                        });

                        // --- Save Settings to NVS using SignalPersistence (outside critical section) ---
                        engine->_persistence.saveSignalParams(
                            engine->_state.getLastAppliedFrequencyHz(),
                            engine->_state.getLastAppliedDutyCycle(),
                            engine->_state.getLastAppliedDurationSec()
                        );
                        // --- End Save Settings ---

                        // Re-enter atomicUpdate for the rest of START command
                        engine->_state.atomicUpdate([&]() {
                            // Continue with START command logic...

                            if (!engine->_state.isRunning_nolock()) { // Record start time only if starting from stopped state
                                 engine->_state.resetAccumulatedTicks_nolock(); // Reset accumulator on new start
                                 engine->_state.setStartTimeMicros_nolock(esp_timer_get_time());
                            } else {
                                // If already running, treat START like an UPDATE_ALL to apply new params
                                // Accumulate ticks from the current segment first
                                uint64_t cycles_just_elapsed = engine->_timingController.calculateCycles(
                                    engine->_state.getStartTimeMicros_nolock(),
                                    esp_timer_get_time(),
                                    engine->_state.getCurrentFrequencyHz_nolock()
                                );
                                engine->_state.addAccumulatedTicks_nolock(cycles_just_elapsed);
                                // New start time for the next segment
                                engine->_state.setStartTimeMicros_nolock(esp_timer_get_time());
                            }

                            // Handle Duration or Pulse Count for START
                            if (receivedCmd.paramMode & PARAM_USE_PULSE_COUNT) {
                                // Use pulse count mode - RESET accumulator for new pulse count
                                // User expects START to begin counting from 0, not continue from accumulated
                                engine->_state.resetAccumulatedTicks_nolock();
                                engine->_state.setStartTimeMicros_nolock(esp_timer_get_time());

                                if (receivedCmd.pulseCount > 0) {
                                    engine->_state.setRequestedPulseCount_nolock(receivedCmd.pulseCount);
                                    engine->_state.setUsingPulseCount_nolock(true);
                                    engine->_state.setRequestedDurationSec_nolock(0); // Clear duration when using pulse count
                                    engine->_state.setDurationStartTimeMicros_nolock(0);
                                    Serial.printf(" - Pulse count set: %llu pulses (accumulator reset)\n",
                                                  engine->_state.getRequestedPulseCount_nolock());
                                } else {
                                    engine->_state.setRequestedPulseCount_nolock(0); // Infinite pulses
                                    engine->_state.setUsingPulseCount_nolock(true);
                                    Serial.println(" - Pulse count: Infinite (accumulator reset)");
                                }
                            } else {
                                // Use duration mode (default, backward compatible)
                                engine->_state.setUsingPulseCount_nolock(false);
                                engine->_state.setRequestedPulseCount_nolock(0); // Clear pulse count when using duration
                                if (receivedCmd.durationSec > 0) {
                                    engine->_state.setRequestedDurationSec_nolock(receivedCmd.durationSec);
                                    engine->_state.setDurationStartTimeMicros_nolock(engine->_state.getStartTimeMicros_nolock()); // Use the same start time
                                    Serial.printf(" - Duration set: %.2f s\n", engine->_state.getRequestedDurationSec_nolock());
                                } else {
                                    engine->_state.setRequestedDurationSec_nolock(0); // Infinite duration
                                    engine->_state.setDurationStartTimeMicros_nolock(0);
                                    Serial.println(" - Duration: Infinite");
                                }
                            }

                            // Apply settings to pulse generator (check which parameters to use)
                            if (receivedCmd.paramMode & PARAM_USE_PERIOD) {
                                engine->pulseGen.setPeriod(receivedCmd.periodUs);
                            } else if (receivedCmd.paramMode & PARAM_USE_FREQUENCY) {
                                engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
                            }

                            if (receivedCmd.paramMode & PARAM_USE_PULSE_WIDTH) {
                                engine->pulseGen.setPulseWidth(0, receivedCmd.pulseWidthUs); // Apply to channel 0
                            } else if (receivedCmd.paramMode & PARAM_USE_DUTY_CYCLE) {
                                engine->pulseGen.setDutyCycle(0, receivedCmd.dutyCycle); // Apply to channel 0
                            }

                            // Apply polarity if specified in command
                            engine->pulseGen.setPolarity(0, receivedCmd.polarity);

                            engine->pulseGen.start(); // Start all enabled channels

                            // Update current state with what was actually applied
                            engine->_state.setCurrentFrequencyHz_nolock(engine->_state.getLastAppliedFrequencyHz_nolock());
                            engine->_state.setCurrentDutyCycle_nolock(engine->_state.getLastAppliedDutyCycle_nolock());
                            engine->_state.setRunning_nolock(true);
                            stateChanged = true;
                            eventId = SIG_EVT_STARTED; // Specific event ID for start
                            Serial.printf("CmdDispatcherTask: START applied F=%.2f Hz, D=%.2f%% - Running (Start time: %llu us)\n",
                                          engine->_state.getCurrentFrequencyHz_nolock(),
                                          engine->_state.getCurrentDutyCycle_nolock() * 100.0,
                                          engine->_state.getStartTimeMicros_nolock());
                        });
                    }
                    break;

                case SIG_CMD_STOP:
                    {
                        Serial.println("CmdDispatcherTask: Processing STOP");

                        // THREAD SAFETY: Use SignalState atomicUpdate
                        engine->_state.atomicUpdate([&]() {
                            // Let TimingController handle final accumulation
                            engine->_timingController.onStop(engine->_state);

                            // Set eventData values based on state BEFORE stopping
                            eventData.channel = 0;
                            eventData.current_freq = (uint32_t)engine->_state.getCurrentFrequencyHz_nolock();
                            eventData.current_duty = engine->_state.getCurrentDutyCycle_nolock();
                            eventData.duration_sec = engine->_state.getLastAppliedDurationSec_nolock();
                            eventData.current_ticks = engine->_state.getAccumulatedTicks_nolock(); // Use final accumulated value

                            // Now reset state (hardware stop happens outside critical section)
                            engine->_state.setRunning_nolock(false);
                            engine->_state.setStartTimeMicros_nolock(0); // Reset start time on stop
                            engine->_state.setRequestedDurationSec_nolock(0); // Cancel any duration timer
                            engine->_state.setRequestedPulseCount_nolock(0); // Cancel any pulse count limit
                            engine->_state.setDurationStartTimeMicros_nolock(0);
                            engine->_state.setUsingPulseCount_nolock(false); // Reset mode flag
                            engine->_state.resetAccumulatedTicks_nolock(); // Reset accumulator AFTER getting final value
                        });

                        // Stop hardware outside critical section
                        engine->pulseGen.stop();

                        stateChanged = true;
                        eventId = SIG_EVT_STOPPED; // Specific event ID for stop
                        Serial.println("CmdDispatcherTask: STOP applied - Stopped");
                        // Event will be posted after the switch statement using the eventData set above
                    }
                    break;

                 case SIG_CMD_UPDATE_FREQ:
                     {
                         Serial.printf("CmdDispatcherTask: Processing UPDATE_FREQ to %.2f Hz\n", receivedCmd.frequencyHz);

                         // THREAD SAFETY: Use SignalState atomicUpdate
                         engine->_state.atomicUpdate([&]() {
                             // Let TimingController handle frequency change accumulation
                             double oldFrequency = engine->_state.getCurrentFrequencyHz_nolock();
                             engine->_timingController.onFrequencyChange(engine->_state, oldFrequency);

                             engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
                             engine->_state.setCurrentFrequencyHz_nolock(receivedCmd.frequencyHz);

                             if (engine->_state.isRunning_nolock()) { // Only trigger event if running
                                 stateChanged = true;
                                 eventId = SIG_EVT_PARAMS_CHANGED;
                             }
                             Serial.printf("CmdDispatcherTask: Frequency updated. Running: %s\n",
                                           engine->_state.isRunning_nolock() ? "true" : "false");
                         });
                     }
                     break;

                 case SIG_CMD_UPDATE_DUTY:
                     {
                         Serial.printf("CmdDispatcherTask: Processing UPDATE_DUTY to %.2f%%\n", receivedCmd.dutyCycle * 100.0);

                         // THREAD SAFETY: Use SignalState atomicUpdate
                         engine->_state.atomicUpdate([&]() {
                             engine->pulseGen.setDutyCycle(0, receivedCmd.dutyCycle); // Update channel 0
                             engine->_state.setCurrentDutyCycle_nolock(receivedCmd.dutyCycle);
                             if (engine->_state.isRunning_nolock()) { // Only trigger event if running
                                 stateChanged = true;
                                 eventId = SIG_EVT_PARAMS_CHANGED;
                             }
                             Serial.printf("CmdDispatcherTask: Duty cycle updated. Running: %s\n",
                                           engine->_state.isRunning_nolock() ? "true" : "false");
                         });
                     }
                     break;

                case SIG_CMD_UPDATE_ALL:
                    { // Start new scope for this case
                        // NOTE: Duration is NOT affected by UPDATE commands
                        Serial.printf("CmdDispatcherTask: Processing UPDATE_ALL F=%.2f Hz, D=%.2f%%\n", receivedCmd.frequencyHz, receivedCmd.dutyCycle * 100.0);

                        // THREAD SAFETY: Use SignalState atomicUpdate
                        engine->_state.atomicUpdate([&]() {
                            // Accumulate ticks for the segment just ending (only if running)
                            if (engine->_state.isRunning_nolock()) {
                                uint64_t cycles_just_elapsed = engine->_timingController.calculateCycles(
                                    engine->_state.getStartTimeMicros_nolock(),
                                    esp_timer_get_time(),
                                    engine->_state.getCurrentFrequencyHz_nolock()
                                );
                                engine->_state.addAccumulatedTicks_nolock(cycles_just_elapsed);
                            }

                            // Apply new settings
                            engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
                            engine->pulseGen.setDutyCycle(0, receivedCmd.dutyCycle);
                            engine->_state.setCurrentFrequencyHz_nolock(receivedCmd.frequencyHz);
                            engine->_state.setCurrentDutyCycle_nolock(receivedCmd.dutyCycle);

                            // Reset start time for the new segment (even if params didn't change running state)
                            engine->_state.setStartTimeMicros_nolock(esp_timer_get_time());

                            bool previouslyRunning = engine->_state.isRunning_nolock();
                            bool shouldBeRunning = (engine->_state.getCurrentDutyCycle_nolock() > 0.0f);

                            if (previouslyRunning != shouldBeRunning) {
                                engine->_state.setRunning_nolock(shouldBeRunning); // Update state
                                eventId = shouldBeRunning ? SIG_EVT_STARTED : SIG_EVT_STOPPED; // START or STOP event
                                if (!shouldBeRunning) { // Transitioning to stopped
                                    engine->_state.setStartTimeMicros_nolock(0); // Clear start time as well
                                    // Keep _accumulatedTicks
                                } else { // Transitioning FROM stopped TO running
                                     engine->_state.resetAccumulatedTicks_nolock(); // Treat as a fresh start if duty was 0
                                     // _startTimeMicros was already set above
                                }
                            } else {
                                 eventId = SIG_EVT_PARAMS_CHANGED; // Just params changed
                            }
                            stateChanged = true;
                            Serial.printf("CmdDispatcherTask: Parameters updated. Running: %s\n",
                                          engine->_state.isRunning_nolock() ? "true" : "false");
                        });
                    } // End new scope for this case
                    break;

                case SIG_CMD_SET_PIN:
                    {
                        Serial.printf("CmdDispatcherTask: Handling SET_PIN command (Pin: %d)\n", receivedCmd.pin);

                        // THREAD SAFETY: Use SignalState atomicUpdate
                        engine->_state.atomicUpdate([&]() {
                            // Validate pin number using isValidOutputPin() helper
                            if (isValidOutputPin(receivedCmd.pin)) {
                                if (receivedCmd.pin != engine->_state.getOutputPin_nolock()) {
                                    Serial.printf("CmdDispatcherTask: Pin changed from %d to %d. Applying.\n",
                                                  engine->_state.getOutputPin_nolock(), receivedCmd.pin);

                                    // Update the engine's internal state
                                    engine->_state.setOutputPin_nolock(receivedCmd.pin);

                                    // Reconfigure master channel (channel 0) with the new pin
                                    engine->pulseGen.configureMaster(receivedCmd.pin);

                                    Serial.printf("CmdDispatcherTask: Master output pin successfully set to %d.\n",
                                                  engine->_state.getOutputPin_nolock());
                                } else {
                                    Serial.printf("CmdDispatcherTask: Pin %d is already the current pin. No change needed.\n",
                                                  receivedCmd.pin);
                                }
                            } else {
                                Serial.printf("CmdDispatcherTask: Error - Invalid pin %d. Valid pins: 2,4,5,12-19,21-23,25-27,32-33.\n",
                                              receivedCmd.pin);
                            }
                            // No state change event needed for pin change unless explicitly desired
                            stateChanged = false; // Prevent default PARAM_CHANGED event
                        });

                        // Save pin to NVS outside critical section
                        if (isValidOutputPin(receivedCmd.pin)) {
                            engine->_persistence.saveOutputPin(receivedCmd.pin);
                        }
                    }
                    break;

                case SIG_CMD_SET_INDICATOR:
                    Serial.printf("CmdDispatcherTask: Handling SET_INDICATOR command (Pin: %d)\n", receivedCmd.pin);
                    // Indicator pin can be 0 (disabled) or any valid GPIO
                    engine->pulseGen.setIndicatorPin(receivedCmd.pin);
                    Serial.printf("CmdDispatcherTask: Status indicator pin set to %d\n", receivedCmd.pin);
                    stateChanged = false; // No state change event needed
                    break;

                case SIG_CMD_CONFIG_CHANNEL:
                    // Note: Typically used for slave channels (1-5). Master channel (0) should use SET_PIN.
                    Serial.printf("CmdDispatcherTask: Configuring channel %d (Pin: %d, Phase: %.1f°, Enabled: %d)\n",
                                 receivedCmd.channel, receivedCmd.pin, receivedCmd.phaseOffset, receivedCmd.enabled);
                    {
                        if (receivedCmd.channel == 0) {
                            // Configure master using the simpler API
                            engine->pulseGen.configureMaster(receivedCmd.pin);
                        } else {
                            // Configure slave with phase offset
                            engine->pulseGen.configureSlave(receivedCmd.channel, receivedCmd.pin,
                                                           receivedCmd.phaseOffset, receivedCmd.enabled);
                        }
                    }
                    stateChanged = false;
                    break;

                case SIG_CMD_ENABLE_CHANNEL:
                    Serial.printf("CmdDispatcherTask: %s channel %d\n",
                                 receivedCmd.enabled ? "Enabling" : "Disabling", receivedCmd.channel);
                    engine->pulseGen.enableChannel(receivedCmd.channel, receivedCmd.enabled);
                    stateChanged = false;
                    break;

                case SIG_CMD_SYNC:
                    Serial.println("CmdDispatcherTask: Triggering sync");
                    engine->pulseGen.triggerSync();
                    stateChanged = false;
                    break;

                case SIG_CMD_SWEEP: // Placeholder
                    Serial.println("CmdDispatcherTask: SWEEP command received (Not Implemented)");
                    break;

                default:
                    Serial.printf("CmdDispatcherTask: Unknown command type %d\n", receivedCmd.type);
                    break;
            }

            // If state changed, post an event
            if (stateChanged) {
                // For START/UPDATE, eventData is set here
                // For STOP, eventData was set inside the case block
                if (eventId != SIG_EVT_STOPPED) {
                    eventData.channel = 0; // Hardcode channel 0 for now
                    eventData.current_freq = (uint32_t)engine->_state.getCurrentFrequencyHz();
                    eventData.current_duty = engine->_state.getCurrentDutyCycle();
                    eventData.duration_sec = engine->_state.getLastAppliedDurationSec(); // Populate duration for START/UPDATE
                    eventData.current_ticks = engine->getEstimatedCycleCount(); // Populate with calculated cycles
                }
                // Always include the current pin in the event data
                eventData.output_pin = engine->_state.getOutputPin();

                // Post the event to the default event loop with timeout
                const TickType_t EVENT_POST_TIMEOUT_MS = 100; // 100ms timeout
                esp_err_t post_err = esp_event_post(SIGNAL_EVENTS, eventId, &eventData, sizeof(eventData),
                                                    pdMS_TO_TICKS(EVENT_POST_TIMEOUT_MS));
                if (post_err == ESP_ERR_TIMEOUT) {
                    Serial.printf("WARNING: Event queue full, event %d dropped\n", eventId);
                } else if (post_err != ESP_OK) {
                    Serial.printf("ERROR: Failed to post event %d: %s\n", eventId, esp_err_to_name(post_err));
                } else {
                    Serial.printf("Posted event: Base=%s, ID=%d\n", "SIGNAL_EVENTS", eventId);
                }
            }
        }
    }
} 