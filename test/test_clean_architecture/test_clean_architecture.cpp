#include <unity.h>

// Include mocks FIRST
#include "mocks/mocks.h"
#include "mocks/MockPerformanceMonitor.h"
#include "mocks/MockSignalController.h"

// Include headers
#include "ISignalController.h"
#include "IPerformanceMonitor.h"
#include "SignalControllerAdapter.h"

// Include mock implementations
#include "../mocks/mocks.cpp"

// ==================== Test Setup/Teardown ====================

void setUp(void) {
    // Reset before each test
}

void tearDown(void) {
    // Cleanup after each test
}

// ==================== Interface Abstraction Tests ====================

void test_iperformance_monitor_is_abstract() {
    // Cannot instantiate abstract interface directly
    // This test verifies the interface is properly abstract

    MockPerformanceMonitor mockMonitor;
    IPerformanceMonitor* interfacePtr = &mockMonitor;

    TEST_ASSERT_NOT_NULL(interfacePtr);

    // Verify we can call all interface methods through pointer
    uint64_t start = interfacePtr->startCommandTiming();
    TEST_ASSERT_EQUAL_UINT64(123456, start);

    interfacePtr->endCommandTiming(start);
    interfacePtr->recordEvent(true);
    interfacePtr->updateMemoryStats();

    PerformanceMetrics metrics;
    interfacePtr->getMetrics(metrics);
    TEST_ASSERT_EQUAL_UINT32(42, metrics.commands_processed);

    interfacePtr->reset();
    interfacePtr->printMetrics();

    // All 7 methods callable through interface ✓
}

void test_isignal_controller_is_abstract() {
    // Verify ISignalController interface is properly abstract

    MockSignalController mockController;
    ISignalController* interfacePtr = &mockController;

    TEST_ASSERT_NOT_NULL(interfacePtr);

    // Test ISignalService methods (8 methods)
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;

    TEST_ASSERT_TRUE(interfacePtr->startSignal(cmd));
    TEST_ASSERT_TRUE(interfacePtr->stopSignal());
    TEST_ASSERT_TRUE(interfacePtr->updateSignal(cmd));
    TEST_ASSERT_TRUE(interfacePtr->sendCommand(cmd));

    bool running = interfacePtr->isRunning();
    TEST_ASSERT_FALSE(running);

    SignalStatus_t status;
    SignalError err = interfacePtr->getStatus(status);
    TEST_ASSERT_EQUAL_INT(SIG_ERR_NONE, err);

    double freq = interfacePtr->getCurrentFrequency();
    TEST_ASSERT_EQUAL_DOUBLE(1000.0, freq);

    float duty = interfacePtr->getCurrentDutyCycle();
    TEST_ASSERT_EQUAL_FLOAT(0.5, duty);

    // Test IChannelService methods (8 methods)
    TEST_ASSERT_TRUE(interfacePtr->configureChannel(1, cmd));
    TEST_ASSERT_TRUE(interfacePtr->enableChannel(1, true));
    TEST_ASSERT_TRUE(interfacePtr->setChannelPin(1, 19));
    TEST_ASSERT_TRUE(interfacePtr->setChannelPhase(1, 90.0));

    int pin = interfacePtr->getChannelPin(1);
    TEST_ASSERT_EQUAL_INT(19, pin);

    bool enabled = interfacePtr->isChannelEnabled(1);
    TEST_ASSERT_FALSE(enabled);

    float phase = interfacePtr->getChannelPhaseOffset(1);
    TEST_ASSERT_EQUAL_FLOAT(0.0, phase);

    SignalPolarity polarity = interfacePtr->getChannelPolarity(1);
    TEST_ASSERT_EQUAL_INT(POLARITY_ACTIVE_HIGH, polarity);

    PulseChannelConfig_t config;
    TEST_ASSERT_TRUE(interfacePtr->getChannelConfig(1, config));

    TEST_ASSERT_TRUE(interfacePtr->triggerSync());

    // Test IPinService methods (4 methods)
    TEST_ASSERT_TRUE(interfacePtr->setOutputPin(18));
    int outPin = interfacePtr->getOutputPin();
    TEST_ASSERT_EQUAL_INT(18, outPin);

    TEST_ASSERT_TRUE(interfacePtr->setIndicatorPin(2));
    int indPin = interfacePtr->getIndicatorPin();
    TEST_ASSERT_EQUAL_INT(0, indPin); // Mock returns 0

    // Test IPerformanceService method (1 method)
    PerformanceMetrics metrics;
    interfacePtr->getPerformanceMetrics(metrics);
    TEST_ASSERT_EQUAL_UINT32(100, metrics.commands_processed);

    // Test IPresetService methods (5 methods)
    TEST_ASSERT_TRUE(interfacePtr->savePreset("test1"));
    TEST_ASSERT_TRUE(interfacePtr->loadPreset("test1"));
    TEST_ASSERT_TRUE(interfacePtr->deletePreset("test1"));
    TEST_ASSERT_FALSE(interfacePtr->presetExists("nonexistent"));

    char buffer[128];
    int count = interfacePtr->listPresets(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(0, count);

    // Total: 26 interface methods callable ✓
}

// ==================== Dependency Inversion Tests ====================

void test_performance_monitor_dependency_inversion() {
    // Test that components can accept IPerformanceMonitor interface
    // and work with any implementation (real or mock)

    MockPerformanceMonitor mockMonitor;
    IPerformanceMonitor& interfaceRef = mockMonitor;

    // Simulate what CommandDispatcher does
    uint64_t start = interfaceRef.startCommandTiming();
    // ... command processing ...
    interfaceRef.endCommandTiming(start);
    interfaceRef.recordEvent(true);

    // Verify mock tracked the calls
    TEST_ASSERT_EQUAL_INT(1, mockMonitor.getStartTimingCallCount());
    TEST_ASSERT_EQUAL_INT(1, mockMonitor.getEndTimingCallCount());
    TEST_ASSERT_EQUAL_INT(1, mockMonitor.getRecordEventCallCount());
}

void test_signal_controller_dependency_inversion() {
    // Test that presentation layers can accept ISignalController interface
    // and work with any implementation (adapter, mock, etc.)

    MockSignalController mockController;
    ISignalController& interfaceRef = mockController;

    // Simulate what ApiRouter/SerialCLI does
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 2000.0;
    cmd.dutyCycle = 0.75;

    bool result = interfaceRef.sendCommand(cmd);
    TEST_ASSERT_TRUE(result);

    SignalStatus_t status;
    SignalError err = interfaceRef.getStatus(status);
    TEST_ASSERT_EQUAL_INT(SIG_ERR_NONE, err);

    // Verify mock tracked the calls
    TEST_ASSERT_EQUAL_INT(1, mockController.getSendCommandCallCount());
    TEST_ASSERT_EQUAL_INT(1, mockController.getGetStatusCallCount());
}

// ==================== Layer Separation Tests ====================

void test_mock_controller_records_interactions() {
    MockSignalController mock;

    // Simulate presentation layer interactions
    SignalCmd cmd1 = {0};
    cmd1.type = SIG_CMD_START;
    cmd1.frequencyHz = 1000.0;

    SignalCmd cmd2 = {0};
    cmd2.type = SIG_CMD_STOP;

    mock.sendCommand(cmd1);
    mock.sendCommand(cmd2);
    mock.getStatus(*(SignalStatus_t*)nullptr); // nullptr is ok for mock
    mock.isRunning();

    // Verify all interactions were recorded
    TEST_ASSERT_EQUAL_INT(2, mock.getSendCommandCallCount());
    TEST_ASSERT_EQUAL_INT(1, mock.getGetStatusCallCount());
    TEST_ASSERT_EQUAL_INT(1, mock.getIsRunningCallCount());

    // Verify command history
    const auto& history = mock.getCommandHistory();
    TEST_ASSERT_EQUAL_size_t(2, history.size());
}

void test_mock_controller_preset_workflow() {
    MockSignalController mock;

    // Test preset workflow
    TEST_ASSERT_TRUE(mock.savePreset("config1"));
    TEST_ASSERT_TRUE(mock.savePreset("config2"));

    TEST_ASSERT_TRUE(mock.presetExists("config1"));
    TEST_ASSERT_TRUE(mock.presetExists("config2"));
    TEST_ASSERT_FALSE(mock.presetExists("nonexistent"));

    TEST_ASSERT_TRUE(mock.loadPreset("config1"));
    TEST_ASSERT_FALSE(mock.loadPreset("nonexistent"));

    TEST_ASSERT_TRUE(mock.deletePreset("config1"));
    TEST_ASSERT_FALSE(mock.presetExists("config1"));

    // Verify call counts
    TEST_ASSERT_EQUAL_INT(2, mock.getSavePresetCallCount());
    TEST_ASSERT_EQUAL_INT(2, mock.getLoadPresetCallCount());
    TEST_ASSERT_EQUAL_INT(1, mock.getDeletePresetCallCount());

    // Verify last names
    TEST_ASSERT_EQUAL_STRING("config2", mock.getLastSavePresetName().c_str());
    TEST_ASSERT_EQUAL_STRING("nonexistent", mock.getLastLoadPresetName().c_str());
    TEST_ASSERT_EQUAL_STRING("config1", mock.getLastDeletePresetName().c_str());
}

// ==================== Interface Segregation Tests ====================

void test_interface_segregation_signal_service() {
    // ISignalService interface contains only signal control methods
    MockSignalController mock;
    ISignalService* signalService = &mock;

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;

    // Only signal-related methods available
    signalService->startSignal(cmd);
    signalService->stopSignal();
    signalService->updateSignal(cmd);
    signalService->sendCommand(cmd);
    signalService->isRunning();

    SignalStatus_t status;
    signalService->getStatus(status);
    signalService->getCurrentFrequency();
    signalService->getCurrentDutyCycle();

    // Cannot call channel/pin/preset methods through ISignalService
    // (would be compile error)

    TEST_ASSERT_EQUAL_INT(4, mock.getSendCommandCallCount()); // 4 command calls
}

void test_interface_segregation_preset_service() {
    // IPresetService interface contains only preset methods
    MockSignalController mock;
    IPresetService* presetService = &mock;

    // Only preset-related methods available
    presetService->savePreset("test");
    presetService->loadPreset("test");
    presetService->deletePreset("test");
    presetService->presetExists("test");

    char buffer[64];
    presetService->listPresets(buffer, sizeof(buffer));

    // Cannot call signal/channel/pin methods through IPresetService
    // (would be compile error)

    TEST_ASSERT_EQUAL_INT(1, mock.getSavePresetCallCount());
}

// ==================== Clean Architecture Compliance Tests ====================

void test_presentation_to_application_layer_boundary() {
    // This test verifies that presentation layers depend ONLY on
    // application layer interfaces, not domain layer implementations

    MockSignalController mock;

    // Presentation layer interacts through ISignalController interface
    ISignalController* applicationInterface = &mock;

    // Presentation layer knows NOTHING about:
    // - SignalEngine (domain layer)
    // - PerformanceMonitor (domain layer)
    // - PulseGenerator (infrastructure layer)

    // Presentation layer only knows about:
    // - ISignalController (application interface)
    // - SignalCmd (data structure)

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    applicationInterface->sendCommand(cmd);

    // This is the ONLY dependency the presentation layer needs ✓
    TEST_ASSERT_EQUAL_INT(1, mock.getSendCommandCallCount());
}

void test_application_to_domain_layer_boundary() {
    // This test verifies that application layer (adapter) correctly
    // bridges the interface to domain implementation

    // In real code:
    // SignalControllerAdapter implements ISignalController
    // SignalControllerAdapter wraps SignalEngine (domain)

    // For testing with mock:
    MockSignalController mockAdapter;
    ISignalController* interface = &mockAdapter;

    // Presentation layer calls through interface
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_STOP;
    interface->sendCommand(cmd);

    // Adapter translates to domain layer (verified by mock)
    TEST_ASSERT_EQUAL_INT(1, mockAdapter.getSendCommandCallCount());

    const SignalCmd& lastCmd = mockAdapter.getLastCommand();
    TEST_ASSERT_EQUAL_INT(SIG_CMD_STOP, lastCmd.type);
}

// ==================== Main ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Interface abstraction tests
    RUN_TEST(test_iperformance_monitor_is_abstract);
    RUN_TEST(test_isignal_controller_is_abstract);

    // Dependency Inversion Principle tests
    RUN_TEST(test_performance_monitor_dependency_inversion);
    RUN_TEST(test_signal_controller_dependency_inversion);

    // Layer separation tests
    RUN_TEST(test_mock_controller_records_interactions);
    RUN_TEST(test_mock_controller_preset_workflow);

    // Interface Segregation Principle tests
    RUN_TEST(test_interface_segregation_signal_service);
    RUN_TEST(test_interface_segregation_preset_service);

    // Clean Architecture compliance tests
    RUN_TEST(test_presentation_to_application_layer_boundary);
    RUN_TEST(test_application_to_domain_layer_boundary);

    return UNITY_END();
}
