// Application layer service interfaces
// These define the contract between presentation and domain layers
#pragma once

#include "signal_iface.h"
#include "IPulseGenerator.h"  // For PulseChannelConfig_t definition

// Forward declarations
struct PerformanceMetrics;
struct SignalSettings;

/**
 * @brief Signal control service interface
 *
 * Defines operations for controlling signal generation.
 * Implements the Interface Segregation Principle by grouping
 * related operations.
 */
class ISignalService {
public:
    virtual ~ISignalService() = default;

    // Command operations
    virtual bool startSignal(const SignalCmd& cmd) = 0;
    virtual bool stopSignal() = 0;
    virtual bool updateSignal(const SignalCmd& cmd) = 0;
    virtual bool sendCommand(const SignalCmd& cmd) = 0;

    // Status queries
    virtual bool isRunning() const = 0;
    virtual SignalError getStatus(SignalStatus_t& status) const = 0;
    virtual double getCurrentFrequency() const = 0;
    virtual float getCurrentDutyCycle() const = 0;
};

/**
 * @brief Channel configuration service interface
 *
 * Handles multi-channel configuration and management.
 */
class IChannelService {
public:
    virtual ~IChannelService() = default;

    // Channel configuration
    virtual bool configureChannel(uint8_t channel, const SignalCmd& cmd) = 0;
    virtual bool enableChannel(uint8_t channel, bool enabled) = 0;
    virtual bool setChannelPin(uint8_t channel, uint8_t pin) = 0;
    virtual bool setChannelPhase(uint8_t channel, float phase_deg) = 0;

    // Channel queries
    virtual int getChannelPin(uint8_t channel) const = 0;
    virtual bool isChannelEnabled(uint8_t channel) const = 0;
    virtual float getChannelPhaseOffset(uint8_t channel) const = 0;
    virtual SignalPolarity getChannelPolarity(uint8_t channel) const = 0;
    virtual bool getChannelConfig(uint8_t channel, PulseChannelConfig_t& config) const = 0;
    virtual bool triggerSync() = 0;
};

/**
 * @brief Pin configuration service interface
 *
 * Manages GPIO pin assignments.
 */
class IPinService {
public:
    virtual ~IPinService() = default;

    virtual bool setOutputPin(uint8_t pin) = 0;
    virtual int getOutputPin() const = 0;
    virtual bool setIndicatorPin(uint8_t pin) = 0;
    virtual int getIndicatorPin() const = 0;
};

/**
 * @brief Performance monitoring service interface
 *
 * Provides access to system performance metrics.
 */
class IPerformanceService {
public:
    virtual ~IPerformanceService() = default;

    /**
     * @brief Get current performance metrics
     * @param metrics Output parameter to receive metrics
     */
    virtual void getPerformanceMetrics(PerformanceMetrics& metrics) = 0;
};

/**
 * @brief Preset configuration service interface
 *
 * Manages saving and loading of signal configuration presets.
 */
class IPresetService {
public:
    virtual ~IPresetService() = default;

    /**
     * @brief Save current configuration as a preset
     * @param name Preset name (max 15 chars)
     * @return true if saved successfully
     */
    virtual bool savePreset(const char* name) = 0;

    /**
     * @brief Load a preset configuration
     * @param name Preset name to load
     * @return true if loaded and applied successfully
     */
    virtual bool loadPreset(const char* name) = 0;

    /**
     * @brief Delete a preset
     * @param name Preset name to delete
     * @return true if deleted successfully
     */
    virtual bool deletePreset(const char* name) = 0;

    /**
     * @brief Check if a preset exists
     * @param name Preset name to check
     * @return true if preset exists
     */
    virtual bool presetExists(const char* name) = 0;

    /**
     * @brief List all saved presets
     * @param buffer Output buffer for comma-separated preset names
     * @param bufferSize Size of output buffer
     * @return Number of presets found
     */
    virtual int listPresets(char* buffer, size_t bufferSize) = 0;
};

/**
 * @brief Complete application facade
 *
 * Combines all service interfaces into a single entry point
 * for presentation layers. This follows the Facade pattern.
 */
class ISignalController : public ISignalService,
                          public IChannelService,
                          public IPinService,
                          public IPerformanceService,
                          public IPresetService {
public:
    virtual ~ISignalController() = default;
};
