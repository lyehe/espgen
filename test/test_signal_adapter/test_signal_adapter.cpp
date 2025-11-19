#include <unity.h>

// Include mocks FIRST
#include "mocks/mocks.h"

// Include headers
#include "SignalControllerAdapter.h"
#include "SignalEngine.h"
#include "ISignalController.h"

// Include mock implementations
#include "../mocks/mocks.cpp"

// Include all required source files
#include "../../lib/SignalEngine/src/SignalState.cpp"
#include "../../lib/SignalEngine/src/SignalPersistence.cpp"
#include "../../lib/SignalEngine/src/TimingController.cpp"
#include "../../lib/SignalEngine/src/SignalEventPublisher.cpp"
#include "../../lib/SignalEngine/src/PerformanceMonitor.cpp"
#include "../../lib/SignalEngine/src/CommandDispatcher.cpp"
#include "../../lib/SignalEngine/src/SignalEngine.cpp"

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

MockPulseGenerator* mockPulseGen = nullptr;
SignalEngine* engine = nullptr;
SignalControllerAdapter* adapter = nullptr;

void setUp(void) {
    g_mockTimeMicros = 0;

    // Create mock pulse generator
    mockPulseGen = new MockPulseGenerator();

    // Create real SignalEngine with mock hardware
    engine = new SignalEngine(*mockPulseGen);
    engine->begin();

    // Create adapter wrapping engine
    adapter = new SignalControllerAdapter(*engine);
}

void tearDown(void) {
    delete adapter;
    delete engine;
    delete mockPulseGen;
}

// ==================== Adapter Pattern Tests ====================

void test_adapter_implements_isignal_controller() {
    // Verify adapter implements ISignalController interface
    ISignalController* interface = adapter;
    TEST_ASSERT_NOT_NULL(interface);
}

void test_adapter_wraps_signal_engine() {
    // Verify adapter correctly wraps SignalEngine
    // by testing delegation of a simple method

    int pin = adapter->getOutputPin();

    // Should return default pin value from engine
    TEST_ASSERT_TRUE(pin >= 0);
}

// ==================== ISignalService Tests ====================

void test_send_command_delegates_to_engine() {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;

    bool result = adapter->sendCommand(cmd);

    // Should succeed
    TEST_ASSERT_TRUE(result);

    // Verify engine state changed
    TEST_ASSERT_TRUE(engine->isRunning());
}

void test_start_signal_creates_start_command() {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 2000.0;
    cmd.dutyCycle = 0.75;

    bool result = adapter->startSignal(cmd);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(engine->isRunning());
    TEST_ASSERT_EQUAL_DOUBLE(2000.0, engine->getCurrentFrequencyHz());
}

void test_stop_signal_creates_stop_command() {
    // Start first
    SignalCmd startCmd = {0};
    startCmd.type = SIG_CMD_START;
    startCmd.frequencyHz = 1000.0;
    startCmd.dutyCycle = 0.5;
    adapter->startSignal(startCmd);

    TEST_ASSERT_TRUE(engine->isRunning());

    // Now stop
    bool result = adapter->stopSignal();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(engine->isRunning());
}

void test_update_signal_delegates_correctly() {
    // Start signal first
    SignalCmd startCmd = {0};
    startCmd.type = SIG_CMD_START;
    startCmd.frequencyHz = 1000.0;
    startCmd.dutyCycle = 0.5;
    adapter->startSignal(startCmd);

    // Update with new parameters
    SignalCmd updateCmd = {0};
    updateCmd.type = SIG_CMD_UPDATE_ALL;
    updateCmd.frequencyHz = 5000.0;
    updateCmd.dutyCycle = 0.25;

    bool result = adapter->updateSignal(updateCmd);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_DOUBLE(5000.0, engine->getCurrentFrequencyHz());
}

void test_is_running_reflects_engine_state() {
    // Initially stopped
    TEST_ASSERT_FALSE(adapter->isRunning());
    TEST_ASSERT_FALSE(engine->isRunning());

    // Start signal
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;
    adapter->sendCommand(cmd);

    // Now running
    TEST_ASSERT_TRUE(adapter->isRunning());
    TEST_ASSERT_TRUE(engine->isRunning());

    // Stop signal
    adapter->stopSignal();

    // Stopped again
    TEST_ASSERT_FALSE(adapter->isRunning());
    TEST_ASSERT_FALSE(engine->isRunning());
}

void test_get_status_delegates_to_engine() {
    SignalStatus_t status;
    SignalError err = adapter->getStatus(status);

    TEST_ASSERT_EQUAL_INT(SIG_ERR_NONE, err);
    TEST_ASSERT_FALSE(status.running);
}

void test_get_current_frequency_delegates() {
    // Start with known frequency
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 3333.0;
    cmd.dutyCycle = 0.5;
    adapter->sendCommand(cmd);

    double freq = adapter->getCurrentFrequency();
    TEST_ASSERT_EQUAL_DOUBLE(3333.0, freq);
}

void test_get_current_duty_cycle_delegates() {
    // Start with known duty cycle
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.123;
    adapter->sendCommand(cmd);

    float duty = adapter->getCurrentDutyCycle();
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.123, duty);
}

// ==================== IChannelService Tests ====================

void test_configure_channel_sends_command() {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_CONFIG_CHANNEL;
    cmd.channel = 1;
    cmd.pin = 19;
    cmd.phaseOffset = 90.0;

    bool result = adapter->configureChannel(1, cmd);
    TEST_ASSERT_TRUE(result);
}

void test_enable_channel_creates_command() {
    bool result = adapter->enableChannel(2, true);
    TEST_ASSERT_TRUE(result);

    result = adapter->enableChannel(2, false);
    TEST_ASSERT_TRUE(result);
}

void test_set_channel_pin_creates_command() {
    bool result = adapter->setChannelPin(1, 19);
    TEST_ASSERT_TRUE(result);
}

void test_set_channel_phase_creates_command() {
    bool result = adapter->setChannelPhase(1, 180.0);
    TEST_ASSERT_TRUE(result);
}

void test_get_channel_pin_delegates() {
    // Master channel (0)
    int pin = adapter->getChannelPin(0);
    TEST_ASSERT_TRUE(pin >= 0);

    // Slave channel (returns -1 if not configured or pin from config)
    pin = adapter->getChannelPin(1);
    TEST_ASSERT_TRUE(pin != 0); // Should return some value
}

void test_is_channel_enabled_master_always_enabled() {
    // Master channel (0) is always enabled
    bool enabled = adapter->isChannelEnabled(0);
    TEST_ASSERT_TRUE(enabled);
}

void test_get_channel_phase_offset_delegates() {
    float phase = adapter->getChannelPhaseOffset(0);
    TEST_ASSERT_EQUAL_FLOAT(0.0, phase); // Master has no offset
}

void test_get_channel_polarity_delegates() {
    SignalPolarity polarity = adapter->getChannelPolarity(0);
    TEST_ASSERT_EQUAL_INT(POLARITY_ACTIVE_HIGH, polarity);
}

void test_get_channel_config_delegates() {
    PulseChannelConfig_t config;
    bool result = adapter->getChannelConfig(0, config);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(config.enabled); // Master is enabled
}

void test_trigger_sync_sends_command() {
    bool result = adapter->triggerSync();
    TEST_ASSERT_TRUE(result);

    // Verify pulse generator received sync
    TEST_ASSERT_TRUE(mockPulseGen->wasSyncCalled());
}

// ==================== IPinService Tests ====================

void test_set_output_pin_sends_command() {
    bool result = adapter->setOutputPin(25);
    TEST_ASSERT_TRUE(result);

    // Verify engine updated pin
    int pin = engine->getOutputPin();
    TEST_ASSERT_EQUAL_INT(25, pin);
}

void test_get_output_pin_delegates() {
    // Set pin first
    adapter->setOutputPin(18);

    // Get should return same value
    int pin = adapter->getOutputPin();
    TEST_ASSERT_EQUAL_INT(18, pin);
}

void test_set_indicator_pin_sends_command() {
    bool result = adapter->setIndicatorPin(2);
    TEST_ASSERT_TRUE(result);

    // Verify engine updated pin
    uint8_t pin = engine->getIndicatorPin();
    TEST_ASSERT_EQUAL_INT(2, pin);
}

void test_get_indicator_pin_delegates() {
    // Set indicator first
    adapter->setIndicatorPin(5);

    // Get should return same value
    int pin = adapter->getIndicatorPin();
    TEST_ASSERT_EQUAL_INT(5, pin);
}

// ==================== IPerformanceService Tests ====================

void test_get_performance_metrics_delegates() {
    PerformanceMetrics metrics;
    adapter->getPerformanceMetrics(metrics);

    // Should get metrics from engine
    // Initial values should be zero
    TEST_ASSERT_EQUAL_UINT32(0, metrics.commands_processed);
}

// ==================== IPresetService Tests ====================

void test_save_preset_delegates() {
    // Start a signal first so we have something to save
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1234.0;
    cmd.dutyCycle = 0.567;
    adapter->sendCommand(cmd);

    // Save preset
    bool result = adapter->savePreset("test_preset");
    TEST_ASSERT_TRUE(result);

    // Verify preset exists
    TEST_ASSERT_TRUE(engine->presetExists("test_preset"));
}

void test_load_preset_delegates() {
    // Save a preset first
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 9999.0;
    cmd.dutyCycle = 0.11;
    adapter->sendCommand(cmd);
    adapter->savePreset("load_test");

    // Change settings
    adapter->stopSignal();

    // Load preset
    bool result = adapter->loadPreset("load_test");
    TEST_ASSERT_TRUE(result);

    // Should have started signal with saved settings
    TEST_ASSERT_TRUE(engine->isRunning());
}

void test_delete_preset_delegates() {
    // Save preset
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;
    adapter->sendCommand(cmd);
    adapter->savePreset("delete_test");

    TEST_ASSERT_TRUE(engine->presetExists("delete_test"));

    // Delete it
    bool result = adapter->deletePreset("delete_test");
    TEST_ASSERT_TRUE(result);

    // Should no longer exist
    TEST_ASSERT_FALSE(engine->presetExists("delete_test"));
}

void test_preset_exists_delegates() {
    // Nonexistent preset
    TEST_ASSERT_FALSE(adapter->presetExists("nonexistent"));

    // Create preset
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;
    adapter->sendCommand(cmd);
    adapter->savePreset("exists_test");

    // Should now exist
    TEST_ASSERT_TRUE(adapter->presetExists("exists_test"));
}

void test_list_presets_delegates() {
    // Create a few presets
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000.0;
    cmd.dutyCycle = 0.5;
    adapter->sendCommand(cmd);

    adapter->savePreset("preset1");
    adapter->savePreset("preset2");
    adapter->savePreset("preset3");

    // List them
    char buffer[256];
    int count = adapter->listPresets(buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_INT(3, count);
    TEST_ASSERT_TRUE(strlen(buffer) > 0);
}

// ==================== Interface Compliance Tests ====================

void test_adapter_provides_all_26_isignal_controller_methods() {
    // This test verifies all 26 methods are callable through interface

    ISignalController* interface = adapter;

    // ISignalService (8 methods)
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    interface->startSignal(cmd);
    interface->stopSignal();
    interface->updateSignal(cmd);
    interface->sendCommand(cmd);
    interface->isRunning();
    SignalStatus_t status;
    interface->getStatus(status);
    interface->getCurrentFrequency();
    interface->getCurrentDutyCycle();

    // IChannelService (8 methods)
    interface->configureChannel(1, cmd);
    interface->enableChannel(1, true);
    interface->setChannelPin(1, 19);
    interface->setChannelPhase(1, 90.0);
    interface->getChannelPin(1);
    interface->isChannelEnabled(1);
    interface->getChannelPhaseOffset(1);
    interface->getChannelPolarity(1);
    PulseChannelConfig_t config;
    interface->getChannelConfig(1, config);
    interface->triggerSync();

    // IPinService (4 methods)
    interface->setOutputPin(18);
    interface->getOutputPin();
    interface->setIndicatorPin(2);
    interface->getIndicatorPin();

    // IPerformanceService (1 method)
    PerformanceMetrics metrics;
    interface->getPerformanceMetrics(metrics);

    // IPresetService (5 methods)
    interface->savePreset("test");
    interface->loadPreset("test");
    interface->deletePreset("test");
    interface->presetExists("test");
    char buffer[64];
    interface->listPresets(buffer, sizeof(buffer));

    // All 26 methods callable ✓
    TEST_ASSERT_TRUE(true);
}

// ==================== Clean Architecture Validation ====================

void test_adapter_bridges_application_to_domain_layer() {
    // Adapter is the bridge between:
    // - Application layer (ISignalController interface)
    // - Domain layer (SignalEngine implementation)

    // Application layer depends on interface
    ISignalController* applicationInterface = adapter;

    // Domain layer is hidden behind adapter
    // Presentation layer cannot access SignalEngine directly

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 7777.0;
    cmd.dutyCycle = 0.88;

    // Call through application interface
    applicationInterface->sendCommand(cmd);

    // Verify domain layer (engine) received the command
    TEST_ASSERT_TRUE(engine->isRunning());
    TEST_ASSERT_EQUAL_DOUBLE(7777.0, engine->getCurrentFrequencyHz());
}

void test_multiple_adapters_can_wrap_same_engine() {
    // Multiple adapters can wrap the same engine
    // (useful for different presentation layers)

    SignalControllerAdapter adapter1(*engine);
    SignalControllerAdapter adapter2(*engine);

    // Both adapters control the same engine
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1111.0;
    cmd.dutyCycle = 0.5;

    adapter1.sendCommand(cmd);

    // Engine state changed
    TEST_ASSERT_TRUE(engine->isRunning());

    // Adapter2 sees same state
    TEST_ASSERT_TRUE(adapter2.isRunning());
    TEST_ASSERT_EQUAL_DOUBLE(1111.0, adapter2.getCurrentFrequency());
}

// ==================== Main ====================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Adapter pattern tests
    RUN_TEST(test_adapter_implements_isignal_controller);
    RUN_TEST(test_adapter_wraps_signal_engine);

    // ISignalService tests (8 methods)
    RUN_TEST(test_send_command_delegates_to_engine);
    RUN_TEST(test_start_signal_creates_start_command);
    RUN_TEST(test_stop_signal_creates_stop_command);
    RUN_TEST(test_update_signal_delegates_correctly);
    RUN_TEST(test_is_running_reflects_engine_state);
    RUN_TEST(test_get_status_delegates_to_engine);
    RUN_TEST(test_get_current_frequency_delegates);
    RUN_TEST(test_get_current_duty_cycle_delegates);

    // IChannelService tests (10 methods)
    RUN_TEST(test_configure_channel_sends_command);
    RUN_TEST(test_enable_channel_creates_command);
    RUN_TEST(test_set_channel_pin_creates_command);
    RUN_TEST(test_set_channel_phase_creates_command);
    RUN_TEST(test_get_channel_pin_delegates);
    RUN_TEST(test_is_channel_enabled_master_always_enabled);
    RUN_TEST(test_get_channel_phase_offset_delegates);
    RUN_TEST(test_get_channel_polarity_delegates);
    RUN_TEST(test_get_channel_config_delegates);
    RUN_TEST(test_trigger_sync_sends_command);

    // IPinService tests (4 methods)
    RUN_TEST(test_set_output_pin_sends_command);
    RUN_TEST(test_get_output_pin_delegates);
    RUN_TEST(test_set_indicator_pin_sends_command);
    RUN_TEST(test_get_indicator_pin_delegates);

    // IPerformanceService tests (1 method)
    RUN_TEST(test_get_performance_metrics_delegates);

    // IPresetService tests (5 methods)
    RUN_TEST(test_save_preset_delegates);
    RUN_TEST(test_load_preset_delegates);
    RUN_TEST(test_delete_preset_delegates);
    RUN_TEST(test_preset_exists_delegates);
    RUN_TEST(test_list_presets_delegates);

    // Interface compliance tests
    RUN_TEST(test_adapter_provides_all_26_isignal_controller_methods);

    // Clean Architecture validation
    RUN_TEST(test_adapter_bridges_application_to_domain_layer);
    RUN_TEST(test_multiple_adapters_can_wrap_same_engine);

    return UNITY_END();
}
