#ifndef TIMING_CONTROLLER_H
#define TIMING_CONTROLLER_H

#include <Arduino.h>
#include <functional>
#include "SignalState.h"

/**
 * @brief Manages timing, pulse counting, and auto-stop functionality.
 *
 * This class encapsulates all timing-related logic that was previously
 * scattered throughout SignalEngine, providing clean interfaces for
 * timing operations and auto-stop detection.
 *
 * Responsibilities:
 * - Calculate elapsed cycles based on time and frequency
 * - Track accumulated pulse counts across frequency changes
 * - Check for duration-based auto-stop conditions
 * - Check for pulse-count-based auto-stop conditions
 * - Provide estimated cycle count for monitoring
 *
 * Design Pattern: Single Responsibility Principle
 * Thread Safety: Works with SignalState which handles mutex protection
 */

// Callback type for auto-stop events
typedef std::function<void()> TimeoutCallback;

class TimingController {
public:
    TimingController();
    ~TimingController();

    /**
     * @brief Check for auto-stop timeout conditions
     *
     * Checks both duration-based and pulse-count-based auto-stop conditions.
     * If a timeout is detected, resets the appropriate state variables and
     * invokes the callback.
     *
     * This should be called periodically from the main loop.
     *
     * @param state Reference to SignalState for reading/updating timing state
     * @param onTimeout Callback to invoke if auto-stop condition is met
     * @return true if auto-stop was triggered, false otherwise
     */
    bool checkTimeouts(SignalState& state, TimeoutCallback onTimeout);

    /**
     * @brief Calculate number of cycles elapsed between two timestamps
     *
     * @param startMicros Start timestamp in microseconds (from esp_timer_get_time())
     * @param endMicros End timestamp in microseconds
     * @param frequencyHz Signal frequency in Hz
     * @return Number of complete cycles elapsed (with overflow protection)
     */
    uint64_t calculateCycles(uint64_t startMicros, uint64_t endMicros, double frequencyHz) const;

    /**
     * @brief Get estimated total cycle count
     *
     * Calculates total cycles including:
     * - Accumulated cycles from previous segments
     * - Cycles in current running segment (if running)
     *
     * @param state Reference to SignalState for reading timing state
     * @return Estimated total cycle count
     */
    uint64_t getEstimatedCycleCount(const SignalState& state) const;

    /**
     * @brief Handle timing state when signal starts
     *
     * Updates start time and optionally resets accumulator based on mode.
     *
     * @param state Reference to SignalState for updating timing state
     * @param resetAccumulator If true, resets accumulated ticks (for new pulse count)
     */
    void onStart(SignalState& state, bool resetAccumulator);

    /**
     * @brief Handle timing state when signal stops
     *
     * Accumulates final cycles and resets start time.
     *
     * @param state Reference to SignalState for updating timing state
     */
    void onStop(SignalState& state);

    /**
     * @brief Handle timing state when frequency changes
     *
     * Accumulates cycles with old frequency before frequency change,
     * then resets start time for new frequency segment.
     *
     * @param state Reference to SignalState for updating timing state
     * @param oldFrequency Previous frequency (for accurate cycle calculation)
     */
    void onFrequencyChange(SignalState& state, double oldFrequency);

private:
    /**
     * @brief Check pulse count limit for auto-stop
     *
     * @param state Reference to SignalState
     * @return true if pulse count limit reached, false otherwise
     */
    bool checkPulseCountLimit(SignalState& state);

    /**
     * @brief Check duration limit for auto-stop
     *
     * @param state Reference to SignalState
     * @return true if duration limit reached, false otherwise
     */
    bool checkDurationLimit(SignalState& state);

    /**
     * @brief Convert duration in seconds to microseconds with overflow protection
     *
     * @param durationSec Duration in seconds
     * @return Duration in microseconds (clamped to UINT64_MAX if overflow)
     */
    uint64_t durationToMicros(float durationSec) const;

    // Constants
    static const double MAX_UINT64_AS_DOUBLE;
};

#endif // TIMING_CONTROLLER_H
