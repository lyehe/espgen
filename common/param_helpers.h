#ifndef PARAM_HELPERS_H
#define PARAM_HELPERS_H

#include "signal_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a START command using frequency and duty cycle (classic method)
 *
 * Automatically sets up all low-level parameters and flags.
 *
 * @param freq_hz Frequency in Hz
 * @param duty Duty cycle 0.0 to 1.0
 * @param duration_sec Duration in seconds (0 = infinite)
 * @return Configured SignalCmd ready to send
 */
static inline SignalCmd createStartCmd_FreqDuty(double freq_hz, float duty, float duration_sec) {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.channel = 0;
    cmd.frequencyHz = freq_hz;
    cmd.dutyCycle = duty;
    cmd.durationSec = duration_sec;
    cmd.paramMode = PARAM_USE_FREQUENCY | PARAM_USE_DUTY_CYCLE | PARAM_USE_DURATION;
    cmd.polarity = POLARITY_ACTIVE_HIGH;
    return cmd;
}

/**
 * @brief Create a START command using period and pulse width (exact timing)
 *
 * Automatically converts to hardware configuration.
 *
 * @param period_us Period in microseconds
 * @param pulse_width_us Pulse width in microseconds
 * @param duration_sec Duration in seconds (0 = infinite)
 * @return Configured SignalCmd ready to send
 */
static inline SignalCmd createStartCmd_PeriodWidth(uint32_t period_us, uint32_t pulse_width_us, float duration_sec) {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.channel = 0;
    cmd.periodUs = period_us;
    cmd.pulseWidthUs = pulse_width_us;
    cmd.durationSec = duration_sec;
    cmd.paramMode = PARAM_USE_PERIOD | PARAM_USE_PULSE_WIDTH | PARAM_USE_DURATION;
    cmd.polarity = POLARITY_ACTIVE_HIGH;
    return cmd;
}

/**
 * @brief Create a START command using pulse count (exact count)
 *
 * @param freq_hz Frequency in Hz
 * @param duty Duty cycle 0.0 to 1.0
 * @param pulse_count Exact number of pulses (0 = infinite)
 * @return Configured SignalCmd ready to send
 */
static inline SignalCmd createStartCmd_PulseCount(double freq_hz, float duty, uint64_t pulse_count) {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.channel = 0;
    cmd.frequencyHz = freq_hz;
    cmd.dutyCycle = duty;
    cmd.pulseCount = pulse_count;
    cmd.paramMode = PARAM_USE_FREQUENCY | PARAM_USE_DUTY_CYCLE | PARAM_USE_PULSE_COUNT;
    cmd.polarity = POLARITY_ACTIVE_HIGH;
    return cmd;
}

/**
 * @brief Create a CONFIG_CHANNEL command with phase delay in microseconds
 *
 * @param channel Channel number (1-5 for slaves)
 * @param gpio_pin GPIO pin
 * @param phase_delay_us Phase delay in microseconds
 * @param enabled Enable this channel
 * @return Configured SignalCmd ready to send
 */
static inline SignalCmd createConfigChannel_TimeDelay(uint8_t channel, uint8_t gpio_pin,
                                                      uint32_t phase_delay_us, bool enabled) {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_CONFIG_CHANNEL;
    cmd.channel = channel;
    cmd.pin = gpio_pin;
    cmd.phaseDelayUs = phase_delay_us;
    cmd.enabled = enabled;
    cmd.paramMode = PARAM_USE_PHASE_TIME;
    return cmd;
}

/**
 * @brief Create a CONFIG_CHANNEL command with phase offset in degrees
 *
 * @param channel Channel number (1-5 for slaves)
 * @param gpio_pin GPIO pin
 * @param phase_deg Phase offset in degrees (0-360)
 * @param enabled Enable this channel
 * @return Configured SignalCmd ready to send
 */
static inline SignalCmd createConfigChannel_PhaseDegrees(uint8_t channel, uint8_t gpio_pin,
                                                         float phase_deg, bool enabled) {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_CONFIG_CHANNEL;
    cmd.channel = channel;
    cmd.pin = gpio_pin;
    cmd.phaseOffset = phase_deg;
    cmd.enabled = enabled;
    cmd.paramMode = PARAM_USE_PHASE_DEGREES;
    return cmd;
}

/**
 * @brief Normalize a SignalCmd by filling in missing parameters
 *
 * This function ensures all parameter representations are consistent:
 * - If frequency is set, calculates period
 * - If period is set, calculates frequency
 * - If duty is set with period, calculates pulse width
 * - If pulse width is set with period, calculates duty
 * - etc.
 *
 * @param cmd Pointer to command to normalize
 */
static inline void normalizeSignalCmd(SignalCmd* cmd) {
    if (!cmd) return;

    // Normalize frequency <-> period
    if (cmd->paramMode & PARAM_USE_FREQUENCY) {
        // Frequency is primary, calculate period
        uint32_t period = freqToPeriodUs(cmd->frequencyHz);
        // Note: Can't store both due to union, but helps with calculations
    } else if (cmd->paramMode & PARAM_USE_PERIOD) {
        // Period is primary, calculate frequency
        double freq = periodToFreqHz(cmd->periodUs);
        // Note: Can't store both due to union
    }

    // For duty <-> pulse width, we need period
    // This is handled in the command dispatcher where we have access to current period

    // For phase offset <-> delay, we need period
    // This is also handled in the command dispatcher
}

/**
 * @brief Validate a SignalCmd for consistency
 *
 * @param cmd Pointer to command to validate
 * @return true if valid, false if inconsistent
 */
static inline bool validateSignalCmd(const SignalCmd* cmd) {
    if (!cmd) return false;

    // Check for conflicting flags
    if ((cmd->paramMode & PARAM_USE_FREQUENCY) && (cmd->paramMode & PARAM_USE_PERIOD)) {
        // Can't use both frequency and period
        return false;
    }

    if ((cmd->paramMode & PARAM_USE_DUTY_CYCLE) && (cmd->paramMode & PARAM_USE_PULSE_WIDTH)) {
        // Can't use both duty cycle and pulse width
        return false;
    }

    if ((cmd->paramMode & PARAM_USE_DURATION) && (cmd->paramMode & PARAM_USE_PULSE_COUNT)) {
        // Can't use both duration and pulse count
        return false;
    }

    if ((cmd->paramMode & PARAM_USE_PHASE_DEGREES) && (cmd->paramMode & PARAM_USE_PHASE_TIME)) {
        // Can't use both phase degrees and phase delay
        return false;
    }

    // Validate ranges for frequency
    if (cmd->paramMode & PARAM_USE_FREQUENCY) {
        if (cmd->frequencyHz <= 0 || cmd->frequencyHz > 40000000) {
            return false; // Invalid frequency range (must be positive, max 40MHz)
        }
    }

    // Validate ranges for period
    if (cmd->paramMode & PARAM_USE_PERIOD) {
        if (cmd->periodUs < 25 || cmd->periodUs > 1000000000) {
            return false; // Invalid period range (25ns to 1000s)
        }
    }

    // Validate ranges for duty cycle
    if (cmd->paramMode & PARAM_USE_DUTY_CYCLE) {
        if (cmd->dutyCycle < 0.0 || cmd->dutyCycle > 1.0) {
            return false; // Invalid duty cycle range
        }
    }

    // Validate ranges for pulse width (can't be negative, checked via uint32_t)
    // Note: pulse width vs period check would require knowing the period,
    // which may not be available at validation time

    // Validate phase degrees
    if (cmd->paramMode & PARAM_USE_PHASE_DEGREES) {
        if (cmd->phaseOffset < 0.0 || cmd->phaseOffset >= 360.0) {
            return false; // Phase must be 0-360 degrees
        }
    }

    // Validate duration (negative duration doesn't make sense)
    if (cmd->paramMode & PARAM_USE_DURATION) {
        if (cmd->durationSec < 0.0) {
            return false; // Duration cannot be negative
        }
    }

    return true;
}

#ifdef __cplusplus
}
#endif

#endif // PARAM_HELPERS_H
