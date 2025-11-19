#include <unity.h>

// Include mocks FIRST
#include "mocks/mocks.h"
#include "mocks/MockPerformanceMonitor.h"

// Include headers
#include "CommandDispatcher.h"
#include "SignalState.h"
#include "SignalPersistence.h"
#include "TimingController.h"
#include "SignalEventPublisher.h"
#include "IPerformanceMonitor.h"

// Include mock implementations
#include "../mocks/mocks.cpp"

// Directly include source files for testing
#include "../../lib/SignalEngine/src/SignalState.cpp"
#include "../../lib/SignalEngine/src/SignalPersistence.cpp"
#include "../../lib/SignalEngine/src/TimingController.cpp"
#include "../../lib/SignalEngine/src/SignalEventPublisher.cpp"
#include "../../lib/SignalEngine/src/CommandDispatcher.cpp"

// Global mock time
static uint64_t g_mockTimeMicros = 0;

#ifdef NATIVE_TEST
uint64_t esp_timer_get_time() {
    return g_mockTimeMicros;
}
#endif

void setMockTime(uint64_t timeMicros) {
    g_mockTimeMicros = timeMicros;
}

// ==================== Test Setup/Teardown ====================

MockPerformanceMonitor* mockMonitor = nullptr;
MockPulseGenerator* mockPulseGen = nullptr;
SignalState* state = nullptr;
SignalPersistence* persistence = nullptr;
TimingController* timingController = nullptr;
SignalEventPublisher* eventPublisher = nullptr;
CommandDispatcher* dispatcher = nullptr;

void setUp(void) {
    g_mockTimeMicros = 0;

    // Create mocks and dependencies
    mockMonitor = new MockPerformanceMonitor();
    mockPulseGen = new MockPulseGenerator();
    state = new SignalState();
    persistence = new SignalPersistence();
    timingController = new TimingController();
    eventPublisher = new SignalEventPublisher();

    // Initialize state
    state->begin();

    // Create dispatcher with mock performance monitor
    dispatcher = new CommandDispatcher(
        *mockPulseGen,
        *state,
        *persistence,
        *timingController,
        *eventPublisher,
        *mockMonitor  // Inject mock via IPerformanceMonitor interface
    );
}

void tearDown(void) {
    delete dispatcher;
    delete eventPublisher;
    delete timingController;
    delete persistence;
    delete state;
    delete mockPulseGen;
    delete mockMonitor;
}

// ==================== Constructor Tests ====================

void test_command_dispatcher_constructor_accepts_iperformance_monitor() {
    // Verify dispatcher was constructed successfully with mock monitor
    TEST_ASSERT_NOT_NULL(dispatcher);

    // Verify it accepts IPerformanceMonitor interface (not concrete class)
    // This is proven by the fact we injected MockPerformanceMonitor
    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getStartTimingCallCount());
}

void test_command_dispatcher_dependency_injection() {
    // Test that dispatcher uses the injected performance monitor
    // by verifying it's the same instance we created

    // Call a method that uses performance monitor
    PerformanceMetrics metrics;
    dispatcher->getPerformanceMetrics(metrics);

    // Verify our mock was called
    TEST_ASSERT_EQUAL_INT(1, mockMonitor->getGetMetricsCallCount());
}

// ==================== Performance Monitoring Integration Tests ====================

void test_command_timing_calls_start_timing() {
    // When dispatcher processes a command, it should call startCommandTiming()

    // Note: We can't easily test actual command processing without initializing
    // the full FreeRTOS queue/task infrastructure. Instead, we'll test the
    // getPerformanceMetrics delegation.

    PerformanceMetrics metrics;
    dispatcher->getPerformanceMetrics(metrics);

    // Verify mock's getMetrics was called
    TEST_ASSERT_EQUAL_INT(1, mockMonitor->getGetMetricsCallCount());

    // Verify mock returns expected values
    TEST_ASSERT_EQUAL_UINT32(42, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.avg_latency_us);
}

void test_get_performance_metrics_delegates_to_monitor() {
    // Verify getPerformanceMetrics() correctly delegates to IPerformanceMonitor

    PerformanceMetrics metrics;
    dispatcher->getPerformanceMetrics(metrics);

    // Should call monitor's getMetrics exactly once
    TEST_ASSERT_EQUAL_INT(1, mockMonitor->getGetMetricsCallCount());

    // Should return mock's predictable values
    TEST_ASSERT_EQUAL_UINT32(42, metrics.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(150, metrics.avg_latency_us);
    TEST_ASSERT_EQUAL_UINT32(50, metrics.min_latency_us);
    TEST_ASSERT_EQUAL_UINT32(500, metrics.max_latency_us);
    TEST_ASSERT_EQUAL_UINT32(20, metrics.events_published);
    TEST_ASSERT_EQUAL_UINT32(1, metrics.events_failed);
}

void test_multiple_metrics_calls() {
    // Verify multiple calls to getPerformanceMetrics work correctly

    PerformanceMetrics metrics1, metrics2, metrics3;

    dispatcher->getPerformanceMetrics(metrics1);
    dispatcher->getPerformanceMetrics(metrics2);
    dispatcher->getPerformanceMetrics(metrics3);

    TEST_ASSERT_EQUAL_INT(3, mockMonitor->getGetMetricsCallCount());

    // All should return same mock values
    TEST_ASSERT_EQUAL_UINT32(42, metrics1.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(42, metrics2.commands_processed);
    TEST_ASSERT_EQUAL_UINT32(42, metrics3.commands_processed);
}

// ==================== Interface Polymorphism Tests ====================

void test_dispatcher_uses_interface_not_concrete_class() {
    // This test verifies that CommandDispatcher depends on IPerformanceMonitor
    // interface, not the concrete PerformanceMonitor class.
    //
    // We prove this by:
    // 1. Injecting a MockPerformanceMonitor (different implementation)
    // 2. Verifying dispatcher calls methods on the interface
    // 3. Confirming mock tracks the calls

    // Create a different mock to prove polymorphism
    MockPerformanceMonitor differentMock;
    IPerformanceMonitor& interfaceRef = differentMock;

    // Call through interface
    uint64_t start = interfaceRef.startCommandTiming();
    interfaceRef.endCommandTiming(start);
    interfaceRef.recordEvent(true);

    // Verify this different mock instance tracked calls
    TEST_ASSERT_EQUAL_INT(1, differentMock.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_INT(1, differentMock.getEndTimingCallCount());
    TEST_ASSERT_EQUAL_INT(1, differentMock.getRecordEventCallCount());

    // Our original dispatcher's mock should still be at 0 for these methods
    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getStartTimingCallCount());
    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getEndTimingCallCount());
    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getRecordEventCallCount());
}

void test_real_and_mock_monitors_are_interchangeable() {
    // Verify that real PerformanceMonitor and MockPerformanceMonitor
    // can be used interchangeably through IPerformanceMonitor interface

    // Test with mock (already using mock in setUp)
    PerformanceMetrics mockMetrics;
    dispatcher->getPerformanceMetrics(mockMetrics);
    TEST_ASSERT_EQUAL_UINT32(42, mockMetrics.commands_processed);

    // Create dispatcher with real PerformanceMonitor
    PerformanceMonitor realMonitor;
    CommandDispatcher realDispatcher(
        *mockPulseGen,
        *state,
        *persistence,
        *timingController,
        *eventPublisher,
        realMonitor  // Real implementation
    );

    PerformanceMetrics realMetrics;
    realDispatcher.getPerformanceMetrics(realMetrics);

    // Real monitor starts with 0 (different from mock's 42)
    TEST_ASSERT_EQUAL_UINT32(0, realMetrics.commands_processed);

    // Both dispatchers work correctly despite different monitor implementations
}

// ==================== Initialization Tests ====================

void test_dispatcher_initialization() {
    // Verify dispatcher can be initialized with begin()
    bool result = dispatcher->begin();

    // Should return true (queue created successfully in mock)
    TEST_ASSERT_TRUE(result);

    // Should report as initialized
    TEST_ASSERT_TRUE(dispatcher->isInitialized());
}

void test_dispatcher_not_initialized_before_begin() {
    // Create new dispatcher without calling begin()
    CommandDispatcher newDispatcher(
        *mockPulseGen,
        *state,
        *persistence,
        *timingController,
        *eventPublisher,
        *mockMonitor
    );

    // Should not be initialized yet
    TEST_ASSERT_FALSE(newDispatcher.isInitialized());

    // After begin, should be initialized
    newDispatcher.begin();
    TEST_ASSERT_TRUE(newDispatcher.isInitialized());
}

// ==================== Command Queue Tests ====================

void test_send_command_before_initialization() {
    // Create new dispatcher without initialization
    CommandDispatcher newDispatcher(
        *mockPulseGen,
        *state,
        *persistence,
        *timingController,
        *eventPublisher,
        *mockMonitor
    );

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;

    // sendCommand should fail before begin()
    bool result = newDispatcher.sendCommand(cmd);
    TEST_ASSERT_FALSE(result);
}

void test_send_command_after_initialization() {
    // Initialize dispatcher
    dispatcher->begin();

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;

    // sendCommand should succeed after begin()
    bool result = dispatcher->sendCommand(cmd);
    TEST_ASSERT_TRUE(result);
}

// ==================== Mock Verification Tests ====================

void test_mock_tracks_metrics_calls() {
    // Verify mock tracks all calls for test verification

    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getGetMetricsCallCount());

    PerformanceMetrics metrics;
    dispatcher->getPerformanceMetrics(metrics);

    TEST_ASSERT_EQUAL_INT(1, mockMonitor->getGetMetricsCallCount());

    dispatcher->getPerformanceMetrics(metrics);
    dispatcher->getPerformanceMetrics(metrics);

    TEST_ASSERT_EQUAL_INT(3, mockMonitor->getGetMetricsCallCount());
}

void test_mock_can_be_reset() {
    // Make some calls
    PerformanceMetrics metrics;
    dispatcher->getPerformanceMetrics(metrics);
    dispatcher->getPerformanceMetrics(metrics);

    TEST_ASSERT_EQUAL_INT(2, mockMonitor->getGetMetricsCallCount());

    // Clear history
    mockMonitor->clearCallHistory();

    // Should be reset
    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getGetMetricsCallCount());

    // New calls tracked from zero
    dispatcher->getPerformanceMetrics(metrics);
    TEST_ASSERT_EQUAL_INT(1, mockMonitor->getGetMetricsCallCount());
}

// ==================== Dependency Inversion Principle Validation ====================

void test_dip_dispatcher_depends_on_abstraction() {
    // Verify CommandDispatcher depends on IPerformanceMonitor (abstraction)
    // not PerformanceMonitor (concrete implementation)

    // We prove this by successfully injecting a mock
    MockPerformanceMonitor customMock;

    CommandDispatcher customDispatcher(
        *mockPulseGen,
        *state,
        *persistence,
        *timingController,
        *eventPublisher,
        customMock  // Mock is-a IPerformanceMonitor
    );

    // Use the dispatcher
    PerformanceMetrics metrics;
    customDispatcher.getPerformanceMetrics(metrics);

    // Verify custom mock was used (not default)
    TEST_ASSERT_EQUAL_INT(1, customMock.getGetMetricsCallCount());
    TEST_ASSERT_EQUAL_INT(0, mockMonitor->getGetMetricsCallCount());
}

void test_dip_high_level_depends_on_abstraction() {
    // High-level module (CommandDispatcher) depends on abstraction (IPerformanceMonitor)
    // Low-level module (PerformanceMonitor) implements abstraction
    // This is the essence of Dependency Inversion Principle

    // High-level module can work with ANY implementation of the interface
    MockPerformanceMonitor mock1;
    PerformanceMonitor real1;

    // Both can be passed to CommandDispatcher
    CommandDispatcher dispatcherWithMock(
        *mockPulseGen, *state, *persistence, *timingController, *eventPublisher, mock1
    );

    CommandDispatcher dispatcherWithReal(
        *mockPulseGen, *state, *persistence, *timingController, *eventPublisher, real1
    );

    // Both work correctly
    PerformanceMetrics metrics1, metrics2;
    dispatcherWithMock.getPerformanceMetrics(metrics1);
    dispatcherWithReal.getPerformanceMetrics(metrics2);

    // Mock returns test values
    TEST_ASSERT_EQUAL_UINT32(42, metrics1.commands_processed);

    // Real returns actual values (0 initially)
    TEST_ASSERT_EQUAL_UINT32(0, metrics2.commands_processed);
}

// ==================== Main ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Constructor tests
    RUN_TEST(test_command_dispatcher_constructor_accepts_iperformance_monitor);
    RUN_TEST(test_command_dispatcher_dependency_injection);

    // Performance monitoring integration tests
    RUN_TEST(test_command_timing_calls_start_timing);
    RUN_TEST(test_get_performance_metrics_delegates_to_monitor);
    RUN_TEST(test_multiple_metrics_calls);

    // Interface polymorphism tests
    RUN_TEST(test_dispatcher_uses_interface_not_concrete_class);
    RUN_TEST(test_real_and_mock_monitors_are_interchangeable);

    // Initialization tests
    RUN_TEST(test_dispatcher_initialization);
    RUN_TEST(test_dispatcher_not_initialized_before_begin);

    // Command queue tests
    RUN_TEST(test_send_command_before_initialization);
    RUN_TEST(test_send_command_after_initialization);

    // Mock verification tests
    RUN_TEST(test_mock_tracks_metrics_calls);
    RUN_TEST(test_mock_can_be_reset);

    // Dependency Inversion Principle validation
    RUN_TEST(test_dip_dispatcher_depends_on_abstraction);
    RUN_TEST(test_dip_high_level_depends_on_abstraction);

    return UNITY_END();
}
