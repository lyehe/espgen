#ifndef SIGNAL_STATE_H
#define SIGNAL_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>

/**
 * @brief Manages all runtime state for the signal engine with thread safety.
 *
 * This class encapsulates all mutable state variables that were previously
 * scattered throughout SignalEngine, providing thread-safe access via mutex.
 *
 * Responsibilities:
 * - Current signal parameters (frequency, duty, running state)
 * - Last applied parameters (for button control)
 * - Duration/pulse count tracking
 * - Timing state (start time, accumulated ticks)
 * - Pin configuration
 * - Thread safety via mutex
 *
 * Design Pattern: Single Responsibility Principle
 * Thread Safety: All public methods are mutex-protected
 */
class SignalState {
public:
    SignalState();
    ~SignalState();

    /**
     * @brief Initialize the SignalState (creates mutex)
     * @return true if mutex created successfully, false otherwise
     */
    bool begin();

    // ========== Current Signal Parameters (Thread-Safe) ==========

    double getCurrentFrequencyHz() const;
    void setCurrentFrequencyHz(double freq);

    float getCurrentDutyCycle() const;
    void setCurrentDutyCycle(float duty);

    bool isRunning() const;
    void setRunning(bool running);

    // ========== Last Applied Parameters (Thread-Safe) ==========

    double getLastAppliedFrequencyHz() const;
    void setLastAppliedFrequencyHz(double freq);

    float getLastAppliedDutyCycle() const;
    void setLastAppliedDutyCycle(float duty);

    float getLastAppliedDurationSec() const;
    void setLastAppliedDurationSec(float duration);

    uint64_t getLastAppliedPulseCount() const;
    void setLastAppliedPulseCount(uint64_t count);

    // ========== Duration/Pulse Count Tracking (Thread-Safe) ==========

    float getRequestedDurationSec() const;
    void setRequestedDurationSec(float duration);

    uint64_t getRequestedPulseCount() const;
    void setRequestedPulseCount(uint64_t count);

    bool isUsingPulseCount() const;
    void setUsingPulseCount(bool usePulseCount);

    uint64_t getDurationStartTimeMicros() const;
    void setDurationStartTimeMicros(uint64_t timeMicros);

    // ========== Timing State (Thread-Safe) ==========

    uint64_t getStartTimeMicros() const;
    void setStartTimeMicros(uint64_t timeMicros);

    uint64_t getAccumulatedTicks() const;
    void setAccumulatedTicks(uint64_t ticks);
    void addAccumulatedTicks(uint64_t ticks);
    void resetAccumulatedTicks();

    // ========== Pin Configuration (Thread-Safe) ==========

    int getOutputPin() const;
    void setOutputPin(int pin);

    // ========== Atomic Operations ==========

    /**
     * @brief Execute a function with exclusive access to state
     *
     * Acquires the mutex once for multiple operations, improving performance
     * and ensuring atomicity of complex state updates.
     *
     * @param updateFn Function to execute with mutex held
     *
     * Example:
     * ```cpp
     * state.atomicUpdate([&]() {
     *     state.setCurrentFrequencyHz_nolock(newFreq);
     *     state.setCurrentDutyCycle_nolock(newDuty);
     *     state.setRunning_nolock(true);
     * });
     * ```
     */
    void atomicUpdate(std::function<void()> updateFn);

    // ========== Non-locking setters (for use inside atomicUpdate) ==========
    // WARNING: Only call these from within atomicUpdate()!

    void setCurrentFrequencyHz_nolock(double freq);
    void setCurrentDutyCycle_nolock(float duty);
    void setRunning_nolock(bool running);
    void setLastAppliedFrequencyHz_nolock(double freq);
    void setLastAppliedDutyCycle_nolock(float duty);
    void setLastAppliedDurationSec_nolock(float duration);
    void setLastAppliedPulseCount_nolock(uint64_t count);
    void setRequestedDurationSec_nolock(float duration);
    void setRequestedPulseCount_nolock(uint64_t count);
    void setUsingPulseCount_nolock(bool usePulseCount);
    void setDurationStartTimeMicros_nolock(uint64_t timeMicros);
    void setStartTimeMicros_nolock(uint64_t timeMicros);
    void setAccumulatedTicks_nolock(uint64_t ticks);
    void addAccumulatedTicks_nolock(uint64_t ticks);
    void resetAccumulatedTicks_nolock();
    void setOutputPin_nolock(int pin);

    // ========== Non-locking getters (for use inside atomicUpdate) ==========
    // WARNING: Only call these from within atomicUpdate()!

    double getCurrentFrequencyHz_nolock() const;
    float getCurrentDutyCycle_nolock() const;
    bool isRunning_nolock() const;
    double getLastAppliedFrequencyHz_nolock() const;
    float getLastAppliedDutyCycle_nolock() const;
    float getLastAppliedDurationSec_nolock() const;
    uint64_t getLastAppliedPulseCount_nolock() const;
    float getRequestedDurationSec_nolock() const;
    uint64_t getRequestedPulseCount_nolock() const;
    bool isUsingPulseCount_nolock() const;
    uint64_t getDurationStartTimeMicros_nolock() const;
    uint64_t getStartTimeMicros_nolock() const;
    uint64_t getAccumulatedTicks_nolock() const;
    int getOutputPin_nolock() const;

private:
    // ========== Thread Safety ==========
    mutable SemaphoreHandle_t _stateMutex;

    // Timeout for mutex acquisition (milliseconds)
    static const TickType_t MUTEX_TIMEOUT_MS = 10;

    // Helper to acquire mutex with timeout
    bool acquireMutex() const;
    void releaseMutex() const;

    // ========== State Variables ==========

    // Current signal parameters
    double _currentFrequencyHz;
    float _currentDutyCycle;
    bool _isRunning;

    // Last applied parameters (for button control)
    double _lastAppliedFrequencyHz;
    float _lastAppliedDutyCycle;
    float _lastAppliedDurationSec;
    uint64_t _lastAppliedPulseCount;

    // Duration/pulse count tracking
    float _requestedDurationSec;
    uint64_t _requestedPulseCount;
    uint64_t _durationStartTimeMicros;
    bool _usePulseCount;

    // Timing state
    uint64_t _startTimeMicros;
    uint64_t _accumulatedTicks;

    // Pin configuration
    int _outputPin;
};

#endif // SIGNAL_STATE_H
