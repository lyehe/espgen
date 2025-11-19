#ifndef I_PULSE_GENERATOR_H
#define I_PULSE_GENERATOR_H

#include <stdint.h>
#include "signal_iface.h"  // For PulseParams_t and helper functions

/**
 * @brief Multi-channel pulse generator configuration structure
 */
typedef struct {
    uint8_t gpio_pin;           // GPIO pin for this channel
    float phase_offset_deg;     // Phase offset in degrees (0-360) relative to master
    bool enabled;               // Enable/disable (master channel is always enabled)
    uint8_t skip_count;         // Skip N pulses (0 = no skip, future feature)
} PulseChannelConfig_t;

/**
 * @brief Interface for multi-channel pulse generator
 *
 * This interface abstracts the pulse generation hardware, allowing for:
 * - Unit testing with mock implementations
 * - Hardware abstraction
 * - Dependency Inversion Principle compliance
 *
 * Implementations may use ESP32 MCPWM, LEDC, or software-based generation.
 */
class IPulseGenerator {
public:
    virtual ~IPulseGenerator() = default;

    /**
     * @brief Initialize the pulse generator
     * @return true on success, false on failure
     */
    virtual bool begin() = 0;

    // ========== Channel Configuration ==========

    /**
     * @brief Configure a specific channel
     * @param channel_id Channel number (0-5, where 0 is master)
     * @param config Channel configuration
     * @return true on success, false on failure
     */
    virtual bool configureChannel(uint8_t channel_id, const PulseChannelConfig_t& config) = 0;

    /**
     * @brief Configure the master channel
     * @param gpio_pin GPIO pin for master output
     * @return true on success, false on failure
     */
    virtual bool configureMaster(uint8_t gpio_pin) = 0;

    /**
     * @brief Configure a slave channel with phase offset
     * @param slave_id Slave channel number (1-5)
     * @param gpio_pin GPIO pin for slave output
     * @param phase_offset_deg Phase offset in degrees relative to master
     * @param enabled Initial enable state
     * @return true on success, false on failure
     */
    virtual bool configureSlave(uint8_t slave_id, uint8_t gpio_pin, float phase_offset_deg, bool enabled = true) = 0;

    // ========== Frequency and Duty Cycle ==========

    /**
     * @brief Set frequency for all channels (synchronized)
     * @param frequency_hz Frequency in Hz (1 - 40000000)
     * @return true on success, false on failure
     */
    virtual bool setFrequency(double frequency_hz) = 0;

    /**
     * @brief Set duty cycle for a specific channel
     * @param channel_id Channel number (0-5)
     * @param duty_cycle Duty cycle (0.0 - 1.0)
     * @return true on success, false on failure
     */
    virtual bool setDutyCycle(uint8_t channel_id, float duty_cycle) = 0;

    /**
     * @brief Set duty cycle for all enabled channels
     * @param duty_cycle Duty cycle (0.0 - 1.0)
     * @return true on success, false on failure
     */
    virtual bool setAllDutyCycles(float duty_cycle) = 0;

    // ========== Exact Parameters ==========

    /**
     * @brief Set exact period for all channels
     * @param period_us Period in microseconds
     * @return true on success, false on failure
     */
    virtual bool setPeriod(uint32_t period_us) = 0;

    /**
     * @brief Set exact pulse width for a specific channel
     * @param channel_id Channel number (0-5)
     * @param pulse_width_us Pulse width in microseconds
     * @return true on success, false on failure
     */
    virtual bool setPulseWidth(uint8_t channel_id, uint32_t pulse_width_us) = 0;

    /**
     * @brief Set pulse width for all enabled channels
     * @param pulse_width_us Pulse width in microseconds
     * @return true on success, false on failure
     */
    virtual bool setAllPulseWidths(uint32_t pulse_width_us) = 0;

    /**
     * @brief Set comprehensive pulse parameters
     * @param channel_id Channel number (0-5)
     * @param params Comprehensive parameters structure
     * @return true on success, false on failure
     */
    virtual bool setParams(uint8_t channel_id, const PulseParams_t& params) = 0;

    /**
     * @brief Set phase delay in microseconds
     * @param channel_id Channel number (0-5)
     * @param delay_us Phase delay in microseconds
     * @return true on success, false on failure
     */
    virtual bool setPhaseDelay(uint8_t channel_id, uint32_t delay_us) = 0;

    /**
     * @brief Set signal polarity for a channel
     * @param channel_id Channel number (0-5)
     * @param polarity POLARITY_ACTIVE_HIGH or POLARITY_ACTIVE_LOW
     * @return true on success, false on failure
     */
    virtual bool setPolarity(uint8_t channel_id, SignalPolarity polarity) = 0;

    // ========== Control ==========

    /**
     * @brief Start pulse generation on all enabled channels
     * @return true on success, false on failure
     */
    virtual bool start() = 0;

    /**
     * @brief Stop pulse generation on all channels
     * @return true on success, false on failure
     */
    virtual bool stop() = 0;

    /**
     * @brief Enable or disable a specific channel
     * @param channel_id Channel number (0-5)
     * @param enabled true to enable, false to disable
     * @return true on success, false on failure
     */
    virtual bool enableChannel(uint8_t channel_id, bool enabled) = 0;

    /**
     * @brief Enable or disable a slave channel
     * @param slave_id Slave channel number (1-5)
     * @param enabled true to enable, false to disable
     * @return true on success, false on failure
     */
    virtual bool enableSlave(uint8_t slave_id, bool enabled) = 0;

    /**
     * @brief Disable all slave channels
     */
    virtual void disableAllSlaves() = 0;

    /**
     * @brief Enable all configured slave channels
     */
    virtual void enableAllSlaves() = 0;

    /**
     * @brief Trigger a software sync to realign all channels
     * @return true on success, false on failure
     */
    virtual bool triggerSync() = 0;

    // ========== Getters ==========

    /**
     * @brief Get current frequency
     * @return Frequency in Hz
     */
    virtual double getFrequency() const = 0;

    /**
     * @brief Get duty cycle for a channel
     * @param channel_id Channel number (0-5)
     * @return Duty cycle (0.0 - 1.0)
     */
    virtual float getDutyCycle(uint8_t channel_id) const = 0;

    /**
     * @brief Check if pulse generator is running
     * @return true if running, false otherwise
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Check if a channel is enabled
     * @param channel_id Channel number (0-5)
     * @return true if enabled, false otherwise
     */
    virtual bool isChannelEnabled(uint8_t channel_id) const = 0;

    /**
     * @brief Get GPIO pin for a channel
     * @param channel_id Channel number (0-5)
     * @return GPIO pin number
     */
    virtual uint8_t getChannelPin(uint8_t channel_id) const = 0;

    /**
     * @brief Get current period in microseconds
     * @return Period in microseconds
     */
    virtual uint32_t getPeriodUs() const = 0;

    /**
     * @brief Get pulse width for a channel
     * @param channel_id Channel number (0-5)
     * @return Pulse width in microseconds
     */
    virtual uint32_t getPulseWidthUs(uint8_t channel_id) const = 0;

    /**
     * @brief Get phase delay for a channel
     * @param channel_id Channel number (0-5)
     * @return Phase delay in microseconds
     */
    virtual uint32_t getPhaseDelayUs(uint8_t channel_id) const = 0;

    /**
     * @brief Get master channel duty cycle
     * @return Master duty cycle (0.0 - 1.0)
     */
    virtual float getMasterDutyCycle() const = 0;

    /**
     * @brief Get number of enabled slave channels
     * @return Count of enabled slaves (0-5)
     */
    virtual uint8_t getEnabledSlaveCount() const = 0;

    /**
     * @brief Check if a channel is the master
     * @param channel_id Channel to check
     * @return true if channel is the master
     */
    virtual bool isMasterChannel(uint8_t channel_id) const = 0;

    /**
     * @brief Get complete channel configuration
     * @param channel_id Channel number (0-5)
     * @param config Output parameter to receive configuration
     * @return true on success, false if channel_id is invalid
     */
    virtual bool getChannelConfig(uint8_t channel_id, PulseChannelConfig_t& config) const = 0;

    /**
     * @brief Get phase offset for a channel
     * @param channel_id Channel number (0-5)
     * @return Phase offset in degrees (0-360)
     */
    virtual float getPhaseOffset(uint8_t channel_id) const = 0;

    /**
     * @brief Get signal polarity for a channel
     * @param channel_id Channel number (0-5)
     * @return Signal polarity
     */
    virtual SignalPolarity getPolarity(uint8_t channel_id) const = 0;

    // ========== Indicator ==========

    /**
     * @brief Set the status indicator GPIO pin
     * @param gpio_pin GPIO pin number (0 = disabled)
     * @return true on success, false on failure
     */
    virtual bool setIndicatorPin(uint8_t gpio_pin) = 0;

    /**
     * @brief Get the current indicator pin
     * @return GPIO pin number (0 = disabled)
     */
    virtual uint8_t getIndicatorPin() const = 0;

    /**
     * @brief Check if indicator is enabled
     * @return true if indicator pin is configured
     */
    virtual bool hasIndicator() const = 0;
};

#endif // I_PULSE_GENERATOR_H
