#ifndef PULSE_GENERATOR_H
#define PULSE_GENERATOR_H

#include <stdint.h>
#include "driver/mcpwm.h"
#include "driver/gpio.h"
#include "signal_iface.h"  // For PulseParams_t and helper functions

/**
 * @brief Multi-channel pulse generator using ESP32 MCPWM peripheral
 *
 * Architecture:
 * - Channel 0 is the MASTER channel (always enabled, cannot be disabled)
 * - Channels 1-5 are SLAVE channels (follow master, can be individually disabled)
 *
 * Features:
 * - Up to 6 synchronized output channels (3 per MCPWM unit)
 * - Hardware-based synchronization for precise phase alignment
 * - Configurable phase offset per channel (for trigger delays)
 * - Individual slave channel enable/disable (skip functionality)
 * - Nanosecond-level timing precision
 * - Master channel controls frequency for all enabled channels
 */

#define MAX_PULSE_CHANNELS 6
#define MASTER_CHANNEL 0

// Channel configuration structure
typedef struct {
    uint8_t gpio_pin;           // GPIO pin for this channel
    float phase_offset_deg;     // Phase offset in degrees (0-360) relative to master
    bool enabled;               // Enable/disable (master channel is always enabled)
    uint8_t skip_count;         // Skip N pulses (0 = no skip, future feature)
} PulseChannelConfig_t;

// Channel type
typedef enum {
    CHANNEL_MASTER = 0,         // Master channel (channel 0)
    CHANNEL_SLAVE = 1           // Slave channels (channels 1-5)
} PulseChannelType_t;

class PulseGenerator {
public:
    PulseGenerator();

    /**
     * @brief Initialize the pulse generator
     * @return true on success, false on failure
     */
    bool begin();

    /**
     * @brief Configure a specific channel
     * @param channel_id Channel number (0-5, where 0 is master)
     * @param config Channel configuration
     * @return true on success, false on failure
     * @note Master channel (0) is always enabled regardless of config.enabled
     */
    bool configureChannel(uint8_t channel_id, const PulseChannelConfig_t& config);

    /**
     * @brief Configure the master channel
     * @param gpio_pin GPIO pin for master output
     * @return true on success, false on failure
     */
    bool configureMaster(uint8_t gpio_pin);

    /**
     * @brief Configure a slave channel with phase offset
     * @param slave_id Slave channel number (1-5)
     * @param gpio_pin GPIO pin for slave output
     * @param phase_offset_deg Phase offset in degrees relative to master
     * @param enabled Initial enable state
     * @return true on success, false on failure
     */
    bool configureSlave(uint8_t slave_id, uint8_t gpio_pin, float phase_offset_deg, bool enabled = true);

    /**
     * @brief Set frequency for all channels (synchronized)
     * @param frequency_hz Frequency in Hz (1 - 40000000)
     * @return true on success, false on failure
     */
    bool setFrequency(double frequency_hz);

    /**
     * @brief Set duty cycle for a specific channel
     * @param channel_id Channel number (0-5)
     * @param duty_cycle Duty cycle (0.0 - 1.0)
     * @return true on success, false on failure
     */
    bool setDutyCycle(uint8_t channel_id, float duty_cycle);

    /**
     * @brief Set duty cycle for all enabled channels
     * @param duty_cycle Duty cycle (0.0 - 1.0)
     * @return true on success, false on failure
     */
    bool setAllDutyCycles(float duty_cycle);

    // ===== EXACT PARAMETER METHODS =====

    /**
     * @brief Set exact period for all channels (alternative to setFrequency)
     * @param period_us Period in microseconds (25 to 1000000000)
     * @return true on success, false on failure
     */
    bool setPeriod(uint32_t period_us);

    /**
     * @brief Set exact pulse width for a specific channel
     * @param channel_id Channel number (0-5)
     * @param pulse_width_us Pulse width in microseconds
     * @return true on success, false on failure
     * @note Pulse width must be less than period
     */
    bool setPulseWidth(uint8_t channel_id, uint32_t pulse_width_us);

    /**
     * @brief Set pulse width for all enabled channels
     * @param pulse_width_us Pulse width in microseconds
     * @return true on success, false on failure
     */
    bool setAllPulseWidths(uint32_t pulse_width_us);

    /**
     * @brief Set comprehensive pulse parameters using PulseParams_t structure
     * @param channel_id Channel number (0-5)
     * @param params Comprehensive parameters structure
     * @return true on success, false on failure
     */
    bool setParams(uint8_t channel_id, const PulseParams_t& params);

    /**
     * @brief Set phase delay in microseconds (alternative to phase in degrees)
     * @param channel_id Channel number (0-5)
     * @param delay_us Phase delay in microseconds
     * @return true on success, false on failure
     */
    bool setPhaseDelay(uint8_t channel_id, uint32_t delay_us);

    /**
     * @brief Set signal polarity for a channel
     * @param channel_id Channel number (0-5)
     * @param polarity POLARITY_ACTIVE_HIGH or POLARITY_ACTIVE_LOW
     * @return true on success, false on failure
     */
    bool setPolarity(uint8_t channel_id, SignalPolarity polarity);

    /**
     * @brief Get current period in microseconds
     * @return Period in microseconds
     */
    uint32_t getPeriodUs() const;

    /**
     * @brief Get current pulse width for a channel
     * @param channel_id Channel number (0-5)
     * @return Pulse width in microseconds
     */
    uint32_t getPulseWidthUs(uint8_t channel_id) const;

    /**
     * @brief Get phase delay in microseconds for a channel
     * @param channel_id Channel number (0-5)
     * @return Phase delay in microseconds
     */
    uint32_t getPhaseDelayUs(uint8_t channel_id) const;

    /**
     * @brief Start pulse generation on all enabled channels (synchronized)
     * @return true on success, false on failure
     */
    bool start();

    /**
     * @brief Stop pulse generation on all channels
     * @return true on success, false on failure
     */
    bool stop();

    /**
     * @brief Enable or disable a specific channel
     * @param channel_id Channel number (0-5)
     * @param enabled true to enable, false to disable
     * @return true on success, false on failure
     * @note Master channel (0) cannot be disabled - calling with channel 0 returns false
     */
    bool enableChannel(uint8_t channel_id, bool enabled);

    /**
     * @brief Enable or disable a slave channel
     * @param slave_id Slave channel number (1-5)
     * @param enabled true to enable, false to disable
     * @return true on success, false on failure
     */
    bool enableSlave(uint8_t slave_id, bool enabled);

    /**
     * @brief Disable all slave channels (master remains active)
     */
    void disableAllSlaves();

    /**
     * @brief Enable all configured slave channels
     */
    void enableAllSlaves();

    /**
     * @brief Trigger a software sync to realign all channels
     * @return true on success, false on failure
     */
    bool triggerSync();

    // Getters
    double getFrequency() const { return _frequency; }
    float getDutyCycle(uint8_t channel_id) const;
    bool isRunning() const { return _is_running; }
    bool isChannelEnabled(uint8_t channel_id) const;
    uint8_t getChannelPin(uint8_t channel_id) const;

    /**
     * @brief Get the master channel's duty cycle
     * @return Master duty cycle (0.0 - 1.0)
     */
    float getMasterDutyCycle() const { return getDutyCycle(MASTER_CHANNEL); }

    /**
     * @brief Get number of enabled slave channels
     * @return Count of enabled slaves (0-5)
     */
    uint8_t getEnabledSlaveCount() const;

    /**
     * @brief Check if a channel is the master
     * @param channel_id Channel to check
     * @return true if channel is the master
     */
    bool isMasterChannel(uint8_t channel_id) const { return channel_id == MASTER_CHANNEL; }

    /**
     * @brief Get complete channel configuration
     * @param channel_id Channel number (0-5)
     * @param config Output parameter to receive channel configuration
     * @return true on success, false if channel_id is invalid
     */
    bool getChannelConfig(uint8_t channel_id, PulseChannelConfig_t& config) const;

    /**
     * @brief Get phase offset for a channel
     * @param channel_id Channel number (0-5)
     * @return Phase offset in degrees (0-360), or 0 if invalid channel
     */
    float getPhaseOffset(uint8_t channel_id) const;

    /**
     * @brief Get signal polarity for a channel
     * @param channel_id Channel number (0-5)
     * @return Signal polarity (POLARITY_ACTIVE_HIGH or POLARITY_ACTIVE_LOW)
     */
    SignalPolarity getPolarity(uint8_t channel_id) const;

    // ===== STATUS INDICATOR METHODS =====

    /**
     * @brief Set the status indicator GPIO pin
     * @param gpio_pin GPIO pin number (0 = disabled)
     * @return true on success, false on failure
     *
     * The indicator pin will be HIGH when signal is running, LOW when stopped.
     * Useful for LED indicators, enable signals, or monitoring.
     */
    bool setIndicatorPin(uint8_t gpio_pin);

    /**
     * @brief Get the current indicator pin
     * @return GPIO pin number (0 = disabled)
     */
    uint8_t getIndicatorPin() const { return _indicator_pin; }

    /**
     * @brief Check if indicator is enabled
     * @return true if indicator pin is configured
     */
    bool hasIndicator() const { return _indicator_pin > 0; }

private:
    // Internal state
    double _frequency;
    uint32_t _period_us;         // Period in microseconds
    bool _is_running;
    PulseChannelConfig_t _channels[MAX_PULSE_CHANNELS];
    float _duty_cycles[MAX_PULSE_CHANNELS];
    uint32_t _pulse_widths_us[MAX_PULSE_CHANNELS];  // Exact pulse widths
    uint32_t _phase_delays_us[MAX_PULSE_CHANNELS];  // Phase delays in microseconds
    SignalPolarity _polarities[MAX_PULSE_CHANNELS]; // Channel polarities

    // MCPWM mapping
    mcpwm_unit_t _mcpwm_unit;
    mcpwm_io_signals_t _io_signals[MAX_PULSE_CHANNELS];
    mcpwm_timer_t _timers[MAX_PULSE_CHANNELS];
    mcpwm_generator_t _generators[MAX_PULSE_CHANNELS];

    // Status indicator
    uint8_t _indicator_pin;      // GPIO pin for status indicator (0 = disabled)

    // Helper functions
    bool _initMCPWM();
    bool _setupSync();
    bool _applyPhaseOffset(uint8_t channel_id, float phase_deg);
    mcpwm_unit_t _getUnit(uint8_t channel_id);
    mcpwm_timer_t _getTimer(uint8_t channel_id);
    mcpwm_generator_t _getGenerator(uint8_t channel_id);
    mcpwm_io_signals_t _getIOSignal(uint8_t channel_id);
};

#endif // PULSE_GENERATOR_H
