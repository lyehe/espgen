#include "SignalState.h"
#include "build_opts.h"

SignalState::SignalState() :
    _stateMutex(NULL),
    _currentFrequencyHz(DEFAULT_FREQUENCY_HZ),
    _currentDutyCycle(DEFAULT_DUTY_CYCLE),
    _isRunning(false),
    _lastAppliedFrequencyHz(DEFAULT_FREQUENCY_HZ),
    _lastAppliedDutyCycle(DEFAULT_DUTY_CYCLE),
    _lastAppliedDurationSec(0.0f),
    _lastAppliedPulseCount(0),
    _requestedDurationSec(0.0f),
    _requestedPulseCount(0),
    _durationStartTimeMicros(0),
    _usePulseCount(false),
    _startTimeMicros(0),
    _accumulatedTicks(0),
    _outputPin(DEFAULT_OUTPUT_PIN)
{
}

SignalState::~SignalState() {
    if (_stateMutex != NULL) {
        vSemaphoreDelete(_stateMutex);
        _stateMutex = NULL;
    }
}

bool SignalState::begin() {
    _stateMutex = xSemaphoreCreateMutex();
    if (_stateMutex == NULL) {
        Serial.println("SignalState: CRITICAL - Failed to create mutex!");
        return false;
    }
    Serial.println("SignalState: Mutex created - thread safety enabled");
    return true;
}

// ========== Helper Methods ==========

bool SignalState::acquireMutex() const {
    if (_stateMutex == NULL) {
        return false; // Mutex not initialized
    }
    return (xSemaphoreTake(_stateMutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE);
}

void SignalState::releaseMutex() const {
    if (_stateMutex != NULL) {
        xSemaphoreGive(_stateMutex);
    }
}

// ========== Current Signal Parameters ==========

double SignalState::getCurrentFrequencyHz() const {
    if (!acquireMutex()) {
        return _currentFrequencyHz; // Best effort
    }
    double result = _currentFrequencyHz;
    releaseMutex();
    return result;
}

void SignalState::setCurrentFrequencyHz(double freq) {
    if (acquireMutex()) {
        _currentFrequencyHz = freq;
        releaseMutex();
    } else {
        _currentFrequencyHz = freq; // Best effort
    }
}

float SignalState::getCurrentDutyCycle() const {
    if (!acquireMutex()) {
        return _currentDutyCycle;
    }
    float result = _currentDutyCycle;
    releaseMutex();
    return result;
}

void SignalState::setCurrentDutyCycle(float duty) {
    if (acquireMutex()) {
        _currentDutyCycle = duty;
        releaseMutex();
    } else {
        _currentDutyCycle = duty;
    }
}

bool SignalState::isRunning() const {
    if (!acquireMutex()) {
        return _isRunning;
    }
    bool result = _isRunning;
    releaseMutex();
    return result;
}

void SignalState::setRunning(bool running) {
    if (acquireMutex()) {
        _isRunning = running;
        releaseMutex();
    } else {
        _isRunning = running;
    }
}

// ========== Last Applied Parameters ==========

double SignalState::getLastAppliedFrequencyHz() const {
    if (!acquireMutex()) {
        return _lastAppliedFrequencyHz;
    }
    double result = _lastAppliedFrequencyHz;
    releaseMutex();
    return result;
}

void SignalState::setLastAppliedFrequencyHz(double freq) {
    if (acquireMutex()) {
        _lastAppliedFrequencyHz = freq;
        releaseMutex();
    } else {
        _lastAppliedFrequencyHz = freq;
    }
}

float SignalState::getLastAppliedDutyCycle() const {
    if (!acquireMutex()) {
        return _lastAppliedDutyCycle;
    }
    float result = _lastAppliedDutyCycle;
    releaseMutex();
    return result;
}

void SignalState::setLastAppliedDutyCycle(float duty) {
    if (acquireMutex()) {
        _lastAppliedDutyCycle = duty;
        releaseMutex();
    } else {
        _lastAppliedDutyCycle = duty;
    }
}

float SignalState::getLastAppliedDurationSec() const {
    if (!acquireMutex()) {
        return _lastAppliedDurationSec;
    }
    float result = _lastAppliedDurationSec;
    releaseMutex();
    return result;
}

void SignalState::setLastAppliedDurationSec(float duration) {
    if (acquireMutex()) {
        _lastAppliedDurationSec = duration;
        releaseMutex();
    } else {
        _lastAppliedDurationSec = duration;
    }
}

uint64_t SignalState::getLastAppliedPulseCount() const {
    if (!acquireMutex()) {
        return _lastAppliedPulseCount;
    }
    uint64_t result = _lastAppliedPulseCount;
    releaseMutex();
    return result;
}

void SignalState::setLastAppliedPulseCount(uint64_t count) {
    if (acquireMutex()) {
        _lastAppliedPulseCount = count;
        releaseMutex();
    } else {
        _lastAppliedPulseCount = count;
    }
}

// ========== Duration/Pulse Count Tracking ==========

float SignalState::getRequestedDurationSec() const {
    if (!acquireMutex()) {
        return _requestedDurationSec;
    }
    float result = _requestedDurationSec;
    releaseMutex();
    return result;
}

void SignalState::setRequestedDurationSec(float duration) {
    if (acquireMutex()) {
        _requestedDurationSec = duration;
        releaseMutex();
    } else {
        _requestedDurationSec = duration;
    }
}

uint64_t SignalState::getRequestedPulseCount() const {
    if (!acquireMutex()) {
        return _requestedPulseCount;
    }
    uint64_t result = _requestedPulseCount;
    releaseMutex();
    return result;
}

void SignalState::setRequestedPulseCount(uint64_t count) {
    if (acquireMutex()) {
        _requestedPulseCount = count;
        releaseMutex();
    } else {
        _requestedPulseCount = count;
    }
}

bool SignalState::isUsingPulseCount() const {
    if (!acquireMutex()) {
        return _usePulseCount;
    }
    bool result = _usePulseCount;
    releaseMutex();
    return result;
}

void SignalState::setUsingPulseCount(bool usePulseCount) {
    if (acquireMutex()) {
        _usePulseCount = usePulseCount;
        releaseMutex();
    } else {
        _usePulseCount = usePulseCount;
    }
}

uint64_t SignalState::getDurationStartTimeMicros() const {
    if (!acquireMutex()) {
        return _durationStartTimeMicros;
    }
    uint64_t result = _durationStartTimeMicros;
    releaseMutex();
    return result;
}

void SignalState::setDurationStartTimeMicros(uint64_t timeMicros) {
    if (acquireMutex()) {
        _durationStartTimeMicros = timeMicros;
        releaseMutex();
    } else {
        _durationStartTimeMicros = timeMicros;
    }
}

// ========== Timing State ==========

uint64_t SignalState::getStartTimeMicros() const {
    if (!acquireMutex()) {
        return _startTimeMicros;
    }
    uint64_t result = _startTimeMicros;
    releaseMutex();
    return result;
}

void SignalState::setStartTimeMicros(uint64_t timeMicros) {
    if (acquireMutex()) {
        _startTimeMicros = timeMicros;
        releaseMutex();
    } else {
        _startTimeMicros = timeMicros;
    }
}

uint64_t SignalState::getAccumulatedTicks() const {
    if (!acquireMutex()) {
        return _accumulatedTicks;
    }
    uint64_t result = _accumulatedTicks;
    releaseMutex();
    return result;
}

void SignalState::setAccumulatedTicks(uint64_t ticks) {
    if (acquireMutex()) {
        _accumulatedTicks = ticks;
        releaseMutex();
    } else {
        _accumulatedTicks = ticks;
    }
}

void SignalState::addAccumulatedTicks(uint64_t ticks) {
    if (acquireMutex()) {
        _accumulatedTicks += ticks;
        releaseMutex();
    } else {
        _accumulatedTicks += ticks;
    }
}

void SignalState::resetAccumulatedTicks() {
    if (acquireMutex()) {
        _accumulatedTicks = 0;
        releaseMutex();
    } else {
        _accumulatedTicks = 0;
    }
}

// ========== Pin Configuration ==========

int SignalState::getOutputPin() const {
    if (!acquireMutex()) {
        return _outputPin;
    }
    int result = _outputPin;
    releaseMutex();
    return result;
}

void SignalState::setOutputPin(int pin) {
    if (acquireMutex()) {
        _outputPin = pin;
        releaseMutex();
    } else {
        _outputPin = pin;
    }
}

// ========== Atomic Operations ==========

void SignalState::atomicUpdate(std::function<void()> updateFn) {
    if (acquireMutex()) {
        updateFn();
        releaseMutex();
    } else {
        // Failed to acquire mutex, execute anyway (best effort)
        updateFn();
    }
}

// ========== Non-locking setters (for use inside atomicUpdate) ==========

void SignalState::setCurrentFrequencyHz_nolock(double freq) {
    _currentFrequencyHz = freq;
}

void SignalState::setCurrentDutyCycle_nolock(float duty) {
    _currentDutyCycle = duty;
}

void SignalState::setRunning_nolock(bool running) {
    _isRunning = running;
}

void SignalState::setLastAppliedFrequencyHz_nolock(double freq) {
    _lastAppliedFrequencyHz = freq;
}

void SignalState::setLastAppliedDutyCycle_nolock(float duty) {
    _lastAppliedDutyCycle = duty;
}

void SignalState::setLastAppliedDurationSec_nolock(float duration) {
    _lastAppliedDurationSec = duration;
}

void SignalState::setLastAppliedPulseCount_nolock(uint64_t count) {
    _lastAppliedPulseCount = count;
}

void SignalState::setRequestedDurationSec_nolock(float duration) {
    _requestedDurationSec = duration;
}

void SignalState::setRequestedPulseCount_nolock(uint64_t count) {
    _requestedPulseCount = count;
}

void SignalState::setUsingPulseCount_nolock(bool usePulseCount) {
    _usePulseCount = usePulseCount;
}

void SignalState::setDurationStartTimeMicros_nolock(uint64_t timeMicros) {
    _durationStartTimeMicros = timeMicros;
}

void SignalState::setStartTimeMicros_nolock(uint64_t timeMicros) {
    _startTimeMicros = timeMicros;
}

void SignalState::setAccumulatedTicks_nolock(uint64_t ticks) {
    _accumulatedTicks = ticks;
}

void SignalState::addAccumulatedTicks_nolock(uint64_t ticks) {
    _accumulatedTicks += ticks;
}

void SignalState::resetAccumulatedTicks_nolock() {
    _accumulatedTicks = 0;
}

void SignalState::setOutputPin_nolock(int pin) {
    _outputPin = pin;
}

// ========== Non-locking getters (for use inside atomicUpdate) ==========

double SignalState::getCurrentFrequencyHz_nolock() const {
    return _currentFrequencyHz;
}

float SignalState::getCurrentDutyCycle_nolock() const {
    return _currentDutyCycle;
}

bool SignalState::isRunning_nolock() const {
    return _isRunning;
}

double SignalState::getLastAppliedFrequencyHz_nolock() const {
    return _lastAppliedFrequencyHz;
}

float SignalState::getLastAppliedDutyCycle_nolock() const {
    return _lastAppliedDutyCycle;
}

float SignalState::getLastAppliedDurationSec_nolock() const {
    return _lastAppliedDurationSec;
}

uint64_t SignalState::getLastAppliedPulseCount_nolock() const {
    return _lastAppliedPulseCount;
}

float SignalState::getRequestedDurationSec_nolock() const {
    return _requestedDurationSec;
}

uint64_t SignalState::getRequestedPulseCount_nolock() const {
    return _requestedPulseCount;
}

bool SignalState::isUsingPulseCount_nolock() const {
    return _usePulseCount;
}

uint64_t SignalState::getDurationStartTimeMicros_nolock() const {
    return _durationStartTimeMicros;
}

uint64_t SignalState::getStartTimeMicros_nolock() const {
    return _startTimeMicros;
}

uint64_t SignalState::getAccumulatedTicks_nolock() const {
    return _accumulatedTicks;
}

int SignalState::getOutputPin_nolock() const {
    return _outputPin;
}
