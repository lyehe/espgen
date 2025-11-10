# SignalEngine Refactoring Plan

## Executive Summary

SignalEngine currently has **10+ distinct responsibilities** violating the Single Responsibility Principle. This document outlines the step-by-step plan to extract focused classes while maintaining backward compatibility.

## Current Responsibilities Analysis

### 1. Command Dispatching (Lines 536-988, ~450 lines)
**Responsibility**: Process commands from FreeRTOS queue via 400+ line switch statement
- `cmdDispatcherTask()` - Static task function
- Command queue management
- Command validation
- 10+ command types (START, STOP, UPDATE_FREQ, UPDATE_DUTY, UPDATE_ALL, SET_PIN, CONFIG_CHANNEL, etc.)

**Extract to**: `CommandDispatcher` class

### 2. State Management (Lines 58-77 in header + getters/setters)
**Responsibility**: Manage runtime state with thread safety
- Current signal parameters (_currentFrequencyHz, _currentDutyCycle, _isRunning)
- Duration/pulse count tracking (_requestedDurationSec, _requestedPulseCount, _usePulseCount)
- Last applied parameters (_lastAppliedFrequencyHz, _lastAppliedDutyCycle, _lastAppliedDurationSec)
- Mutex-protected getters/setters
- Accumulated ticks tracking (_startTimeMicros, _accumulatedTicks)

**Extract to**: `SignalState` class

### 3. NVS Persistence (Lines 58-131, 619-624, 885-888)
**Responsibility**: Load/save settings from/to Non-Volatile Storage
- Load frequency/duty/duration from "SignalEngine" namespace
- Load pin configuration from "device_config" namespace
- Save on START command
- Save on SET_PIN command
- Validation of loaded values

**Extract to**: `SignalPersistence` class

### 4. Timing & Pulse Counting (Lines 211-293, 403-426)
**Responsibility**: Track time and pulses, trigger auto-stop
- Duration-based auto-stop (loop() lines 257-287)
- Pulse count-based auto-stop (loop() lines 235-254)
- Cycle calculation (`calculateCycles` helper function)
- Pulse count estimation (`getEstimatedCycleCount`)
- Accumulator management

**Extract to**: `TimingController` class

### 5. Event Publishing (Lines 960-985)
**Responsibility**: Publish ESP events to notify subscribers
- ESP_EVENT_POST calls
- Event data population (SignalEvtData)
- Event types: SIG_EVT_STARTED, SIG_EVT_STOPPED, SIG_EVT_PARAMS_CHANGED
- Timeout handling for event queue

**Extract to**: `SignalEventPublisher` class

### 6. Initialization & Coordination (Lines 50-209)
**Responsibility**: Orchestrate subsystems (will remain in SignalEngine)
- FreeRTOS task/queue/mutex creation
- PulseGenerator initialization
- Heartbeat task (optional)
- Coordinate extracted classes

**Remains in**: `SignalEngine` (as coordinator/facade)

---

## Refactoring Steps

### Phase 1: Extract SignalState
**Time Estimate**: 2 hours
**Priority**: HIGH (foundation for other extractions)

**Files to Create**:
- `lib/SignalEngine/include/SignalState.h`
- `lib/SignalEngine/src/SignalState.cpp`

**Responsibilities**:
```cpp
class SignalState {
public:
    // Constructor/Initialization
    SignalState();
    bool begin(); // Create mutex

    // Current signal parameters (thread-safe)
    double getCurrentFrequencyHz() const;
    void setCurrentFrequencyHz(double freq);
    float getCurrentDutyCycle() const;
    void setCurrentDutyCycle(float duty);
    bool isRunning() const;
    void setRunning(bool running);

    // Last applied parameters (thread-safe)
    double getLastAppliedFrequencyHz() const;
    void setLastAppliedFrequencyHz(double freq);
    float getLastAppliedDutyCycle() const;
    void setLastAppliedDutyCycle(float duty);
    float getLastAppliedDurationSec() const;
    void setLastAppliedDurationSec(float duration);
    uint64_t getLastAppliedPulseCount() const;
    void setLastAppliedPulseCount(uint64_t count);

    // Duration/pulse count tracking (thread-safe)
    float getRequestedDurationSec() const;
    void setRequestedDurationSec(float duration);
    uint64_t getRequestedPulseCount() const;
    void setRequestedPulseCount(uint64_t count);
    bool isUsingPulseCount() const;
    void setUsingPulseCount(bool usePulseCount);
    uint64_t getDurationStartTimeMicros() const;
    void setDurationStartTimeMicros(uint64_t timeMicros);

    // Timing state (thread-safe)
    uint64_t getStartTimeMicros() const;
    void setStartTimeMicros(uint64_t timeMicros);
    uint64_t getAccumulatedTicks() const;
    void setAccumulatedTicks(uint64_t ticks);
    void addAccumulatedTicks(uint64_t ticks);
    void resetAccumulatedTicks();

    // Pin configuration (thread-safe)
    int getOutputPin() const;
    void setOutputPin(int pin);

    // Atomic operations (acquire mutex once for multiple operations)
    void atomicUpdate(std::function<void()> updateFn);

private:
    // All state variables moved here
    mutable SemaphoreHandle_t _stateMutex;

    double _currentFrequencyHz;
    float _currentDutyCycle;
    bool _isRunning;

    double _lastAppliedFrequencyHz;
    float _lastAppliedDutyCycle;
    float _lastAppliedDurationSec;
    uint64_t _lastAppliedPulseCount;

    float _requestedDurationSec;
    uint64_t _requestedPulseCount;
    uint64_t _durationStartTimeMicros;
    bool _usePulseCount;

    uint64_t _startTimeMicros;
    uint64_t _accumulatedTicks;

    int _outputPin;
};
```

**Integration**:
- SignalEngine.h: Add `SignalState _state;` member
- Replace all `_currentFrequencyHz` → `_state.getCurrentFrequencyHz()`
- Replace all `_isRunning` → `_state.isRunning()`
- Replace all mutex operations on state variables with SignalState methods

**Testing**: Build and verify all getters/setters work

---

### Phase 2: Extract SignalPersistence
**Time Estimate**: 1.5 hours
**Priority**: HIGH (simplifies initialization)

**Files to Create**:
- `lib/SignalEngine/include/SignalPersistence.h`
- `lib/SignalEngine/src/SignalPersistence.cpp`

**Responsibilities**:
```cpp
struct SignalSettings {
    double frequencyHz;
    float dutyCycle;
    float durationSec;
    uint8_t outputPin;
    bool valid;
};

class SignalPersistence {
public:
    SignalPersistence();

    // Load all settings from NVS (both namespaces)
    SignalSettings loadSettings();

    // Save individual settings
    bool saveSignalParams(double freq, float duty, float duration);
    bool saveOutputPin(uint8_t pin);

    // Clear all settings
    bool clearSettings();

private:
    // Namespace keys
    static const char* NVS_NAMESPACE;
    static const char* NVS_KEY_FREQ;
    static const char* NVS_KEY_DUTY;
    static const char* NVS_KEY_DUR;

    // Device config namespace (shared with other modules)
    // Defined in preferences_keys.h: DEVICE_CFG_NAMESPACE, OUTPUT_PIN_KEY
};
```

**Integration**:
- SignalEngine.h: Add `SignalPersistence _persistence;` member
- Replace begin() NVS loading with `SignalSettings settings = _persistence.loadSettings();`
- Replace cmdDispatcherTask NVS saving with `_persistence.saveSignalParams(...)` and `_persistence.saveOutputPin(...)`

**Testing**: Build and verify NVS load/save still works

---

### Phase 3: Extract TimingController
**Time Estimate**: 2 hours
**Priority**: HIGH (complex timing logic)

**Files to Create**:
- `lib/SignalEngine/include/TimingController.h`
- `lib/SignalEngine/src/TimingController.cpp`

**Responsibilities**:
```cpp
// Callback types
typedef std::function<void()> TimeoutCallback;

class TimingController {
public:
    TimingController();

    // Check for auto-stop conditions (called from loop())
    // Returns true if auto-stop was triggered
    bool checkTimeouts(SignalState& state, TimeoutCallback onTimeout);

    // Pulse counting
    uint64_t calculateCycles(uint64_t startMicros, uint64_t endMicros, double frequencyHz) const;
    uint64_t getEstimatedCycleCount(const SignalState& state) const;

    // State management helpers
    void onStart(SignalState& state, bool resetAccumulator);
    void onStop(SignalState& state);
    void onFrequencyChange(SignalState& state);

private:
    // Helper functions
    bool checkPulseCountLimit(SignalState& state);
    bool checkDurationLimit(SignalState& state);
};
```

**Integration**:
- SignalEngine.h: Add `TimingController _timingController;` member
- Replace loop() auto-stop logic with `_timingController.checkTimeouts(_state, stopCallback)`
- Replace `getEstimatedCycleCount()` with `_timingController.getEstimatedCycleCount(_state)`
- Replace `calculateCycles` helper with `_timingController.calculateCycles(...)`

**Testing**: Build and verify duration/pulse count auto-stop still works

---

### Phase 4: Extract SignalEventPublisher
**Time Estimate**: 1 hour
**Priority**: HIGH (simple extraction)

**Files to Create**:
- `lib/SignalEngine/include/SignalEventPublisher.h`
- `lib/SignalEngine/src/SignalEventPublisher.cpp`

**Responsibilities**:
```cpp
class SignalEventPublisher {
public:
    SignalEventPublisher();

    // Publish events
    bool publishStarted(const SignalState& state);
    bool publishStopped(const SignalState& state);
    bool publishParamsChanged(const SignalState& state);

private:
    // Helper to populate event data from state
    void populateEventData(SignalEvtData& eventData, const SignalState& state);

    // Helper to post event with timeout
    bool postEvent(SigEvtId eventId, const SignalEvtData& eventData);

    static const TickType_t EVENT_POST_TIMEOUT_MS = 100;
};
```

**Integration**:
- SignalEngine.h: Add `SignalEventPublisher _eventPublisher;` member
- Replace all `esp_event_post(SIGNAL_EVENTS, ...)` with `_eventPublisher.publishStarted(_state)` etc.
- Remove event data population code from cmdDispatcherTask

**Testing**: Build and verify WebSocket events still work

---

### Phase 5: Extract CommandDispatcher
**Time Estimate**: 3 hours
**Priority**: HIGH (largest extraction, enables Command Pattern)

**Files to Create**:
- `lib/SignalEngine/include/CommandDispatcher.h`
- `lib/SignalEngine/src/CommandDispatcher.cpp`

**Responsibilities**:
```cpp
// Forward declarations
class PulseGenerator;
class SignalState;
class SignalPersistence;
class TimingController;
class SignalEventPublisher;

class CommandDispatcher {
public:
    CommandDispatcher(
        PulseGenerator& pulseGen,
        SignalState& state,
        SignalPersistence& persistence,
        TimingController& timingController,
        SignalEventPublisher& eventPublisher
    );

    bool begin(); // Create queue and task
    bool sendCommand(const SignalCmd& cmd);

private:
    // Dependencies (injected)
    PulseGenerator& _pulseGen;
    SignalState& _state;
    SignalPersistence& _persistence;
    TimingController& _timingController;
    SignalEventPublisher& _eventPublisher;

    // FreeRTOS resources
    QueueHandle_t _commandQueue;
    TaskHandle_t _taskHandle;

    // Task function
    static void dispatcherTask(void* pvParameters);

    // Command handlers (will enable Command Pattern later)
    void handleStart(const SignalCmd& cmd);
    void handleStop(const SignalCmd& cmd);
    void handleUpdateFreq(const SignalCmd& cmd);
    void handleUpdateDuty(const SignalCmd& cmd);
    void handleUpdateAll(const SignalCmd& cmd);
    void handleSetPin(const SignalCmd& cmd);
    void handleSetIndicator(const SignalCmd& cmd);
    void handleConfigChannel(const SignalCmd& cmd);
    void handleEnableChannel(const SignalCmd& cmd);
    void handleSync(const SignalCmd& cmd);
};
```

**Integration**:
- SignalEngine.h: Add `CommandDispatcher _commandDispatcher;` member
- Constructor: Initialize `_commandDispatcher(_pulseGen, _state, _persistence, _timingController, _eventPublisher)`
- Replace `sendCommand()` with `_commandDispatcher.sendCommand(cmd)`
- Remove `cmdDispatcherTask()` from SignalEngine
- Remove command queue/task creation from begin()

**Testing**: Build and verify all commands still work (START, STOP, UPDATE_FREQ, etc.)

---

### Phase 6: Refactor SignalEngine as Coordinator
**Time Estimate**: 1 hour
**Priority**: HIGH (cleanup)

**After all extractions, SignalEngine should**:
- Initialize all subsystems (call `.begin()` on each)
- Provide facade methods (delegate to extracted classes)
- Coordinate interactions between subsystems
- Manage PulseGenerator lifecycle

**Simplified SignalEngine**:
```cpp
class SignalEngine {
public:
    SignalEngine();
    bool begin();
    void loop(); // Delegate to _timingController
    bool sendCommand(const SignalCmd& cmd); // Delegate to _commandDispatcher

    // Status getters (delegate to _state)
    double getCurrentFrequencyHz() const { return _state.getCurrentFrequencyHz(); }
    float getCurrentDutyCycle() const { return _state.getCurrentDutyCycle(); }
    bool isRunning() const { return _state.isRunning(); }
    // ... other getters ...

private:
    // Multi-channel pulse generator
    PulseGenerator _pulseGen;

    // Extracted subsystems (dependencies)
    SignalState _state;
    SignalPersistence _persistence;
    TimingController _timingController;
    SignalEventPublisher _eventPublisher;
    CommandDispatcher _commandDispatcher;

    bool _initialized;
};
```

**Testing**: Full system test (all API endpoints, serial commands, WebSocket events)

---

## Implementation Order

1. **Phase 1**: Extract SignalState (foundation) ✅ START HERE
2. **Phase 2**: Extract SignalPersistence (simplifies init)
3. **Phase 3**: Extract TimingController (complex logic)
4. **Phase 4**: Extract SignalEventPublisher (simple extraction)
5. **Phase 5**: Extract CommandDispatcher (large extraction)
6. **Phase 6**: Refactor SignalEngine as Coordinator

**Total Estimated Time**: 10.5 hours

---

## Success Criteria

- [ ] All unit tests pass (if available)
- [ ] All API endpoints work correctly
- [ ] All serial CLI commands work correctly
- [ ] WebSocket events still fire
- [ ] Duration-based auto-stop works
- [ ] Pulse count-based auto-stop works
- [ ] NVS persistence works (settings survive reboot)
- [ ] Code compiles without warnings
- [ ] No regressions in existing functionality

---

## Benefits After Refactoring

1. **Single Responsibility Principle**: Each class has one clear purpose
2. **Testability**: Can unit test each class independently
3. **Maintainability**: Changes to timing logic don't affect persistence logic
4. **Extensibility**: Easy to add new command types (Command Pattern)
5. **Reduced Complexity**: SignalEngine.cpp shrinks from ~1000 lines to ~200 lines
6. **Better Documentation**: Each class is self-documenting

---

## Next Steps After This Refactoring

1. Implement Command Pattern for command handlers
2. Create IPulseGenerator interface for unit testing
3. Add comprehensive unit tests for each extracted class
4. Consider extracting HeartbeatTask into separate health monitoring class
