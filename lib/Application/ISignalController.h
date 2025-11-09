// Application layer service interfaces
// These define the contract between presentation and domain layers
#pragma once

#include "signal_iface.h"

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
 * @brief Complete application facade
 *
 * Combines all service interfaces into a single entry point
 * for presentation layers. This follows the Facade pattern.
 */
class ISignalController : public ISignalService,
                          public IChannelService,
                          public IPinService {
public:
    virtual ~ISignalController() = default;
};
