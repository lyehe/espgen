#include <unity.h>

// Include mocks FIRST
#include "mocks/mocks.h"
#include "mocks/MockPerformanceMonitor.h"

// Include headers
#include "IPerformanceMonitor.h"
#include "PerformanceMonitor.h"

// Include mock implementations
#include "../mocks/mocks.cpp"

// Directly include source files for testing
#include "../../lib/SignalEngine/src/PerformanceMonitor.cpp"

// Global mock time for esp_timer_get_time()
static uint64_t g_mockTimeMicros = 0;

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
    g_mockTimeMicros = 0;
}

void tearDown(void) {
    // Nothing to clean up
}

// ==================== MockPerformanceMonitor Tests ====================

void test_mock_performance_monitor_interface() {
    MockPerformanceMonitor mock;

    // Test that mock implements IPerformanceMonitor interface
    IPerformanceMonitor* interface = &mock;
    TEST_ASSERT_NOT_NULL(interface);
}

void test_mock_starts_with_zero_calls() {
    MockPerformanceMonitor mock;

    TEST_ASSERT_EQUAL_INT(0, mock.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_INT(0, mock.getEndTimingCallCount());
    TEST_ASSERT_EQUAL_INT(0, mock.getRecordEventCallCount());
    TEST_ASSERT_EQUAL_INT(0, mock.getGetMetricsCallCount());
}

void test_mock_tracks_start_timing_calls() {
    MockPerformanceMonitor mock;

    uint64_t time1 = mock.startCommandTiming();
    TEST_ASSERT_EQUAL_INT(1, mock.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_UINT64(123456, time1);

    uint64_t time2 = mock.startCommandTiming();
    TEST_ASSERT_EQUAL_INT(2, mock.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_UINT64(123456, time2);
}

void test_mock_tracks_end_timing_calls() {
    MockPerformanceMonitor mock;

    mock.endCommandTiming(100);
    mock.endCommandTiming(200);
    mock.endCommandTiming(300);

    TEST_ASSERT_EQUAL_INT(3, mock.getEndTimingCallCount());

    const auto& times = mock.getEndTimingStartTimes();
    TEST_ASSERT_EQUAL_size_t(3, times.size());
    TEST_ASSERT_EQUAL_UINT64(100, times[0]);
    TEST_ASSERT_EQUAL_UINT64(200, times[1]);
    TEST_ASSERT_EQUAL_UINT64(300, times[2]);
}

void test_mock_tracks_record_event_calls() {
    MockPerformanceMonitor mock;

    mock.recordEvent(true);
    mock.recordEvent(true);
    mock.recordEvent(false);

    TEST_ASSERT_EQUAL_INT(3, mock.getRecordEventCallCount());
    TEST_ASSERT_EQUAL_INT(2, mock.getSuccessEventCount());
    TEST_ASSERT_EQUAL_INT(1, mock.getFailEventCount());

    const auto& events = mock.getRecordedEvents();
    TEST_ASSERT_EQUAL_size_t(3, events.size());
    TEST_ASSERT_TRUE(events[0]);
    TEST_ASSERT_TRUE(events[1]);
    TEST_ASSERT_FALSE(events[2]);
}

void test_mock_tracks_get_metrics_calls() {
    MockPerformanceMonitor mock;

    PerformanceMetrics metrics;
    mock.getMetrics(metrics);

    TEST_ASSERT_EQUAL_INT(1, mock.getGetMetricsCallCount());
    TEST_ASSERT_EQUAL_UINT32(42, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.avg_latency_us);
}

void test_mock_clear_call_history() {
    MockPerformanceMonitor mock;

    // Make some calls
    mock.startCommandTiming();
    mock.endCommandTiming(100);
    mock.recordEvent(true);

    // Verify calls recorded
    TEST_ASSERT_EQUAL_INT(1, mock.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_INT(1, mock.getEndTimingCallCount());
    TEST_ASSERT_EQUAL_INT(1, mock.getRecordEventCallCount());

    // Clear history
    mock.clearCallHistory();

    // Verify reset
    TEST_ASSERT_EQUAL_INT(0, mock.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_INT(0, mock.getEndTimingCallCount());
    TEST_ASSERT_EQUAL_INT(0, mock.getRecordEventCallCount());
}

// ==================== Real PerformanceMonitor Tests ====================

void test_real_performance_monitor_initialization() {
    PerformanceMonitor monitor;

    PerformanceMetrics metrics;
    monitor.getMetrics(metrics);

    // Initial state should be zero
    TEST_ASSERT_EQUAL_UINT32(0, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.events_published);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.events_failed);
}

void test_real_performance_monitor_command_timing() {
    PerformanceMonitor monitor;

    setMockTime(1000000); // 1 second
    uint64_t startTime = monitor.startCommandTiming();
    TEST_ASSERT_EQUAL_UINT64(1000000, startTime);

    setMockTime(1000150); // 150 microseconds later
    monitor.endCommandTiming(startTime);

    PerformanceMetrics metrics;
    monitor.getMetrics(metrics);

    TEST_ASSERT_EQUAL_UINT32(1, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.avg_latency_us);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.min_latency_us);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.max_latency_us);
}

void test_real_performance_monitor_multiple_commands() {
    PerformanceMonitor monitor;

    // Command 1: 100us latency
    setMockTime(0);
    uint64_t start1 = monitor.startCommandTiming();
    setMockTime(100);
    monitor.endCommandTiming(start1);

    // Command 2: 200us latency
    setMockTime(1000);
    uint64_t start2 = monitor.startCommandTiming();
    setMockTime(1200);
    monitor.endCommandTiming(start2);

    // Command 3: 150us latency
    setMockTime(2000);
    uint64_t start3 = monitor.startCommandTiming();
    setMockTime(2150);
    monitor.endCommandTiming(start3);

    PerformanceMetrics metrics;
    monitor.getMetrics(metrics);

    TEST_ASSERT_EQUAL_UINT32(3, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.avg_latency_us); // (100+200+150)/3 = 150
    TEST_ASSERT_EQUAL_UINT32(100, metrics.min_latency_us);
    TEST_ASSERT_EQUAL_UINT32(200, metrics.max_latency_us);
}

void test_real_performance_monitor_event_tracking() {
    PerformanceMonitor monitor;

    monitor.recordEvent(true);
    monitor.recordEvent(true);
    monitor.recordEvent(false);
    monitor.recordEvent(true);

    PerformanceMetrics metrics;
    monitor.getMetrics(metrics);

    TEST_ASSERT_EQUAL_UINT32(3, metrics.events_published);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.events_failed);
}

void test_real_performance_monitor_reset() {
    PerformanceMonitor monitor;

    // Add some data
    setMockTime(1000);
    uint64_t start = monitor.startCommandTiming();
    setMockTime(1100);
    monitor.endCommandTiming(start);
    monitor.recordEvent(true);
    monitor.recordEvent(false);

    // Reset
    monitor.reset();

    PerformanceMetrics metrics;
    monitor.getMetrics(metrics);

    // All counters should be reset except uptime
    TEST_ASSERT_EQUAL_UINT32(0, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.avg_latency_us);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.events_published);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.events_failed);
}

void test_real_performance_monitor_interface_polymorphism() {
    PerformanceMonitor concreteMonitor;
    IPerformanceMonitor* interfacePtr = &concreteMonitor;

    // Call through interface pointer
    setMockTime(5000);
    uint64_t start = interfacePtr->startCommandTiming();
    TEST_ASSERT_EQUAL_UINT64(5000, start);

    setMockTime(5100);
    interfacePtr->endCommandTiming(start);

    interfacePtr->recordEvent(true);

    PerformanceMetrics metrics;
    interfacePtr->getMetrics(metrics);

    TEST_ASSERT_EQUAL_UINT32(1, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(100, metrics.avg_latency_us);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.events_published);
}

// ==================== Main ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // MockPerformanceMonitor tests
    RUN_TEST(test_mock_performance_monitor_interface);
    RUN_TEST(test_mock_starts_with_zero_calls);
    RUN_TEST(test_mock_tracks_start_timing_calls);
    RUN_TEST(test_mock_tracks_end_timing_calls);
    RUN_TEST(test_mock_tracks_record_event_calls);
    RUN_TEST(test_mock_tracks_get_metrics_calls);
    RUN_TEST(test_mock_clear_call_history);

    // Real PerformanceMonitor tests
    RUN_TEST(test_real_performance_monitor_initialization);
    RUN_TEST(test_real_performance_monitor_command_timing);
    RUN_TEST(test_real_performance_monitor_multiple_commands);
    RUN_TEST(test_real_performance_monitor_event_tracking);
    RUN_TEST(test_real_performance_monitor_reset);
    RUN_TEST(test_real_performance_monitor_interface_polymorphism);

    return UNITY_END();
}
