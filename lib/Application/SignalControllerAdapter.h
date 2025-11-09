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
        SignalCmd cmd = {0};
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
        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_ENABLE_CHANNEL;
        cmd.channel = channel;
        cmd.enabled = enabled;
        return _engine.sendCommand(cmd);
    }

    bool setChannelPin(uint8_t channel, uint8_t pin) override {
        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_SET_PIN;
        cmd.channel = channel;
        cmd.pin = pin;
        return _engine.sendCommand(cmd);
    }

    bool setChannelPhase(uint8_t channel, float phase_deg) override {
        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_CONFIG_CHANNEL;
        cmd.channel = channel;
        cmd.phaseOffset = phase_deg;
        cmd.paramMode = PARAM_USE_PHASE_DEGREES;
        return _engine.sendCommand(cmd);
    }

    int getChannelPin(uint8_t channel) const override {
        // Note: Currently only master channel pin is available
        if (channel == 0) {
            return _engine.getOutputPin();
        }
        return -1; // Not available without getters in PulseGenerator
    }

    bool isChannelEnabled(uint8_t channel) const override {
        // Note: Would need getters in PulseGenerator
        if (channel == 0) return true; // Master always enabled
        return false; // Unknown without getters
    }

    bool triggerSync() override {
        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_SYNC;
        return _engine.sendCommand(cmd);
    }

    // IPinService implementation
    bool setOutputPin(uint8_t pin) override {
        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_SET_PIN;
        cmd.pin = pin;
        return _engine.sendCommand(cmd);
    }

    int getOutputPin() const override {
        return _engine.getOutputPin();
    }

    bool setIndicatorPin(uint8_t pin) override {
        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_SET_INDICATOR;
        cmd.pin = pin;
        return _engine.sendCommand(cmd);
    }

    int getIndicatorPin() const override {
        // Note: Would need getter in PulseGenerator
        return -1; // Not available
    }

private:
    SignalEngine& _engine; // Reference to existing domain object
};
