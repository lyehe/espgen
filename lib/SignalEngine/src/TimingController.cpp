#include "TimingController.h"
#include "esp_timer.h"

// Define constant
const double TimingController::MAX_UINT64_AS_DOUBLE = 18446744073709551615.0;

TimingController::TimingController() {
    // Constructor body (if needed)
}

TimingController::~TimingController() {
    // Destructor body (if needed)
}

// ========== Public Methods ==========

bool TimingController::checkTimeouts(SignalState& state, TimeoutCallback onTimeout) {
    bool timeoutTriggered = false;

    // Use atomicUpdate to check state and potentially trigger timeout
    state.atomicUpdate([this, &state, &timeoutTriggered]() {
        if (!state.isRunning_nolock()) {
            return; // Nothing to check if not running
        }

        // Check pulse count limit first (higher priority)
        if (checkPulseCountLimit(state)) {
            timeoutTriggered = true;
            return; // Exit early, don't check duration
        }

        // Check duration limit
        if (checkDurationLimit(state)) {
            timeoutTriggered = true;
        }
    });

    // Invoke callback outside critical section if timeout was triggered
    if (timeoutTriggered && onTimeout) {
        onTimeout();
    }

    return timeoutTriggered;
}

uint64_t TimingController::calculateCycles(uint64_t startMicros, uint64_t endMicros, double frequencyHz) const {
    if (endMicros <= startMicros || frequencyHz <= 0) {
        return 0;
    }

    uint64_t elapsedTimeMicros = endMicros - startMicros;
    double elapsedSeconds = (double)elapsedTimeMicros / 1000000.0;
    double cycles = elapsedSeconds * frequencyHz;

    // Check for overflow: UINT64_MAX = 18446744073709551615
    // If cycles would overflow, clamp to UINT64_MAX
    if (cycles >= MAX_UINT64_AS_DOUBLE) {
        return UINT64_MAX;
    }

    // Check for negative (shouldn't happen but safety check)
    if (cycles < 0) {
        return 0;
    }

    return (uint64_t)cycles;
}

uint64_t TimingController::getEstimatedCycleCount(const SignalState& state) const {
    uint64_t result = 0;

    // Use atomicUpdate to read multiple state values consistently
    // Note: We need to cast away const since atomicUpdate modifies mutex
    // This is safe because we're only reading state inside the lambda
    const_cast<SignalState&>(state).atomicUpdate([this, &state, &result]() {
        if (state.isRunning_nolock() && state.getStartTimeMicros_nolock() > 0) {
            uint64_t currentCycles = calculateCycles(
                state.getStartTimeMicros_nolock(),
                esp_timer_get_time(),
                state.getCurrentFrequencyHz_nolock()
            );
            result = state.getAccumulatedTicks_nolock() + currentCycles;
        } else {
            result = state.getAccumulatedTicks_nolock();
        }
    });

    return result;
}

void TimingController::onStart(SignalState& state, bool resetAccumulator) {
    state.atomicUpdate([&state, resetAccumulator]() {
        if (resetAccumulator) {
            state.resetAccumulatedTicks_nolock();
        }
        state.setStartTimeMicros_nolock(esp_timer_get_time());
    });
}

void TimingController::onStop(SignalState& state) {
    state.atomicUpdate([this, &state]() {
        // Calculate final ticks BEFORE resetting state
        if (state.isRunning_nolock() && state.getStartTimeMicros_nolock() > 0) {
            uint64_t cycles_just_elapsed = calculateCycles(
                state.getStartTimeMicros_nolock(),
                esp_timer_get_time(),
                state.getCurrentFrequencyHz_nolock()
            );
            state.addAccumulatedTicks_nolock(cycles_just_elapsed);
        }

        // Reset start time
        state.setStartTimeMicros_nolock(0);
    });
}

void TimingController::onFrequencyChange(SignalState& state, double oldFrequency) {
    state.atomicUpdate([this, &state, oldFrequency]() {
        // Accumulate ticks with OLD frequency before changing
        if (state.isRunning_nolock() && state.getStartTimeMicros_nolock() > 0) {
            uint64_t cycles_just_elapsed = calculateCycles(
                state.getStartTimeMicros_nolock(),
                esp_timer_get_time(),
                oldFrequency  // Use OLD frequency for OLD time segment
            );
            state.addAccumulatedTicks_nolock(cycles_just_elapsed);
            state.setStartTimeMicros_nolock(esp_timer_get_time()); // Reset segment start

            Serial.printf("TimingController: Accumulated %llu cycles before frequency change\n", cycles_just_elapsed);
        }
    });
}

// ========== Private Helper Methods ==========

bool TimingController::checkPulseCountLimit(SignalState& state) {
    // Note: Called inside atomicUpdate, state is already locked

    if (!state.isUsingPulseCount_nolock() || state.getRequestedPulseCount_nolock() == 0) {
        return false; // Not using pulse count mode or infinite pulses
    }

    // Calculate current pulses
    uint64_t currentCycles = 0;
    uint64_t startTime = state.getStartTimeMicros_nolock();
    if (startTime > 0) {
        currentCycles = calculateCycles(
            startTime,
            esp_timer_get_time(),
            state.getCurrentFrequencyHz_nolock()
        );
    }
    uint64_t currentPulses = state.getAccumulatedTicks_nolock() + currentCycles;

    if (currentPulses >= state.getRequestedPulseCount_nolock()) {
        Serial.printf("TimingController: Pulse count (%llu) reached. Triggering auto-stop.\n",
                     state.getRequestedPulseCount_nolock());

        // Reset pulse count tracking
        state.setRequestedPulseCount_nolock(0);
        state.setUsingPulseCount_nolock(false);

        return true;
    }

    return false;
}

bool TimingController::checkDurationLimit(SignalState& state) {
    // Note: Called inside atomicUpdate, state is already locked

    if (state.isUsingPulseCount_nolock()) {
        return false; // Using pulse count mode, not duration mode
    }

    if (state.getRequestedDurationSec_nolock() <= 0 || state.getDurationStartTimeMicros_nolock() == 0) {
        return false; // No duration set or infinite duration
    }

    uint64_t nowMicros = esp_timer_get_time();
    uint64_t targetDurationMicros = durationToMicros(state.getRequestedDurationSec_nolock());
    uint64_t elapsedMicros = nowMicros - state.getDurationStartTimeMicros_nolock();

    if (elapsedMicros >= targetDurationMicros) {
        Serial.printf("TimingController: Duration (%.2f s) expired. Triggering auto-stop.\n",
                     state.getRequestedDurationSec_nolock());

        // Reset duration tracking
        state.setRequestedDurationSec_nolock(0);
        state.setDurationStartTimeMicros_nolock(0);

        return true;
    }

    return false;
}

uint64_t TimingController::durationToMicros(float durationSec) const {
    // Convert duration to microseconds with overflow protection
    // Max safe duration: UINT64_MAX / 1000000 = ~18446744073 seconds = ~584 years
    double durationMicrosDouble = durationSec * 1000000.0;

    if (durationMicrosDouble >= MAX_UINT64_AS_DOUBLE) {
        return UINT64_MAX;
    } else if (durationMicrosDouble < 0) {
        return 0;
    } else {
        return (uint64_t)durationMicrosDouble;
    }
}
