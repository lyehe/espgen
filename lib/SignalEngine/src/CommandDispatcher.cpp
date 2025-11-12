#include "CommandDispatcher.h"
#include "IPulseGenerator.h"
#include "SignalState.h"
#include "SignalPersistence.h"
#include "TimingController.h"
#include "SignalEventPublisher.h"
#include "param_helpers.h"
#include "build_opts.h"
#include "esp_timer.h"

CommandDispatcher::CommandDispatcher(
    IPulseGenerator& pulseGen,
    SignalState& state,
    SignalPersistence& persistence,
    TimingController& timingController,
    SignalEventPublisher& eventPublisher
) :
    _pulseGen(pulseGen),
    _state(state),
    _persistence(persistence),
    _timingController(timingController),
    _eventPublisher(eventPublisher),
    _commandQueue(NULL),
    _taskHandle(NULL),
    _initialized(false)
{
}

CommandDispatcher::~CommandDispatcher() {
    // Cleanup if needed
    if (_commandQueue != NULL) {
        vQueueDelete(_commandQueue);
    }
    if (_taskHandle != NULL) {
        vTaskDelete(_taskHandle);
    }
}

bool CommandDispatcher::begin() {
    // Create command queue
    _commandQueue = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
    if (_commandQueue == NULL) {
        Serial.println("CommandDispatcher: CRITICAL - Failed to create command queue!");
        return false;
    }
    Serial.printf("CommandDispatcher: Command queue created (size: %d)\n", SIGNAL_ENGINE_CMD_QUEUE_LEN);

    // Create dispatcher task
    BaseType_t taskCreated = xTaskCreate(
        dispatcherTask,
        "CmdDispatchTask",
        2048,
        this,
        5,
        &_taskHandle
    );

    if (taskCreated != pdPASS) {
        Serial.println("CommandDispatcher: CRITICAL - Failed to create dispatcher task!");
        vQueueDelete(_commandQueue);
        _commandQueue = NULL;
        return false;
    }

    Serial.println("CommandDispatcher: Dispatcher task started.");
    _initialized = true;
    return true;
}

bool CommandDispatcher::sendCommand(const SignalCmd& cmd) {
    if (!_initialized || _commandQueue == NULL) {
        Serial.println("CommandDispatcher::sendCommand - ERROR: Not initialized!");
        return false;
    }

    if (xQueueSend(_commandQueue, &cmd, (TickType_t)0) != pdPASS) {
        Serial.println("CommandDispatcher::sendCommand - WARNING: Command queue full.");
        return false;
    }
    return true;
}

bool CommandDispatcher::isInitialized() const {
    return _initialized;
}

void CommandDispatcher::dispatcherTask(void* pvParameters) {
    CommandDispatcher* dispatcher = static_cast<CommandDispatcher*>(pvParameters);
    SignalCmd receivedCmd;

    Serial.println("CommandDispatcher: Task loop starting.");

    for (;;) {
        if (xQueueReceive(dispatcher->_commandQueue, &receivedCmd, portMAX_DELAY) == pdPASS) {
            Serial.printf("CommandDispatcher: Received command type %d\n", receivedCmd.type);

            // Validate command
            if (!validateSignalCmd(&receivedCmd)) {
                Serial.println("CommandDispatcher: ERROR - Invalid command parameters, ignoring");
                continue;
            }

            bool stateChanged = false;
            SigEvtId eventId = SIG_EVT_PARAMS_CHANGED;
            uint64_t finalTicks = 0;

            // Dispatch to appropriate handler
            switch (receivedCmd.type) {
                case SIG_CMD_START:
                    dispatcher->handleStart(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_STOP:
                    dispatcher->handleStop(receivedCmd, stateChanged, eventId, finalTicks);
                    break;
                case SIG_CMD_UPDATE_FREQ:
                    dispatcher->handleUpdateFreq(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_UPDATE_DUTY:
                    dispatcher->handleUpdateDuty(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_UPDATE_ALL:
                    dispatcher->handleUpdateAll(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_SET_PIN:
                    dispatcher->handleSetPin(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_SET_INDICATOR:
                    dispatcher->handleSetIndicator(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_CONFIG_CHANNEL:
                    dispatcher->handleConfigChannel(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_ENABLE_CHANNEL:
                    dispatcher->handleEnableChannel(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_SYNC:
                    dispatcher->handleSync(receivedCmd, stateChanged, eventId);
                    break;
                case SIG_CMD_SWEEP:
                    dispatcher->handleSweep(receivedCmd, stateChanged, eventId);
                    break;
                default:
                    Serial.printf("CommandDispatcher: Unknown command type %d\n", receivedCmd.type);
                    break;
            }

            // Post event if state changed
            if (stateChanged) {
                bool eventPosted = false;
                switch (eventId) {
                    case SIG_EVT_STARTED:
                        eventPosted = dispatcher->_eventPublisher.publishStarted(dispatcher->_state);
                        break;
                    case SIG_EVT_STOPPED:
                        eventPosted = dispatcher->_eventPublisher.publishStopped(dispatcher->_state, finalTicks);
                        break;
                    case SIG_EVT_PARAMS_CHANGED:
                        eventPosted = dispatcher->_eventPublisher.publishParamsChanged(dispatcher->_state);
                        break;
                    default:
                        Serial.printf("WARNING: Unknown event ID %d\n", eventId);
                        break;
                }
                if (!eventPosted) {
                    Serial.printf("WARNING: Failed to post event %d\n", eventId);
                }
            }
        }
    }
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void CommandDispatcher::handleStart(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.println("CmdDispatcherTask: Processing START");

    // THREAD SAFETY: Use SignalState atomicUpdate
    _state.atomicUpdate([&]() {
        // Update frequency/period (check which mode is being used)
        if (cmd.paramMode & PARAM_USE_FREQUENCY) {
            _state.setLastAppliedFrequencyHz_nolock(cmd.frequencyHz);
        } else if (cmd.paramMode & PARAM_USE_PERIOD) {
            // Convert period to frequency for internal storage
            _state.setLastAppliedFrequencyHz_nolock(periodToFreqHz(cmd.periodUs));
        } else {
            // Default to current frequency if not specified
            _state.setLastAppliedFrequencyHz_nolock(_state.getCurrentFrequencyHz_nolock());
        }

        // Update duty cycle/pulse width (check which mode is being used)
        if (cmd.paramMode & PARAM_USE_DUTY_CYCLE) {
            _state.setLastAppliedDutyCycle_nolock(cmd.dutyCycle);
        } else if (cmd.paramMode & PARAM_USE_PULSE_WIDTH) {
            // Convert pulse width to duty cycle for internal storage
            uint32_t currentPeriodUs = _pulseGen.getPeriodUs();
            if (currentPeriodUs > 0) {
                _state.setLastAppliedDutyCycle_nolock(pulseWidthToDuty(cmd.pulseWidthUs, currentPeriodUs));
            }
        } else {
            // Default to current duty if not specified
            _state.setLastAppliedDutyCycle_nolock(_state.getCurrentDutyCycle_nolock());
        }

        // Update duration/pulse count (check which mode is being used)
        if (cmd.paramMode & PARAM_USE_DURATION) {
            _state.setLastAppliedDurationSec_nolock(cmd.durationSec);
            _state.setLastAppliedPulseCount_nolock(0); // Clear pulse count when using duration
        } else if (cmd.paramMode & PARAM_USE_PULSE_COUNT) {
            _state.setLastAppliedPulseCount_nolock(cmd.pulseCount);
            _state.setLastAppliedDurationSec_nolock(0.0f); // Clear duration when using pulse count
        } else {
            // Default to no duration/count if not specified
            _state.setLastAppliedDurationSec_nolock(0.0f);
            _state.setLastAppliedPulseCount_nolock(0);
        }

        Serial.printf(" - Stored Last Applied: F=%.2f Hz, D=%.2f%%",
                      _state.getLastAppliedFrequencyHz_nolock(),
                      _state.getLastAppliedDutyCycle_nolock() * 100.0);

        // Determine if using pulse count or duration
        if (cmd.paramMode & PARAM_USE_PULSE_COUNT) {
            Serial.printf(", Pulses=%llu\n", _state.getLastAppliedPulseCount_nolock());
        } else {
            Serial.printf(", Dur=%.2f s\n", _state.getLastAppliedDurationSec_nolock());
        }

        // Note: NVS saving happens outside atomicUpdate to avoid blocking mutex
    });

    // --- Save Settings to NVS using SignalPersistence (outside critical section) ---
    _persistence.saveSignalParams(
        _state.getLastAppliedFrequencyHz(),
        _state.getLastAppliedDutyCycle(),
        _state.getLastAppliedDurationSec()
    );
    // --- End Save Settings ---

    // Re-enter atomicUpdate for the rest of START command
    _state.atomicUpdate([&]() {
        // Continue with START command logic...

        if (!_state.isRunning_nolock()) { // Record start time only if starting from stopped state
             _state.resetAccumulatedTicks_nolock(); // Reset accumulator on new start
             _state.setStartTimeMicros_nolock(esp_timer_get_time());
        } else {
            // If already running, treat START like an UPDATE_ALL to apply new params
            // Accumulate ticks from the current segment first
            uint64_t cycles_just_elapsed = _timingController.calculateCycles(
                _state.getStartTimeMicros_nolock(),
                esp_timer_get_time(),
                _state.getCurrentFrequencyHz_nolock()
            );
            _state.addAccumulatedTicks_nolock(cycles_just_elapsed);
            // New start time for the next segment
            _state.setStartTimeMicros_nolock(esp_timer_get_time());
        }

        // Handle Duration or Pulse Count for START
        if (cmd.paramMode & PARAM_USE_PULSE_COUNT) {
            // Use pulse count mode - RESET accumulator for new pulse count
            // User expects START to begin counting from 0, not continue from accumulated
            _state.resetAccumulatedTicks_nolock();
            _state.setStartTimeMicros_nolock(esp_timer_get_time());

            if (cmd.pulseCount > 0) {
                _state.setRequestedPulseCount_nolock(cmd.pulseCount);
                _state.setUsingPulseCount_nolock(true);
                _state.setRequestedDurationSec_nolock(0); // Clear duration when using pulse count
                _state.setDurationStartTimeMicros_nolock(0);
                Serial.printf(" - Pulse count set: %llu pulses (accumulator reset)\n",
                              _state.getRequestedPulseCount_nolock());
            } else {
                _state.setRequestedPulseCount_nolock(0); // Infinite pulses
                _state.setUsingPulseCount_nolock(true);
                Serial.println(" - Pulse count: Infinite (accumulator reset)");
            }
        } else {
            // Use duration mode (default, backward compatible)
            _state.setUsingPulseCount_nolock(false);
            _state.setRequestedPulseCount_nolock(0); // Clear pulse count when using duration
            if (cmd.durationSec > 0) {
                _state.setRequestedDurationSec_nolock(cmd.durationSec);
                _state.setDurationStartTimeMicros_nolock(_state.getStartTimeMicros_nolock()); // Use the same start time
                Serial.printf(" - Duration set: %.2f s\n", _state.getRequestedDurationSec_nolock());
            } else {
                _state.setRequestedDurationSec_nolock(0); // Infinite duration
                _state.setDurationStartTimeMicros_nolock(0);
                Serial.println(" - Duration: Infinite");
            }
        }

        // Apply settings to pulse generator (check which parameters to use)
        if (cmd.paramMode & PARAM_USE_PERIOD) {
            _pulseGen.setPeriod(cmd.periodUs);
        } else if (cmd.paramMode & PARAM_USE_FREQUENCY) {
            _pulseGen.setFrequency(cmd.frequencyHz);
        }

        if (cmd.paramMode & PARAM_USE_PULSE_WIDTH) {
            _pulseGen.setPulseWidth(0, cmd.pulseWidthUs); // Apply to channel 0
        } else if (cmd.paramMode & PARAM_USE_DUTY_CYCLE) {
            _pulseGen.setDutyCycle(0, cmd.dutyCycle); // Apply to channel 0
        }

        // Apply polarity if specified in command
        _pulseGen.setPolarity(0, cmd.polarity);

        _pulseGen.start(); // Start all enabled channels

        // Update current state with what was actually applied
        _state.setCurrentFrequencyHz_nolock(_state.getLastAppliedFrequencyHz_nolock());
        _state.setCurrentDutyCycle_nolock(_state.getLastAppliedDutyCycle_nolock());
        _state.setRunning_nolock(true);
        stateChanged = true;
        eventId = SIG_EVT_STARTED; // Specific event ID for start
        Serial.printf("CmdDispatcherTask: START applied F=%.2f Hz, D=%.2f%% - Running (Start time: %llu us)\n",
                      _state.getCurrentFrequencyHz_nolock(),
                      _state.getCurrentDutyCycle_nolock() * 100.0,
                      _state.getStartTimeMicros_nolock());
    });
}

void CommandDispatcher::handleStop(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId, uint64_t& finalTicks) {
    Serial.println("CmdDispatcherTask: Processing STOP");

    SignalEvtData eventData; // Structure to hold event data

    // THREAD SAFETY: Use SignalState atomicUpdate
    _state.atomicUpdate([&]() {
        // Let TimingController handle final accumulation
        _timingController.onStop(_state);

        // Set eventData values based on state BEFORE stopping
        eventData.channel = 0;
        eventData.current_freq = (uint32_t)_state.getCurrentFrequencyHz_nolock();
        eventData.current_duty = _state.getCurrentDutyCycle_nolock();
        eventData.duration_sec = _state.getLastAppliedDurationSec_nolock();
        eventData.current_ticks = _state.getAccumulatedTicks_nolock(); // Use final accumulated value

        // Now reset state (hardware stop happens outside critical section)
        _state.setRunning_nolock(false);
        _state.setStartTimeMicros_nolock(0); // Reset start time on stop
        _state.setRequestedDurationSec_nolock(0); // Cancel any duration timer
        _state.setRequestedPulseCount_nolock(0); // Cancel any pulse count limit
        _state.setDurationStartTimeMicros_nolock(0);
        _state.setUsingPulseCount_nolock(false); // Reset mode flag
        _state.resetAccumulatedTicks_nolock(); // Reset accumulator AFTER getting final value
    });

    // Save final ticks before they're reset (for event publishing)
    finalTicks = eventData.current_ticks;

    // Stop hardware outside critical section
    _pulseGen.stop();

    stateChanged = true;
    eventId = SIG_EVT_STOPPED; // Specific event ID for stop
    Serial.println("CmdDispatcherTask: STOP applied - Stopped");
    // Event will be posted after the switch statement using the eventData set above
}

void CommandDispatcher::handleUpdateFreq(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.printf("CmdDispatcherTask: Processing UPDATE_FREQ to %.2f Hz\n", cmd.frequencyHz);

    // THREAD SAFETY: Use SignalState atomicUpdate
    _state.atomicUpdate([&]() {
        // Let TimingController handle frequency change accumulation
        double oldFrequency = _state.getCurrentFrequencyHz_nolock();
        _timingController.onFrequencyChange(_state, oldFrequency);

        _pulseGen.setFrequency(cmd.frequencyHz);
        _state.setCurrentFrequencyHz_nolock(cmd.frequencyHz);

        if (_state.isRunning_nolock()) { // Only trigger event if running
            stateChanged = true;
            eventId = SIG_EVT_PARAMS_CHANGED;
        }
        Serial.printf("CmdDispatcherTask: Frequency updated. Running: %s\n",
                      _state.isRunning_nolock() ? "true" : "false");
    });
}

void CommandDispatcher::handleUpdateDuty(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.printf("CmdDispatcherTask: Processing UPDATE_DUTY to %.2f%%\n", cmd.dutyCycle * 100.0);

    // THREAD SAFETY: Use SignalState atomicUpdate
    _state.atomicUpdate([&]() {
        _pulseGen.setDutyCycle(0, cmd.dutyCycle); // Update channel 0
        _state.setCurrentDutyCycle_nolock(cmd.dutyCycle);
        if (_state.isRunning_nolock()) { // Only trigger event if running
            stateChanged = true;
            eventId = SIG_EVT_PARAMS_CHANGED;
        }
        Serial.printf("CmdDispatcherTask: Duty cycle updated. Running: %s\n",
                      _state.isRunning_nolock() ? "true" : "false");
    });
}

void CommandDispatcher::handleUpdateAll(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    // NOTE: Duration is NOT affected by UPDATE commands
    Serial.printf("CmdDispatcherTask: Processing UPDATE_ALL F=%.2f Hz, D=%.2f%%\n", cmd.frequencyHz, cmd.dutyCycle * 100.0);

    // THREAD SAFETY: Use SignalState atomicUpdate
    _state.atomicUpdate([&]() {
        // Accumulate ticks for the segment just ending (only if running)
        if (_state.isRunning_nolock()) {
            uint64_t cycles_just_elapsed = _timingController.calculateCycles(
                _state.getStartTimeMicros_nolock(),
                esp_timer_get_time(),
                _state.getCurrentFrequencyHz_nolock()
            );
            _state.addAccumulatedTicks_nolock(cycles_just_elapsed);
        }

        // Apply new settings
        _pulseGen.setFrequency(cmd.frequencyHz);
        _pulseGen.setDutyCycle(0, cmd.dutyCycle);
        _state.setCurrentFrequencyHz_nolock(cmd.frequencyHz);
        _state.setCurrentDutyCycle_nolock(cmd.dutyCycle);

        // Reset start time for the new segment (even if params didn't change running state)
        _state.setStartTimeMicros_nolock(esp_timer_get_time());

        bool previouslyRunning = _state.isRunning_nolock();
        bool shouldBeRunning = (_state.getCurrentDutyCycle_nolock() > 0.0f);

        if (previouslyRunning != shouldBeRunning) {
            _state.setRunning_nolock(shouldBeRunning); // Update state
            eventId = shouldBeRunning ? SIG_EVT_STARTED : SIG_EVT_STOPPED; // START or STOP event
            if (!shouldBeRunning) { // Transitioning to stopped
                _state.setStartTimeMicros_nolock(0); // Clear start time as well
                // Keep _accumulatedTicks
            } else { // Transitioning FROM stopped TO running
                 _state.resetAccumulatedTicks_nolock(); // Treat as a fresh start if duty was 0
                 // _startTimeMicros was already set above
            }
        } else {
             eventId = SIG_EVT_PARAMS_CHANGED; // Just params changed
        }
        stateChanged = true;
        Serial.printf("CmdDispatcherTask: Parameters updated. Running: %s\n",
                      _state.isRunning_nolock() ? "true" : "false");
    });
}

void CommandDispatcher::handleSetPin(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.printf("CmdDispatcherTask: Handling SET_PIN command (Pin: %d)\n", cmd.pin);

    // THREAD SAFETY: Use SignalState atomicUpdate
    _state.atomicUpdate([&]() {
        // Validate pin number using isValidOutputPin() helper
        if (isValidOutputPin(cmd.pin)) {
            if (cmd.pin != _state.getOutputPin_nolock()) {
                Serial.printf("CmdDispatcherTask: Pin changed from %d to %d. Applying.\n",
                              _state.getOutputPin_nolock(), cmd.pin);

                // Update the engine's internal state
                _state.setOutputPin_nolock(cmd.pin);

                // Reconfigure master channel (channel 0) with the new pin
                _pulseGen.configureMaster(cmd.pin);

                Serial.printf("CmdDispatcherTask: Master output pin successfully set to %d.\n",
                              _state.getOutputPin_nolock());
            } else {
                Serial.printf("CmdDispatcherTask: Pin %d is already the current pin. No change needed.\n",
                              cmd.pin);
            }
        } else {
            Serial.printf("CmdDispatcherTask: Error - Invalid pin %d. Valid pins: 2,4,5,12-19,21-23,25-27,32-33.\n",
                          cmd.pin);
        }
        // No state change event needed for pin change unless explicitly desired
        stateChanged = false; // Prevent default PARAM_CHANGED event
    });

    // Save pin to NVS outside critical section
    if (isValidOutputPin(cmd.pin)) {
        _persistence.saveOutputPin(cmd.pin);
    }
}

void CommandDispatcher::handleSetIndicator(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.printf("CmdDispatcherTask: Handling SET_INDICATOR command (Pin: %d)\n", cmd.pin);
    // Indicator pin can be 0 (disabled) or any valid GPIO
    _pulseGen.setIndicatorPin(cmd.pin);
    Serial.printf("CmdDispatcherTask: Status indicator pin set to %d\n", cmd.pin);
    stateChanged = false; // No state change event needed
}

void CommandDispatcher::handleConfigChannel(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    // Note: Typically used for slave channels (1-5). Master channel (0) should use SET_PIN.
    Serial.printf("CmdDispatcherTask: Configuring channel %d (Pin: %d, Phase: %.1f°, Enabled: %d)\n",
                 cmd.channel, cmd.pin, cmd.phaseOffset, cmd.enabled);

    if (cmd.channel == 0) {
        // Configure master using the simpler API
        _pulseGen.configureMaster(cmd.pin);
    } else {
        // Configure slave with phase offset
        _pulseGen.configureSlave(cmd.channel, cmd.pin,
                                cmd.phaseOffset, cmd.enabled);
    }

    stateChanged = false;
}

void CommandDispatcher::handleEnableChannel(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.printf("CmdDispatcherTask: %s channel %d\n",
                 cmd.enabled ? "Enabling" : "Disabling", cmd.channel);
    _pulseGen.enableChannel(cmd.channel, cmd.enabled);
    stateChanged = false;
}

void CommandDispatcher::handleSync(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.println("CmdDispatcherTask: Triggering sync");
    _pulseGen.triggerSync();
    stateChanged = false;
}

void CommandDispatcher::handleSweep(const SignalCmd& cmd, bool& stateChanged, SigEvtId& eventId) {
    Serial.println("CmdDispatcherTask: SWEEP command received (Not Implemented)");
}
