// Application layer adapter
// Adapts the domain SignalEngine to the application service interface
#pragma once

#include "ISignalController.h"
#include "SignalEngine.h"

/**
 * @brief Adapter for SignalEngine
 *
 * Implements the Adapter pattern to wrap the existing SignalEngine
 * and expose it through clean service interfaces. This allows us to
 * maintain backward compatibility while introducing clean architecture.
 *
 * Benefits:
 * - Presentation layers depend on interfaces, not concrete classes
 * - Can swap implementations for testing
 * - Single Responsibility: Adapter only handles interface translation
 */
class SignalControllerAdapter : public ISignalController {
public:
    explicit SignalControllerAdapter(SignalEngine& engine)
        : _engine(engine) {}

    // ISignalService implementation
    bool startSignal(const SignalCmd& cmd) override {
        return _engine.sendCommand(cmd);
    }

    bool stopSignal() override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_STOP;
        return _engine.sendCommand(cmd);
    }

    bool updateSignal(const SignalCmd& cmd) override {
        return _engine.sendCommand(cmd);
    }

    bool sendCommand(const SignalCmd& cmd) override {
        return _engine.sendCommand(cmd);
    }

    bool isRunning() const override {
        return _engine.isRunning();
    }

    SignalError getStatus(SignalStatus_t& status) const override {
        return _engine.getCurrentStatus(status);
    }

    double getCurrentFrequency() const override {
        return _engine.getCurrentFrequencyHz();
    }

    float getCurrentDutyCycle() const override {
        return _engine.getCurrentDutyCycle();
    }

    // IChannelService implementation
    bool configureChannel(uint8_t channel, const SignalCmd& cmd) override {
        return _engine.sendCommand(cmd);
    }

    bool enableChannel(uint8_t channel, bool enabled) override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_ENABLE_CHANNEL;
        cmd.channel = channel;
        cmd.enabled = enabled;
        return _engine.sendCommand(cmd);
    }

    bool setChannelPin(uint8_t channel, uint8_t pin) override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_SET_PIN;
        cmd.channel = channel;
        cmd.pin = pin;
        return _engine.sendCommand(cmd);
    }

    bool setChannelPhase(uint8_t channel, float phase_deg) override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_CONFIG_CHANNEL;
        cmd.channel = channel;
        cmd.phaseOffset = phase_deg;
        cmd.paramMode = PARAM_USE_PHASE_DEGREES;
        return _engine.sendCommand(cmd);
    }

    int getChannelPin(uint8_t channel) const override {
        if (channel == 0) {
            return _engine.getOutputPin();
        }
        PulseChannelConfig_t config;
        if (_engine.getChannelConfig(channel, config)) {
            return config.gpio_pin;
        }
        return -1; // Invalid channel
    }

    bool isChannelEnabled(uint8_t channel) const override {
        if (channel == 0) return true; // Master always enabled
        PulseChannelConfig_t config;
        if (_engine.getChannelConfig(channel, config)) {
            return config.enabled;
        }
        return false; // Invalid channel or disabled
    }

    float getChannelPhaseOffset(uint8_t channel) const override {
        return _engine.getChannelPhaseOffset(channel);
    }

    SignalPolarity getChannelPolarity(uint8_t channel) const override {
        return _engine.getChannelPolarity(channel);
    }

    bool getChannelConfig(uint8_t channel, PulseChannelConfig_t& config) const override {
        return _engine.getChannelConfig(channel, config);
    }

    bool triggerSync() override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_SYNC;
        return _engine.sendCommand(cmd);
    }

    // IPinService implementation
    bool setOutputPin(uint8_t pin) override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_SET_PIN;
        cmd.pin = pin;
        return _engine.sendCommand(cmd);
    }

    int getOutputPin() const override {
        return _engine.getOutputPin();
    }

    bool setIndicatorPin(uint8_t pin) override {
        SignalCmd cmd = {};
        cmd.type = SIG_CMD_SET_INDICATOR;
        cmd.pin = pin;
        return _engine.sendCommand(cmd);
    }

    int getIndicatorPin() const override {
        return _engine.getIndicatorPin();
    }

    // IPerformanceService implementation
    void getPerformanceMetrics(PerformanceMetrics& metrics) override {
        _engine.getPerformanceMetrics(metrics);
    }

    // IPresetService implementation
    bool savePreset(const char* name) override {
        return _engine.savePreset(name);
    }

    bool loadPreset(const char* name) override {
        return _engine.loadPreset(name);
    }

    bool deletePreset(const char* name) override {
        return _engine.deletePreset(name);
    }

    bool presetExists(const char* name) override {
        return _engine.presetExists(name);
    }

    int listPresets(char* buffer, size_t bufferSize) override {
        return _engine.listPresets(buffer, bufferSize);
    }

private:
    SignalEngine& _engine; // Reference to existing domain object
};
