#include <unity.h>

// Include mocks FIRST so they override real headers
#include "mocks/mocks.h"

// Now include the headers
#include "SignalState.h"
#include "TimingController.h"

// Include mock implementations
#include "../mocks/mocks.cpp"

// Directly include the source files we're testing (to avoid library auto-build issues)
// This way we only compile what we need for this test
#include "../../lib/SignalEngine/src/SignalState.cpp"
#include "../../lib/SignalEngine/src/TimingController.cpp"

// Global mock time for esp_timer_get_time()
static uint64_t g_mockTimeMicros = 0;

// Override esp_timer_get_time() for testing
#ifdef NATIVE_TEST
uint64_t esp_timer_get_time() {
    return g_mockTimeMicros;
}
#endif

// Test helper: Set mock time
void setMockTime(uint64_t timeMicros) {
    g_mockTimeMicros = timeMicros;
}

// Test helper: Advance mock time
void advanceMockTime(uint64_t deltaMicros) {
    g_mockTimeMicros += deltaMicros;
}

// ==================== Test Setup/Teardown ====================

void setUp(void) {
    // Reset mock time before each test
    g_mockTimeMicros = 0;
}

void tearDown(void) {
    // Nothing to clean up per test
}

// ==================== calculateCycles() Tests ====================

void test_calculateCycles_basic() {
    TimingController tc;

    // Test: 1 second at 1 Hz = 1 cycle
    uint64_t cycles = tc.calculateCycles(0, 1000000, 1.0);
    TEST_ASSERT_EQUAL_UINT64(1, cycles);

    // Test: 1 second at 10 Hz = 10 cycles
    cycles = tc.calculateCycles(0, 1000000, 10.0);
    TEST_ASSERT_EQUAL_UINT64(10, cycles);

    // Test: 2 seconds at 5 Hz = 10 cycles
    cycles = tc.calculateCycles(0, 2000000, 5.0);
    TEST_ASSERT_EQUAL_UINT64(10, cycles);

    // Test: 0.5 seconds at 10 Hz = 5 cycles
    cycles = tc.calculateCycles(0, 500000, 10.0);
    TEST_ASSERT_EQUAL_UINT64(5, cycles);
}

void test_calculateCycles_fractional_cycles() {
    TimingController tc;

    // Test: 1.5 seconds at 1 Hz = 1.5 cycles (truncates to 1)
    uint64_t cycles = tc.calculateCycles(0, 1500000, 1.0);
    TEST_ASSERT_EQUAL_UINT64(1, cycles);

    // Test: 0.99 seconds at 1 Hz = 0.99 cycles (truncates to 0)
    cycles = tc.calculateCycles(0, 990000, 1.0);
    TEST_ASSERT_EQUAL_UINT64(0, cycles);

    // Test: 1.01 seconds at 1 Hz = 1.01 cycles (truncates to 1)
    cycles = tc.calculateCycles(0, 1010000, 1.0);
    TEST_ASSERT_EQUAL_UINT64(1, cycles);
}

void test_calculateCycles_high_frequency() {
    TimingController tc;

    // Test: 1 second at 1 kHz = 1000 cycles
    uint64_t cycles = tc.calculateCycles(0, 1000000, 1000.0);
    TEST_ASSERT_EQUAL_UINT64(1000, cycles);

    // Test: 1 second at 1 MHz = 1,000,000 cycles
    cycles = tc.calculateCycles(0, 1000000, 1000000.0);
    TEST_ASSERT_EQUAL_UINT64(1000000, cycles);

    // Test: 1 millisecond at 1 kHz = 1 cycle
    cycles = tc.calculateCycles(0, 1000, 1000.0);
    TEST_ASSERT_EQUAL_UINT64(1, cycles);
}

void test_calculateCycles_with_offset() {
    TimingController tc;

    // Test: 1 second elapsed (1M to 2M) at 10 Hz = 10 cycles
    uint64_t cycles = tc.calculateCycles(1000000, 2000000, 10.0);
    TEST_ASSERT_EQUAL_UINT64(10, cycles);

    // Test: Arbitrary start time
    cycles = tc.calculateCycles(5000000, 6000000, 5.0);
    TEST_ASSERT_EQUAL_UINT64(5, cycles);
}

void test_calculateCycles_edge_cases() {
    TimingController tc;

    // Test: Zero elapsed time = 0 cycles
    uint64_t cycles = tc.calculateCycles(1000000, 1000000, 10.0);
    TEST_ASSERT_EQUAL_UINT64(0, cycles);

    // Test: End before start = 0 cycles
    cycles = tc.calculateCycles(2000000, 1000000, 10.0);
    TEST_ASSERT_EQUAL_UINT64(0, cycles);

    // Test: Zero frequency = 0 cycles
    cycles = tc.calculateCycles(0, 1000000, 0.0);
    TEST_ASSERT_EQUAL_UINT64(0, cycles);

    // Test: Negative frequency = 0 cycles
    cycles = tc.calculateCycles(0, 1000000, -5.0);
    TEST_ASSERT_EQUAL_UINT64(0, cycles);
}

void test_calculateCycles_overflow_protection() {
    TimingController tc;

    // Test: Very long duration with very high frequency
    // This would overflow uint64_t if not protected
    uint64_t maxTime = UINT64_MAX;
    double veryHighFreq = 1e15; // 1 PHz (absurdly high)

    uint64_t cycles = tc.calculateCycles(0, maxTime, veryHighFreq);

    // Should clamp to UINT64_MAX, not wrap around
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, cycles);
}

// ==================== onStart/onStop/onFrequencyChange Tests ====================

void test_onStart_resets_accumulator() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Set some initial accumulated ticks
    state.setAccumulatedTicks(100);

    // Start with reset
    setMockTime(5000000); // 5 seconds
    tc.onStart(state, true);

    // Should reset accumulator and set start time
    TEST_ASSERT_EQUAL_UINT64(0, state.getAccumulatedTicks());
    TEST_ASSERT_EQUAL_UINT64(5000000, state.getStartTimeMicros());
}

void test_onStart_preserves_accumulator() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Set some initial accumulated ticks
    state.setAccumulatedTicks(100);

    // Start without reset
    setMockTime(5000000); // 5 seconds
    tc.onStart(state, false);

    // Should preserve accumulator
    TEST_ASSERT_EQUAL_UINT64(100, state.getAccumulatedTicks());
    TEST_ASSERT_EQUAL_UINT64(5000000, state.getStartTimeMicros());
}

void test_onStop_accumulates_cycles() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start at time 100ms with 10 Hz
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    tc.onStart(state, true); // Sets start time to current mock time (100000)

    // Stop after 1 second more (should have 10 cycles)
    setMockTime(1100000);  // 100000 + 1000000
    tc.onStop(state);

    // Should accumulate 10 cycles and reset start time
    TEST_ASSERT_EQUAL_UINT64(10, state.getAccumulatedTicks());
    TEST_ASSERT_EQUAL_UINT64(0, state.getStartTimeMicros());
}

void test_onStop_adds_to_existing_accumulator() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 50 accumulated ticks
    state.setAccumulatedTicks(50);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);

    setMockTime(100000);
    tc.onStart(state, false); // Sets start time, preserves accumulator

    // Stop after 1 second (10 more cycles)
    setMockTime(1100000);  // 100000 + 1000000
    tc.onStop(state);

    // Should have 50 + 10 = 60 cycles
    TEST_ASSERT_EQUAL_UINT64(60, state.getAccumulatedTicks());
}

void test_onFrequencyChange_accumulates_with_old_frequency() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start at time 0 with 10 Hz
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    tc.onStart(state, true); // Sets start time to current mock time

    // Change frequency after 1 second (should accumulate 10 cycles at old frequency)
    setMockTime(1100000);  // 100000 + 1000000
    double oldFreq = 10.0;
    tc.onFrequencyChange(state, oldFreq);

    // Should accumulate 10 cycles and reset start time to current time
    TEST_ASSERT_EQUAL_UINT64(10, state.getAccumulatedTicks());
    TEST_ASSERT_EQUAL_UINT64(1100000, state.getStartTimeMicros());
}

void test_onFrequencyChange_uses_old_frequency_correctly() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start at 5 Hz
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(5.0); // Current is 5 Hz
    state.setAccumulatedTicks(0);
    tc.onStart(state, true); // Sets start time to current mock time

    // After 1 second, frequency changes to 20 Hz
    // But we need to count cycles at OLD frequency (5 Hz)
    setMockTime(1100000);  // 100000 + 1000000
    double oldFreq = 5.0; // OLD frequency
    state.setCurrentFrequencyHz(20.0); // Update to new frequency
    tc.onFrequencyChange(state, oldFreq);

    // Should accumulate 5 cycles (using old 5 Hz), not 20
    TEST_ASSERT_EQUAL_UINT64(5, state.getAccumulatedTicks());
}

// ==================== getEstimatedCycleCount Tests ====================

void test_getEstimatedCycleCount_when_stopped() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Set accumulator but not running
    state.setAccumulatedTicks(100);
    state.setRunning(false);

    // Should return just the accumulated ticks
    uint64_t count = tc.getEstimatedCycleCount(state);
    TEST_ASSERT_EQUAL_UINT64(100, count);
}

void test_getEstimatedCycleCount_when_running() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start at time 0 with 10 Hz, 50 accumulated ticks
    setMockTime(100000);
    state.setAccumulatedTicks(50);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    tc.onStart(state, false); // Sets start time, preserves accumulator

    // Advance to 1 second (10 more cycles)
    setMockTime(1100000);  // 100000 + 1000000

    uint64_t count = tc.getEstimatedCycleCount(state);
    // Should be 50 (accumulated) + 10 (current segment) = 60
    TEST_ASSERT_EQUAL_UINT64(60, count);
}

void test_getEstimatedCycleCount_zero_start_time() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Running but start time is 0
    state.setAccumulatedTicks(25);
    state.setRunning(true);
    state.setStartTimeMicros(0);

    setMockTime(1000000);

    // Should handle gracefully (calculateCycles returns 0 when start == 0)
    uint64_t count = tc.getEstimatedCycleCount(state);
    TEST_ASSERT_EQUAL_UINT64(25, count); // Only accumulated
}

// ==================== checkTimeouts - Duration Tests ====================

void test_checkTimeouts_no_timeout_when_stopped() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Set duration but not running
    state.setRunning(false);
    state.setRequestedDurationSec(1.0);
    state.setDurationStartTimeMicros(0);
    state.setUsingPulseCount(false);

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);
}

void test_checkTimeouts_duration_not_expired() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 2 second duration
    setMockTime(0);
    state.setRunning(true);
    state.setRequestedDurationSec(2.0);
    state.setDurationStartTimeMicros(0);
    state.setUsingPulseCount(false);

    // Check after 1 second (not expired yet)
    setMockTime(1000000);

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);

    // Duration should still be set
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2.0, state.getRequestedDurationSec());
}

void test_checkTimeouts_duration_expired() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 1 second duration
    setMockTime(100000);
    state.setRunning(true);
    state.setRequestedDurationSec(1.0);
    state.setDurationStartTimeMicros(esp_timer_get_time());
    state.setUsingPulseCount(false);

    // Check after 1.5 seconds (expired)
    setMockTime(1600000);  // 100000 + 1500000

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(timeoutCalled);

    // Duration should be reset to 0
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, state.getRequestedDurationSec());
    TEST_ASSERT_EQUAL_UINT64(0, state.getDurationStartTimeMicros());
}

void test_checkTimeouts_duration_exactly_expired() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 1 second duration
    setMockTime(100000);
    state.setRunning(true);
    state.setRequestedDurationSec(1.0);
    state.setDurationStartTimeMicros(esp_timer_get_time());
    state.setUsingPulseCount(false);

    // Check after exactly 1 second
    setMockTime(1100000);  // 100000 + 1000000

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(timeoutCalled);
}

void test_checkTimeouts_duration_zero_means_infinite() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Duration = 0 means infinite (no timeout)
    setMockTime(0);
    state.setRunning(true);
    state.setRequestedDurationSec(0.0);
    state.setDurationStartTimeMicros(0);
    state.setUsingPulseCount(false);

    // Check after a long time
    setMockTime(1000000000); // 1000 seconds

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);
}

// ==================== checkTimeouts - Pulse Count Tests ====================

void test_checkTimeouts_pulse_count_not_reached() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 100 pulse target at 10 Hz
    setMockTime(0);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    state.setStartTimeMicros(0);
    state.setRequestedPulseCount(100);
    state.setUsingPulseCount(true);

    // After 5 seconds = 50 pulses (not reached yet)
    setMockTime(5000000);

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);

    // Pulse count should still be set
    TEST_ASSERT_EQUAL_UINT64(100, state.getRequestedPulseCount());
}

void test_checkTimeouts_pulse_count_reached() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 50 pulse target at 10 Hz
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    tc.onStart(state, true); // Sets start time to current mock time
    state.setRequestedPulseCount(50);
    state.setUsingPulseCount(true);

    // After 5 seconds = 50 pulses (reached)
    setMockTime(5100000);  // 100000 + 5000000

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(timeoutCalled);

    // Pulse count should be reset
    TEST_ASSERT_EQUAL_UINT64(0, state.getRequestedPulseCount());
    TEST_ASSERT_FALSE(state.isUsingPulseCount());
}

void test_checkTimeouts_pulse_count_exceeded() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 30 pulse target at 10 Hz
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    tc.onStart(state, true); // Sets start time to current mock time
    state.setRequestedPulseCount(30);
    state.setUsingPulseCount(true);

    // After 5 seconds = 50 pulses (exceeded target)
    setMockTime(5100000);  // 100000 + 5000000

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(timeoutCalled);
}

void test_checkTimeouts_pulse_count_with_accumulator() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Start with 100 accumulated ticks, target 120 at 10 Hz
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(100); // Already have 100
    tc.onStart(state, false); // Sets start time, preserves accumulator
    state.setRequestedPulseCount(120);
    state.setUsingPulseCount(true);

    // After 1 second = 10 more pulses (100 + 10 = 110, not reached)
    setMockTime(1100000);  // 100000 + 1000000

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);

    // After 3 seconds total = 30 more pulses (100 + 30 = 130, reached)
    setMockTime(3100000);  // 100000 + 3000000

    result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(timeoutCalled);
}

void test_checkTimeouts_pulse_count_zero_means_infinite() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Pulse count = 0 means infinite (no timeout)
    setMockTime(0);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    state.setStartTimeMicros(0);
    state.setRequestedPulseCount(0);
    state.setUsingPulseCount(true);

    // After many pulses
    setMockTime(1000000000); // Very long time

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);
}

// ==================== checkTimeouts - Priority Tests ====================

void test_checkTimeouts_pulse_count_has_priority_over_duration() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Set both pulse count (will expire first) and duration
    setMockTime(100000);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    tc.onStart(state, true); // Sets start time to current mock time
    state.setRequestedPulseCount(50); // 5 seconds at 10 Hz
    state.setUsingPulseCount(true);
    state.setRequestedDurationSec(10.0); // 10 seconds
    state.setDurationStartTimeMicros(esp_timer_get_time());

    // After 5 seconds, pulse count is reached but duration is not
    setMockTime(5100000);  // 100000 + 5000000

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(timeoutCalled);

    // Pulse count should be reset (it triggered)
    TEST_ASSERT_EQUAL_UINT64(0, state.getRequestedPulseCount());
    TEST_ASSERT_FALSE(state.isUsingPulseCount());

    // Duration should NOT be reset (it didn't trigger)
    // Note: Based on code, duration is only checked if NOT using pulse count
    // So duration would still be set but not checked
    TEST_ASSERT_FLOAT_WITHIN(0.01, 10.0, state.getRequestedDurationSec());
}

void test_checkTimeouts_duration_ignored_when_using_pulse_count() {
    SignalState state;
    TimingController tc;
    state.begin();

    // Using pulse count mode, even if duration is expired
    setMockTime(0);
    state.setRunning(true);
    state.setCurrentFrequencyHz(10.0);
    state.setAccumulatedTicks(0);
    state.setStartTimeMicros(0);
    state.setRequestedPulseCount(1000); // Many pulses (not reached)
    state.setUsingPulseCount(true);
    state.setRequestedDurationSec(1.0); // 1 second duration
    state.setDurationStartTimeMicros(0);

    // After 5 seconds (duration expired but pulse count not reached)
    setMockTime(5000000);

    bool timeoutCalled = false;
    bool result = tc.checkTimeouts(state, [&timeoutCalled]() {
        timeoutCalled = true;
    });

    // Should NOT timeout because using pulse count mode
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(timeoutCalled);
}

// ==================== Main ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // calculateCycles() tests
    RUN_TEST(test_calculateCycles_basic);
    RUN_TEST(test_calculateCycles_fractional_cycles);
    RUN_TEST(test_calculateCycles_high_frequency);
    RUN_TEST(test_calculateCycles_with_offset);
    RUN_TEST(test_calculateCycles_edge_cases);
    RUN_TEST(test_calculateCycles_overflow_protection);

    // onStart/onStop/onFrequencyChange tests
    RUN_TEST(test_onStart_resets_accumulator);
    RUN_TEST(test_onStart_preserves_accumulator);
    RUN_TEST(test_onStop_accumulates_cycles);
    RUN_TEST(test_onStop_adds_to_existing_accumulator);
    RUN_TEST(test_onFrequencyChange_accumulates_with_old_frequency);
    RUN_TEST(test_onFrequencyChange_uses_old_frequency_correctly);

    // getEstimatedCycleCount tests
    RUN_TEST(test_getEstimatedCycleCount_when_stopped);
    RUN_TEST(test_getEstimatedCycleCount_when_running);
    RUN_TEST(test_getEstimatedCycleCount_zero_start_time);

    // checkTimeouts - duration tests
    RUN_TEST(test_checkTimeouts_no_timeout_when_stopped);
    RUN_TEST(test_checkTimeouts_duration_not_expired);
    RUN_TEST(test_checkTimeouts_duration_expired);
    RUN_TEST(test_checkTimeouts_duration_exactly_expired);
    RUN_TEST(test_checkTimeouts_duration_zero_means_infinite);

    // checkTimeouts - pulse count tests
    RUN_TEST(test_checkTimeouts_pulse_count_not_reached);
    RUN_TEST(test_checkTimeouts_pulse_count_reached);
    RUN_TEST(test_checkTimeouts_pulse_count_exceeded);
    RUN_TEST(test_checkTimeouts_pulse_count_with_accumulator);
    RUN_TEST(test_checkTimeouts_pulse_count_zero_means_infinite);

    // checkTimeouts - priority tests
    RUN_TEST(test_checkTimeouts_pulse_count_has_priority_over_duration);
    RUN_TEST(test_checkTimeouts_duration_ignored_when_using_pulse_count);

    return UNITY_END();
}
