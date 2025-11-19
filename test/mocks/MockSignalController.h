#ifndef MOCK_SIGNAL_CONTROLLER_H
#define MOCK_SIGNAL_CONTROLLER_H

#include "ISignalController.h"
#include <vector>
#include <string>

/**
 * @brief Mock implementation of ISignalController for testing presentation layers
 *
 * This mock allows testing of presentation components (ApiRouter, SerialCLI, etc.)
 * without needing the full SignalEngine implementation.
 *
 * Features:
 * - Records all method calls with parameters
 * - Returns predictable test values
 * - Allows assertion of interactions between layers
 * - Verifies Clean Architecture layer separation
 */
class MockSignalController : public ISignalController {
public:
    MockSignalController() :
        _sendCommandCallCount(0),
        _getStatusCallCount(0),
        _isRunningCallCount(0),
        _getOutputPinCallCount(0),
        _getPerformanceMetricsCallCount(0),
        _savePresetCallCount(0),
        _loadPresetCallCount(0),
        _deletePresetCallCount(0),
        _isRunning(false),
        _outputPin(18),
        _lastSavePresetName(""),
        _lastLoadPresetName(""),
        _lastDeletePresetName("")
    {}

    // ==================== ISignalService Implementation ====================

    bool startSignal(const SignalCmd& cmd) override {
        recordCommand("startSignal", cmd);
        return true;
    }

    bool stopSignal() override {
        _commandHistory.push_back("stopSignal");
        return true;
    }

    bool updateSignal(const SignalCmd& cmd) override {
        recordCommand("updateSignal", cmd);
        return true;
    }

    bool sendCommand(const SignalCmd& cmd) override {
        _sendCommandCallCount++;
        _lastCommand = cmd;
        recordCommand("sendCommand", cmd);
        return true;
    }

    bool isRunning() const override {
        _isRunningCallCount++;
        return _isRunning;
    }

    SignalError getStatus(SignalStatus_t& status) const override {
        _getStatusCallCount++;
        status.running = _isRunning;
        status.frequency = 1000.0;
        status.duty_cycle = 0.5;
        status.period_us = 1000;
        status.pulse_width_us = 500;
        status.polarity = POLARITY_ACTIVE_HIGH;
        status.estimated_cycles = 12345;
        status.uptime_ms = 60000;
        return SIG_ERR_NONE;
    }

    double getCurrentFrequency() const override {
        return 1000.0;
    }

    float getCurrentDutyCycle() const override {
        return 0.5;
    }

    // ==================== IChannelService Implementation ====================

    bool configureChannel(uint8_t channel, const SignalCmd& cmd) override {
        _channelConfigHistory.push_back(channel);
        return true;
    }

    bool enableChannel(uint8_t channel, bool enabled) override {
        _channelEnableHistory.push_back({channel, enabled});
        return true;
    }

    bool setChannelPin(uint8_t channel, uint8_t pin) override {
        return true;
    }

    bool setChannelPhase(uint8_t channel, float phase_deg) override {
        return true;
    }

    int getChannelPin(uint8_t channel) const override {
        return channel == 0 ? _outputPin : (18 + channel);
    }

    bool isChannelEnabled(uint8_t channel) const override {
        return channel == 0 ? true : false;
    }

    float getChannelPhaseOffset(uint8_t channel) const override {
        return 0.0;
    }

    SignalPolarity getChannelPolarity(uint8_t channel) const override {
        return POLARITY_ACTIVE_HIGH;
    }

    bool getChannelConfig(uint8_t channel, PulseChannelConfig_t& config) const override {
        config.gpio_pin = getChannelPin(channel);
        config.phase_offset_deg = 0.0;
        config.polarity = POLARITY_ACTIVE_HIGH;
        config.enabled = (channel == 0);
        config.skip_count = 0;
        return true;
    }

    bool triggerSync() override {
        _syncCallCount++;
        return true;
    }

    // ==================== IPinService Implementation ====================

    bool setOutputPin(uint8_t pin) override {
        _outputPin = pin;
        return true;
    }

    int getOutputPin() const override {
        _getOutputPinCallCount++;
        return _outputPin;
    }

    bool setIndicatorPin(uint8_t pin) override {
        _indicatorPin = pin;
        return true;
    }

    int getIndicatorPin() const override {
        return _indicatorPin;
    }

    // ==================== IPerformanceService Implementation ====================

    void getPerformanceMetrics(PerformanceMetrics& metrics) override {
        _getPerformanceMetricsCallCount++;
        metrics.commands_processed = 100;
        metrics.avg_latency_us = 200;
        metrics.min_latency_us = 50;
        metrics.max_latency_us = 800;
        metrics.events_published = 50;
        metrics.events_failed = 2;
        metrics.free_heap = 220000;
        metrics.min_free_heap = 190000;
        metrics.uptime_ms = 120000;
    }

    // ==================== IPresetService Implementation ====================

    bool savePreset(const char* name) override {
        _savePresetCallCount++;
        _lastSavePresetName = name;
        _savedPresets.push_back(name);
        return true;
    }

    bool loadPreset(const char* name) override {
        _loadPresetCallCount++;
        _lastLoadPresetName = name;
        // Simulate success if preset exists
        for (const auto& preset : _savedPresets) {
            if (preset == name) {
                return true;
            }
        }
        return false;
    }

    bool deletePreset(const char* name) override {
        _deletePresetCallCount++;
        _lastDeletePresetName = name;
        // Remove from list
        for (auto it = _savedPresets.begin(); it != _savedPresets.end(); ++it) {
            if (*it == name) {
                _savedPresets.erase(it);
                return true;
            }
        }
        return false;
    }

    bool presetExists(const char* name) override {
        for (const auto& preset : _savedPresets) {
            if (preset == name) {
                return true;
            }
        }
        return false;
    }

    int listPresets(char* buffer, size_t bufferSize) override {
        int count = 0;
        buffer[0] = '\0';
        for (const auto& preset : _savedPresets) {
            if (strlen(buffer) + preset.length() + 2 < bufferSize) {
                if (count > 0) strcat(buffer, ",");
                strcat(buffer, preset.c_str());
                count++;
            }
        }
        return count;
    }

    // ==================== Test Helper Methods ====================

    // Call count getters
    int getSendCommandCallCount() const { return _sendCommandCallCount; }
    int getGetStatusCallCount() const { return _getStatusCallCount; }
    int getIsRunningCallCount() const { return _isRunningCallCount; }
    int getGetOutputPinCallCount() const { return _getOutputPinCallCount; }
    int getGetPerformanceMetricsCallCount() const { return _getPerformanceMetricsCallCount; }
    int getSavePresetCallCount() const { return _savePresetCallCount; }
    int getLoadPresetCallCount() const { return _loadPresetCallCount; }
    int getDeletePresetCallCount() const { return _deletePresetCallCount; }
    int getSyncCallCount() const { return _syncCallCount; }

    // Last command access
    const SignalCmd& getLastCommand() const { return _lastCommand; }

    // Command history
    const std::vector<std::string>& getCommandHistory() const { return _commandHistory; }

    // Preset history
    const std::string& getLastSavePresetName() const { return _lastSavePresetName; }
    const std::string& getLastLoadPresetName() const { return _lastLoadPresetName; }
    const std::string& getLastDeletePresetName() const { return _lastDeletePresetName; }

    // State setters for testing
    void setIsRunning(bool running) { _isRunning = running; }

    // Clear history
    void clearCallHistory() {
        _sendCommandCallCount = 0;
        _getStatusCallCount = 0;
        _isRunningCallCount = 0;
        _getOutputPinCallCount = 0;
        _getPerformanceMetricsCallCount = 0;
        _savePresetCallCount = 0;
        _loadPresetCallCount = 0;
        _deletePresetCallCount = 0;
        _syncCallCount = 0;
        _commandHistory.clear();
        _channelConfigHistory.clear();
        _channelEnableHistory.clear();
    }

private:
    // Call counters
    mutable int _sendCommandCallCount;
    mutable int _getStatusCallCount;
    mutable int _isRunningCallCount;
    mutable int _getOutputPinCallCount;
    int _getPerformanceMetricsCallCount;
    int _savePresetCallCount;
    int _loadPresetCallCount;
    int _deletePresetCallCount;
    int _syncCallCount = 0;

    // Recorded values
    SignalCmd _lastCommand;
    std::vector<std::string> _commandHistory;
    std::vector<uint8_t> _channelConfigHistory;
    std::vector<std::pair<uint8_t, bool>> _channelEnableHistory;

    // Simulated state
    bool _isRunning;
    int _outputPin;
    int _indicatorPin = 0;

    // Preset tracking
    std::string _lastSavePresetName;
    std::string _lastLoadPresetName;
    std::string _lastDeletePresetName;
    std::vector<std::string> _savedPresets;

    // Helper to record commands
    void recordCommand(const char* name, const SignalCmd& cmd) {
        std::string entry = std::string(name);
        if (cmd.type == SIG_CMD_START) entry += "(START)";
        else if (cmd.type == SIG_CMD_STOP) entry += "(STOP)";
        else if (cmd.type == SIG_CMD_UPDATE_ALL) entry += "(UPDATE)";
        _commandHistory.push_back(entry);
    }
};

#endif // MOCK_SIGNAL_CONTROLLER_H
