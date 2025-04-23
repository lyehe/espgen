#include "SignalEngine.h"
#include <Arduino.h> // For Serial, pinMode etc.
#include "build_opts.h" // Include project-wide build options/defaults
#include "signal_iface.h" // Make sure this is included
#include "esp_event.h" // Include ESP event library
#include "esp_timer.h" // Include for esp_timer_get_time()

// Define the event base we declared in signal_iface.h
ESP_EVENT_DEFINE_BASE(SIGNAL_EVENTS);

// Forward declare the heartbeat task function from LedcDriver.cpp
// It's better practice to put this in a shared header or manage tasks differently later,
// but for Phase 1, this keeps LedcDriver self-contained.
extern void ledc_heartbeat_task(void *pvParameters);

SignalEngine::SignalEngine() :
    _outputPin(DEFAULT_OUTPUT_PIN), // Initialize output pin
    ledcChannel0(DEFAULT_OUTPUT_PIN, DEFAULT_LEDC_CHANNEL, DEFAULT_FREQUENCY_HZ, DEFAULT_LEDC_RESOLUTION),
    _currentFrequencyHz(DEFAULT_FREQUENCY_HZ), // Initialize state variable
    _currentDutyCycle(DEFAULT_DUTY_CYCLE),      // Initialize state variable
    _isRunning(false), // Initialize isRunning state
    _startTimeMicros(0), // Initialize start time
    _accumulatedTicks(0) // Initialize accumulated ticks
{
    // Constructor body (if needed)
}                           

void SignalEngine::begin() {
    Serial.println("SignalEngine: Initializing...");
    _startTimeMicros = 0; // Ensure start time is 0 initially
    _accumulatedTicks = 0; // Ensure accumulated ticks are 0 initially

    // Create the command queue
    xQueueCmd = xQueueCreate(SIGNAL_ENGINE_CMD_QUEUE_LEN, sizeof(SignalCmd));
    if (xQueueCmd == NULL) {
        Serial.println("SignalEngine: Error creating command queue!");
        // Handle error appropriately - maybe halt or signal critical failure
        return;
    }
    Serial.printf("SignalEngine: Command queue created (size: %d)\n", SIGNAL_ENGINE_CMD_QUEUE_LEN);

    // Initialize the LEDC driver
    ledcChannel0.begin();

    // Set initial duty cycle BUT keep it stopped initially
    // ledcChannel0.setDuty(DEFAULT_DUTY_CYCLE); // Don't start automatically
    _isRunning = false; // Explicitly set initial state
    ledcChannel0.stop(); // Make sure it's stopped

    Serial.printf("SignalEngine: Initialized (Stopped). Default F=%.2f Hz, D=%.2f%%\n", _currentFrequencyHz, _currentDutyCycle * 100.0);

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
        ledc_heartbeat_task,    // Task function
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

// --- Private Static Task Functions ---

void SignalEngine::cmdDispatcherTask(void *pvParameters) {
    SignalEngine* engine = static_cast<SignalEngine*>(pvParameters);
    SignalCmd receivedCmd;
    SignalEvtData eventData; // Structure to hold event data

    Serial.println("CmdDispatcherTask: Starting loop.");

    for (;;) {
        if (xQueueReceive(engine->xQueueCmd, &receivedCmd, portMAX_DELAY) == pdPASS) {
            Serial.printf("CmdDispatcherTask: Received command type %d\n", receivedCmd.type);
            bool stateChanged = false;
            SigEvtId eventId = SIG_EVT_PARAMS_CHANGED; // Default event ID

            switch (receivedCmd.type) {
                case SIG_CMD_START:
                    Serial.println("CmdDispatcherTask: Processing START");
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
                    ledcAttachPin(engine->_outputPin, engine->ledcChannel0.getChannel());
                    engine->ledcChannel0.setFrequency(receivedCmd.frequencyHz);
                    engine->ledcChannel0.setDuty(receivedCmd.dutyCycle);
                    engine->_currentFrequencyHz = receivedCmd.frequencyHz;
                    engine->_currentDutyCycle = receivedCmd.dutyCycle;
                    engine->_isRunning = true;
                    stateChanged = true;
                    eventId = SIG_EVT_STARTED; // Specific event ID for start
                    Serial.printf("CmdDispatcherTask: START applied (Pin %d attached) F=%.2f Hz, D=%.2f%% - Running (Start time: %llu us)\n", engine->_outputPin, engine->_currentFrequencyHz, engine->_currentDutyCycle * 100.0, engine->_startTimeMicros);
                    break;

                case SIG_CMD_STOP:
                    Serial.println("CmdDispatcherTask: Processing STOP");
                    if (engine->_isRunning) { // Only accumulate if it was running
                        uint64_t cycles_just_elapsed = calculateCycles(engine->_startTimeMicros, esp_timer_get_time(), engine->_currentFrequencyHz);
                        engine->_accumulatedTicks += cycles_just_elapsed;
                    }
                    engine->ledcChannel0.stop();
                    engine->_isRunning = false;
                    engine->_startTimeMicros = 0; // Reset start time on stop
                    // Keep _accumulatedTicks value until next start
                    stateChanged = true;
                    eventId = SIG_EVT_STOPPED; // Specific event ID for stop
                    Serial.println("CmdDispatcherTask: STOP applied - Stopped");
                    break;

                 case SIG_CMD_UPDATE_FREQ: // (If re-enabled)
                     Serial.printf("CmdDispatcherTask: Processing UPDATE_FREQ to %.2f Hz\n", receivedCmd.frequencyHz);
                     engine->ledcChannel0.setFrequency(receivedCmd.frequencyHz);
                     engine->_currentFrequencyHz = receivedCmd.frequencyHz;
                     if (engine->_isRunning) { // Only trigger event if running
                         stateChanged = true;
                         eventId = SIG_EVT_PARAMS_CHANGED;
                     }
                     Serial.printf("CmdDispatcherTask: Frequency updated. Running: %s\n", engine->_isRunning ? "true" : "false");
                     break;

                 case SIG_CMD_UPDATE_DUTY: // (If re-enabled)
                      Serial.printf("CmdDispatcherTask: Processing UPDATE_DUTY to %.2f%%\n", receivedCmd.dutyCycle * 100.0);
                     engine->ledcChannel0.setDuty(receivedCmd.dutyCycle);
                     engine->_currentDutyCycle = receivedCmd.dutyCycle;
                     if (engine->_isRunning) { // Only trigger event if running
                         stateChanged = true;
                         eventId = SIG_EVT_PARAMS_CHANGED;
                     }
                     Serial.printf("CmdDispatcherTask: Duty cycle updated. Running: %s\n", engine->_isRunning ? "true" : "false");
                     break;

                case SIG_CMD_UPDATE_ALL:
                    { // Start new scope for this case
                        Serial.printf("CmdDispatcherTask: Processing UPDATE_ALL F=%.2f Hz, D=%.2f%%\n", receivedCmd.frequencyHz, receivedCmd.dutyCycle * 100.0);
                        
                        // Accumulate ticks for the segment just ending (only if running)
                        if (engine->_isRunning) {
                            uint64_t cycles_just_elapsed = calculateCycles(engine->_startTimeMicros, esp_timer_get_time(), engine->_currentFrequencyHz);
                            engine->_accumulatedTicks += cycles_just_elapsed;
                        }
                        
                        // Apply new settings
                        engine->ledcChannel0.setFrequency(receivedCmd.frequencyHz);
                        engine->ledcChannel0.setDuty(receivedCmd.dutyCycle);
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
                            } else { // Transitioning to running (shouldn't happen here if UPDATE kept it running, but for completeness)
                                 engine->_accumulatedTicks = 0; // Treat as a fresh start if duty was 0
                            }
                        } else {
                             eventId = SIG_EVT_PARAMS_CHANGED; // Just params changed
                        }
                        stateChanged = true;
                        Serial.printf("CmdDispatcherTask: Parameters updated. Running: %s\n", engine->_isRunning ? "true" : "false");
                    } // End new scope for this case
                    break;

                default:
                    Serial.printf("CmdDispatcherTask: Received unknown command type %d\n", receivedCmd.type);
                    break;
            }

            // If state changed, post an event
            if (stateChanged) {
                eventData.channel = 0; // Hardcode channel 0 for now
                eventData.current_freq = (uint32_t)engine->_currentFrequencyHz;
                eventData.current_duty = engine->_currentDutyCycle;
                eventData.current_ticks = engine->getEstimatedCycleCount(); // Populate with calculated cycles

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