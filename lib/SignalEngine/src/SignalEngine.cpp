#include "SignalEngine.h"
#include <Arduino.h>
#include "build_opts.h"
#include "signal_iface.h"
#include "esp_event.h"
#include "esp_timer.h"
#include <Preferences.h>
#include "preferences_keys.h"

// Define the event base we declared in signal_iface.h
ESP_EVENT_DEFINE_BASE(SIGNAL_EVENTS);

// NVS Keys for storing settings
const char* NVS_NAMESPACE = "SignalEngine";
const char* NVS_KEY_FREQ = "lastFreq";
const char* NVS_KEY_DUTY = "lastDuty";
const char* NVS_KEY_DUR = "lastDur";

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
    _currentFrequencyHz(DEFAULT_FREQUENCY_HZ),
    _currentDutyCycle(DEFAULT_DUTY_CYCLE),
    _isRunning(false),
    _lastAppliedFrequencyHz(DEFAULT_FREQUENCY_HZ),
    _lastAppliedDutyCycle(DEFAULT_DUTY_CYCLE),
    _lastAppliedDurationSec(0.0f),
    _startTimeMicros(0),
    _accumulatedTicks(0),
    _requestedDurationSec(0),
    _durationStartTimeMicros(0),
    _outputPin(DEFAULT_OUTPUT_PIN)
{
    // Constructor body (if needed)
}                           

void SignalEngine::begin() {
    Serial.println("SignalEngine: Initializing...");
    _startTimeMicros = 0; // Ensure start time is 0 initially
    _accumulatedTicks = 0; // Ensure accumulated ticks are 0 initially
    _requestedDurationSec = 0; // Ensure duration is 0 initially
    _durationStartTimeMicros = 0;

    // --- Load Settings from NVS --- 
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE, false); // Start SignalEngine namespace first

    // Load values, using compile-time defaults if not found
    double loadedFreq = preferences.getDouble(NVS_KEY_FREQ, DEFAULT_FREQUENCY_HZ);
    float loadedDuty = preferences.getFloat(NVS_KEY_DUTY, DEFAULT_DUTY_CYCLE);
    float loadedDur = preferences.getFloat(NVS_KEY_DUR, 0.0f); // Default duration is 0 (infinite)
    preferences.end(); // Close SignalEngine namespace

    // --- Load device config (including pin) --- 
    preferences.begin(DEVICE_CFG_NAMESPACE, false); // Open device config namespace
    _outputPin = preferences.getUChar(OUTPUT_PIN_KEY, DEFAULT_OUTPUT_PIN);
    preferences.end(); // Close device config namespace

    // Validate loaded pin (just in case NVS was corrupted or value is invalid)
    if (_outputPin < 12 || _outputPin > 19) {
        Serial.printf("SignalEngine: Warning - Invalid pin %d loaded from NVS. Using default %d.\n", _outputPin, DEFAULT_OUTPUT_PIN);
        _outputPin = DEFAULT_OUTPUT_PIN;
        // Optionally, save the default back to NVS here?
    }
    Serial.printf(" - Loaded Output Pin: %d (Namespace: %s, Key: %s)\n", _outputPin, DEVICE_CFG_NAMESPACE, OUTPUT_PIN_KEY);

    Serial.printf(" - Loaded Freq/Duty: F=%.2f Hz, D=%.2f%%, Dur=%.2f s\n", 
                  loadedFreq, loadedDuty * 100.0, loadedDur);

    // Apply loaded settings to internal state
    _currentFrequencyHz = loadedFreq;
    _currentDutyCycle = loadedDuty;
    _lastAppliedFrequencyHz = loadedFreq;
    _lastAppliedDutyCycle = loadedDuty;
    _lastAppliedDurationSec = loadedDur;
    // _isRunning remains false initially
    // _requestedDurationSec will be set by START command
    // --- End Load Settings --- 

    // Create the command queue
    xQueueCmd = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
    if (xQueueCmd == NULL) {
        Serial.println("SignalEngine: Error creating command queue!");
        // Handle error appropriately - maybe halt or signal critical failure
        return;
    }
    Serial.printf("SignalEngine: Command queue created (size: %d)\n", SIGNAL_ENGINE_CMD_QUEUE_LEN);

    // Initialize the PulseGenerator
    if (!pulseGen.begin()) {
        Serial.println("SignalEngine: Error - Failed to initialize PulseGenerator!");
        return;
    }

    // Configure master channel (Channel 0) with loaded settings
    pulseGen.configureMaster((uint8_t)_outputPin);

    // Set frequency and duty cycle
    pulseGen.setFrequency(_currentFrequencyHz);
    pulseGen.setDutyCycle(0, _currentDutyCycle);

    _isRunning = false; // Explicitly set initial state (not started yet)

    Serial.printf("SignalEngine: Initialized (Stopped). Master channel on Pin %d, F=%.2f Hz, D=%.2f%%\n",
                  _outputPin, _currentFrequencyHz, _currentDutyCycle * 100.0);

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
        Serial.println("SignalEngine: Error creating command dispatcher task!");
        // Handle error
        return;
    }
    Serial.println("SignalEngine: Command dispatcher task started.");

    // Create the heartbeat task (as per Phase 1 requirements)
    // Stack size might need adjustment later. Priority 1 is low.
    xTaskCreate(
        heartbeat_task,         // Task function
        "HeartbeatTask",        // Name of the task
        1024,                   // Stack size in words
        NULL,                   // Task input parameter
        1,                      // Priority of the task
        NULL                    // Task handle
    );
    Serial.println("SignalEngine: Heartbeat task started.");

    Serial.println("SignalEngine: Initialization complete.");
}

void SignalEngine::loop() {
    // This loop is called from the main application loop.
    // Check for timed stop if duration is set
    if (_isRunning && _requestedDurationSec > 0 && _durationStartTimeMicros > 0) {
        uint64_t nowMicros = esp_timer_get_time();
        uint64_t targetDurationMicros = (uint64_t)(_requestedDurationSec * 1000000.0);
        uint64_t elapsedMicros = nowMicros - _durationStartTimeMicros;

        if (elapsedMicros >= targetDurationMicros) {
            Serial.printf("SignalEngine: Duration (%.2f s) expired. Auto-stopping.\n", _requestedDurationSec);
            // Reset duration tracking BEFORE sending stop command to prevent race condition
            _requestedDurationSec = 0;
            _durationStartTimeMicros = 0;
            // Send stop command to self
            SignalCmd stopCmd = { .type = SIG_CMD_STOP };
            sendCommand(stopCmd); // Use the public sendCommand method
        }
    }
    // Other non-blocking checks can go here later.
}

// Removed SignalEngine::sendCommand implementation

// Removed SignalEngine::cmdDispatcherTask implementation 

// --- Public Methods ---

bool SignalEngine::sendCommand(const SignalCmd& cmd) {
    if (xQueueCmd == NULL) {
        Serial.println("SignalEngine::sendCommand - Error: Queue not initialized.");
        return false;
    }

    // Send the command to the queue. Wait 0 ticks if the queue is full.
    if (xQueueSend(xQueueCmd, &cmd, (TickType_t)0) != pdPASS) {
        Serial.println("SignalEngine::sendCommand - Warning: Command queue full.");
        return false; // Indicate failure
    }
    return true; // Indicate success
}

// --- Status Getters Implementation ---
double SignalEngine::getCurrentFrequencyHz() const {
    // TODO: Add locking (mutex) if this is accessed from multiple tasks
    // For now, assuming it's safe enough or accessed infrequently
    return _currentFrequencyHz;
}

float SignalEngine::getCurrentDutyCycle() const {
    // TODO: Add locking (mutex)
    return _currentDutyCycle;
}

bool SignalEngine::isRunning() const {
    // TODO: Add locking (mutex)
    return _isRunning;
}

SignalError SignalEngine::getCurrentStatus(SignalStatus_t& status) {
    // TODO: Add locking (mutex) if state variables can be modified concurrently
    // For now, assumes single-threaded access or that reads are atomic enough
    status.channel = DEFAULT_LEDC_CHANNEL; // Hardcoded for now, assuming channel 0
    status.frequency = _currentFrequencyHz; 
    status.dutyCycle = _currentDutyCycle;
    status.isRunning = _isRunning;
    status.lastAppliedDurationSec = _lastAppliedDurationSec; // Populate the new field
    
    return SIG_OK; // Use the defined success code
}

// Helper function to calculate cycles based on time and frequency
static uint64_t calculateCycles(uint64_t startMicros, uint64_t endMicros, double frequencyHz) {
    if (endMicros > startMicros && frequencyHz > 0) {
        uint64_t elapsedTimeMicros = endMicros - startMicros;
        double elapsedSeconds = elapsedTimeMicros / 1000000.0;
        double cycles = elapsedSeconds * frequencyHz;
        return (uint64_t)cycles;
    }
    return 0;
}

uint64_t SignalEngine::getEstimatedCycleCount() const {
    // TODO: Add locking (mutex) if tasks access this concurrently
    if (_isRunning && _startTimeMicros > 0) {
        uint64_t currentCycles = calculateCycles(_startTimeMicros, esp_timer_get_time(), _currentFrequencyHz);
        return _accumulatedTicks + currentCycles;
    } else if (!_isRunning) {
        // If stopped, return the last accumulated count before stop (or 0 if never run)
        return _accumulatedTicks; 
    } else {
        // Running but start time is 0? Should not happen, return accumulator only.
         return _accumulatedTicks;
    }
}

// --- Last Applied Parameter Getters Implementation ---
double SignalEngine::getLastAppliedFrequencyHz() const {
    // TODO: Add locking (mutex) if needed
    return _lastAppliedFrequencyHz;
}

float SignalEngine::getLastAppliedDutyCycle() const {
    // TODO: Add locking (mutex) if needed
    return _lastAppliedDutyCycle;
}

float SignalEngine::getLastAppliedDurationSec() const {
    // TODO: Add locking (mutex) if needed
    return _lastAppliedDurationSec;
}

// --- Getter for Output Pin ---
int SignalEngine::getOutputPin() const {
    // TODO: Add locking (mutex) if needed and accessed concurrently
    return _outputPin;
}

// --- Private Static Task Functions ---

void SignalEngine::cmdDispatcherTask(void *pvParameters) {
    SignalEngine* engine = static_cast<SignalEngine*>(pvParameters);
    SignalCmd receivedCmd;
    SignalEvtData eventData; // Structure to hold event data
    Preferences preferences; // Declare Preferences object outside loop for efficiency

    Serial.println("CmdDispatcherTask: Starting loop.");

    for (;;) {
        if (xQueueReceive(engine->xQueueCmd, &receivedCmd, portMAX_DELAY) == pdPASS) {
            Serial.printf("CmdDispatcherTask: Received command type %d\n", receivedCmd.type);
            bool stateChanged = false;
            SigEvtId eventId = SIG_EVT_PARAMS_CHANGED; // Default event ID

            switch (receivedCmd.type) {
                case SIG_CMD_START:
                    Serial.println("CmdDispatcherTask: Processing START");
                    // Update Last Applied Params in memory
                    engine->_lastAppliedFrequencyHz = receivedCmd.frequencyHz;
                    engine->_lastAppliedDutyCycle = receivedCmd.dutyCycle;
                    engine->_lastAppliedDurationSec = receivedCmd.durationSec;
                    Serial.printf(" - Stored Last Applied: F=%.2f Hz, D=%.2f%%, Dur=%.2f s\n", 
                                  engine->_lastAppliedFrequencyHz, 
                                  engine->_lastAppliedDutyCycle * 100.0, 
                                  engine->_lastAppliedDurationSec);

                    // --- Save Settings to NVS --- 
                    preferences.begin(NVS_NAMESPACE, false);
                    preferences.putDouble(NVS_KEY_FREQ, engine->_lastAppliedFrequencyHz);
                    preferences.putFloat(NVS_KEY_DUTY, engine->_lastAppliedDutyCycle);
                    preferences.putFloat(NVS_KEY_DUR, engine->_lastAppliedDurationSec);
                    preferences.end();
                    Serial.println(" - Saved settings to NVS.");
                    // --- End Save Settings --- 

                    if (!engine->_isRunning) { // Record start time only if starting from stopped state
                         engine->_accumulatedTicks = 0; // Reset accumulator on new start
                         engine->_startTimeMicros = esp_timer_get_time();
                    } else {
                        // If already running, treat START like an UPDATE_ALL to apply new params
                        // Accumulate ticks from the current segment first
                        uint64_t cycles_just_elapsed = calculateCycles(engine->_startTimeMicros, esp_timer_get_time(), engine->_currentFrequencyHz);
                        engine->_accumulatedTicks += cycles_just_elapsed;
                        // New start time for the next segment
                        engine->_startTimeMicros = esp_timer_get_time();
                    }
                    
                    // Handle Duration for START
                    if (receivedCmd.durationSec > 0) {
                        engine->_requestedDurationSec = receivedCmd.durationSec;
                        engine->_durationStartTimeMicros = engine->_startTimeMicros; // Use the same start time
                         Serial.printf(" - Duration set: %.2f s\n", engine->_requestedDurationSec);
                    } else {
                        engine->_requestedDurationSec = 0; // Infinite duration
                        engine->_durationStartTimeMicros = 0;
                         Serial.println(" - Duration: Infinite");
                    }

                    // Apply settings to pulse generator
                    engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
                    engine->pulseGen.setDutyCycle(0, receivedCmd.dutyCycle); // Apply to channel 0
                    engine->pulseGen.start(); // Start all enabled channels

                    engine->_currentFrequencyHz = receivedCmd.frequencyHz;
                    engine->_currentDutyCycle = receivedCmd.dutyCycle;
                    engine->_isRunning = true;
                    stateChanged = true;
                    eventId = SIG_EVT_STARTED; // Specific event ID for start
                    Serial.printf("CmdDispatcherTask: START applied F=%.2f Hz, D=%.2f%% - Running (Start time: %llu us)\n", engine->_currentFrequencyHz, engine->_currentDutyCycle * 100.0, engine->_startTimeMicros);
                    break;

                case SIG_CMD_STOP:
                    Serial.println("CmdDispatcherTask: Processing STOP");
                    // Calculate final ticks BEFORE resetting state
                    if (engine->_isRunning) { 
                        uint64_t cycles_just_elapsed = calculateCycles(engine->_startTimeMicros, esp_timer_get_time(), engine->_currentFrequencyHz);
                        engine->_accumulatedTicks += cycles_just_elapsed;
                    }
                    // Set eventData values based on state BEFORE stopping
                    eventData.channel = 0;
                    eventData.current_freq = (uint32_t)engine->_currentFrequencyHz;
                    eventData.current_duty = engine->_currentDutyCycle;
                    eventData.duration_sec = engine->_lastAppliedDurationSec; 
                    eventData.current_ticks = engine->_accumulatedTicks; // Use final accumulated value
                    
                    // Now stop the hardware and reset state
                    engine->pulseGen.stop();
                    engine->_isRunning = false;
                    engine->_startTimeMicros = 0; // Reset start time on stop
                    engine->_requestedDurationSec = 0; // Cancel any duration timer
                    engine->_durationStartTimeMicros = 0;
                    engine->_accumulatedTicks = 0; // Reset accumulator AFTER getting final value
                    
                    stateChanged = true;
                    eventId = SIG_EVT_STOPPED; // Specific event ID for stop
                    Serial.println("CmdDispatcherTask: STOP applied - Stopped");
                    // Event will be posted after the switch statement using the eventData set above
                    break;

                 case SIG_CMD_UPDATE_FREQ:
                     Serial.printf("CmdDispatcherTask: Processing UPDATE_FREQ to %.2f Hz\n", receivedCmd.frequencyHz);
                     engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
                     engine->_currentFrequencyHz = receivedCmd.frequencyHz;
                     if (engine->_isRunning) { // Only trigger event if running
                         stateChanged = true;
                         eventId = SIG_EVT_PARAMS_CHANGED;
                     }
                     Serial.printf("CmdDispatcherTask: Frequency updated. Running: %s\n", engine->_isRunning ? "true" : "false");
                     break;

                 case SIG_CMD_UPDATE_DUTY:
                     Serial.printf("CmdDispatcherTask: Processing UPDATE_DUTY to %.2f%%\n", receivedCmd.dutyCycle * 100.0);
                     engine->pulseGen.setDutyCycle(0, receivedCmd.dutyCycle); // Update channel 0
                     engine->_currentDutyCycle = receivedCmd.dutyCycle;
                     if (engine->_isRunning) { // Only trigger event if running
                         stateChanged = true;
                         eventId = SIG_EVT_PARAMS_CHANGED;
                     }
                     Serial.printf("CmdDispatcherTask: Duty cycle updated. Running: %s\n", engine->_isRunning ? "true" : "false");
                     break;

                case SIG_CMD_UPDATE_ALL:
                    { // Start new scope for this case
                        // NOTE: Duration is NOT affected by UPDATE commands
                        Serial.printf("CmdDispatcherTask: Processing UPDATE_ALL F=%.2f Hz, D=%.2f%%\n", receivedCmd.frequencyHz, receivedCmd.dutyCycle * 100.0);
                        
                        // Accumulate ticks for the segment just ending (only if running)
                        if (engine->_isRunning) {
                            uint64_t cycles_just_elapsed = calculateCycles(engine->_startTimeMicros, esp_timer_get_time(), engine->_currentFrequencyHz);
                            engine->_accumulatedTicks += cycles_just_elapsed;
                        }
                        
                        // Apply new settings
                        engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
                        engine->pulseGen.setDutyCycle(0, receivedCmd.dutyCycle);
                        engine->_currentFrequencyHz = receivedCmd.frequencyHz;
                        engine->_currentDutyCycle = receivedCmd.dutyCycle;
                        
                        // Reset start time for the new segment (even if params didn't change running state)
                        engine->_startTimeMicros = esp_timer_get_time(); 

                        bool previouslyRunning = engine->_isRunning;
                        bool shouldBeRunning = (engine->_currentDutyCycle > 0.0f);
                        
                        if (previouslyRunning != shouldBeRunning) {
                            engine->_isRunning = shouldBeRunning; // Update state
                            eventId = shouldBeRunning ? SIG_EVT_STARTED : SIG_EVT_STOPPED; // START or STOP event
                            if (!shouldBeRunning) { // Transitioning to stopped
                                engine->_startTimeMicros = 0; // Clear start time as well
                                // Keep _accumulatedTicks
                            } else { // Transitioning FROM stopped TO running
                                 engine->_accumulatedTicks = 0; // Treat as a fresh start if duty was 0
                                 // _startTimeMicros was already set above
                            }
                        } else {
                             eventId = SIG_EVT_PARAMS_CHANGED; // Just params changed
                        }
                        stateChanged = true;
                        Serial.printf("CmdDispatcherTask: Parameters updated. Running: %s\n", engine->_isRunning ? "true" : "false");
                    } // End new scope for this case
                    break;

                case SIG_CMD_SET_PIN:
                    Serial.printf("CmdDispatcherTask: Handling SET_PIN command (Pin: %d)\n", receivedCmd.pin);
                    // Validate pin number (GPIO 12-19)
                    if (receivedCmd.pin >= 12 && receivedCmd.pin <= 19) {
                        if (receivedCmd.pin != engine->_outputPin) {
                            Serial.printf("CmdDispatcherTask: Pin changed from %d to %d. Applying.\n", engine->_outputPin, receivedCmd.pin);

                            // Save the new pin to NVS
                            preferences.begin(DEVICE_CFG_NAMESPACE, false); // Open R/W
                            preferences.putUChar(OUTPUT_PIN_KEY, receivedCmd.pin);
                            preferences.end();
                            Serial.printf(" - Saved pin %d to NVS (Namespace: %s, Key: %s)\n", receivedCmd.pin, DEVICE_CFG_NAMESPACE, OUTPUT_PIN_KEY);

                            // Update the engine's internal state
                            engine->_outputPin = receivedCmd.pin;

                            // Reconfigure master channel (channel 0) with the new pin
                            engine->pulseGen.configureMaster(receivedCmd.pin);

                            Serial.printf("CmdDispatcherTask: Master output pin successfully set to %d.\n", engine->_outputPin);
                        } else {
                            Serial.printf("CmdDispatcherTask: Pin %d is already the current pin. No change needed.\n", receivedCmd.pin);
                        }
                    } else {
                        Serial.printf("CmdDispatcherTask: Error - Invalid pin %d received. Must be between 12 and 19.\n", receivedCmd.pin);
                    }
                    // No state change event needed for pin change unless explicitly desired
                    stateChanged = false; // Prevent default PARAM_CHANGED event
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
                    eventData.current_freq = (uint32_t)engine->_currentFrequencyHz;
                    eventData.current_duty = engine->_currentDutyCycle;
                    eventData.duration_sec = engine->_lastAppliedDurationSec; // Populate duration for START/UPDATE
                    eventData.current_ticks = engine->getEstimatedCycleCount(); // Populate with calculated cycles
                }
                // Always include the current pin in the event data
                eventData.output_pin = engine->_outputPin; 

                // Post the event to the default event loop
                esp_err_t post_err = esp_event_post(SIGNAL_EVENTS, eventId, &eventData, sizeof(eventData), portMAX_DELAY);
                if (post_err != ESP_OK) {
                     Serial.printf("Error posting signal event %d: %s\n", eventId, esp_err_to_name(post_err));
                } else {
                     Serial.printf("Posted event: Base=%s, ID=%d\n", "SIGNAL_EVENTS", eventId);
                }
            }
        }
    }
} 