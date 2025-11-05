#ifndef PULSE_GENERATOR_H
#define PULSE_GENERATOR_H

#include <stdint.h>
#include "driver/mcpwm.h"
#include "driver/gpio.h"

/**
 * @brief Multi-channel pulse generator using ESP32 MCPWM peripheral
 *
 * Features:
 * - Up to 6 synchronized output channels (3 per MCPWM unit)
 * - Hardware-based synchronization for precise phase alignment
 * - Configurable phase offset per channel (for trigger delays)
 * - Individual channel enable/disable (skip functionality)
 * - Nanosecond-level timing precision
 */

#define MAX_PULSE_CHANNELS 6

// Channel configuration structure
typedef struct {
    uint8_t gpio_pin;           // GPIO pin for this channel
    float phase_offset_deg;     // Phase offset in degrees (0-360)
    bool enabled;               // Enable/disable this channel
    uint8_t skip_count;         // Skip N pulses (0 = no skip, future feature)
} PulseChannelConfig_t;

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
     * @param channel_id Channel number (0-5)
     * @param config Channel configuration
     * @return true on success, false on failure
     */
    bool configureChannel(uint8_t channel_id, const PulseChannelConfig_t& config);

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
     */
    bool enableChannel(uint8_t channel_id, bool enabled);

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

private:
    // Internal state
    double _frequency;
    bool _is_running;
    PulseChannelConfig_t _channels[MAX_PULSE_CHANNELS];
    float _duty_cycles[MAX_PULSE_CHANNELS];

    // MCPWM mapping
    mcpwm_unit_t _mcpwm_unit;
    mcpwm_io_signals_t _io_signals[MAX_PULSE_CHANNELS];
    mcpwm_timer_t _timers[MAX_PULSE_CHANNELS];
    mcpwm_generator_t _generators[MAX_PULSE_CHANNELS];

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
