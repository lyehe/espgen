#include "PulseGenerator.h"
#include <Arduino.h>
#include "soc/mcpwm_periph.h"

// Default configuration
#define DEFAULT_FREQUENCY_HZ 1000.0
#define DEFAULT_DUTY_CYCLE 0.5
#define MCPWM_RESOLUTION_HZ 1000000  // 1 MHz base frequency for good resolution

PulseGenerator::PulseGenerator() :
    _frequency(DEFAULT_FREQUENCY_HZ),
    _is_running(false),
    _mcpwm_unit(MCPWM_UNIT_0)
{
    // Initialize channel configurations
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        _channels[i].gpio_pin = 0;
        _channels[i].phase_offset_deg = 0.0;
        _channels[i].enabled = (i == MASTER_CHANNEL);  // Master is always enabled
        _channels[i].skip_count = 0;
        _duty_cycles[i] = DEFAULT_DUTY_CYCLE;
    }

    // Map MCPWM I/O signals for 6 channels
    // Unit 0, Timer 0: MCPWM0A, MCPWM0B
    // Unit 0, Timer 1: MCPWM1A, MCPWM1B
    // Unit 0, Timer 2: MCPWM2A, MCPWM2B
    _io_signals[0] = MCPWM0A;
    _io_signals[1] = MCPWM0B;
    _io_signals[2] = MCPWM1A;
    _io_signals[3] = MCPWM1B;
    _io_signals[4] = MCPWM2A;
    _io_signals[5] = MCPWM2B;

    _timers[0] = MCPWM_TIMER_0;
    _timers[1] = MCPWM_TIMER_0;
    _timers[2] = MCPWM_TIMER_1;
    _timers[3] = MCPWM_TIMER_1;
    _timers[4] = MCPWM_TIMER_2;
    _timers[5] = MCPWM_TIMER_2;

    _generators[0] = MCPWM_GEN_A;
    _generators[1] = MCPWM_GEN_B;
    _generators[2] = MCPWM_GEN_A;
    _generators[3] = MCPWM_GEN_B;
    _generators[4] = MCPWM_GEN_A;
    _generators[5] = MCPWM_GEN_B;
}

bool PulseGenerator::begin() {
    Serial.println("PulseGenerator: Initializing MCPWM...");

    if (!_initMCPWM()) {
        Serial.println("PulseGenerator: Failed to initialize MCPWM");
        return false;
    }

    Serial.println("PulseGenerator: Initialization complete");
    return true;
}

bool PulseGenerator::configureChannel(uint8_t channel_id, const PulseChannelConfig_t& config) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid channel ID %d\n", channel_id);
        return false;
    }

    // Store configuration
    _channels[channel_id] = config;

    // Master channel (channel 0) is ALWAYS enabled
    if (channel_id == MASTER_CHANNEL) {
        _channels[channel_id].enabled = true;
        Serial.printf("PulseGenerator: Configuring MASTER channel (Pin: %d, Phase: %.1f°)\n",
                      config.gpio_pin, config.phase_offset_deg);
    } else {
        Serial.printf("PulseGenerator: Configuring SLAVE channel %d (Pin: %d, Phase: %.1f°, Enabled: %d)\n",
                      channel_id, config.gpio_pin, config.phase_offset_deg, config.enabled);
    }

    // Configure GPIO pin
    if (config.gpio_pin > 0 && _channels[channel_id].enabled) {
        mcpwm_gpio_init(_mcpwm_unit, _io_signals[channel_id], config.gpio_pin);
        Serial.printf("PulseGenerator: GPIO %d configured for channel %d signal\n",
                     config.gpio_pin, channel_id);
    }

    return true;
}

bool PulseGenerator::configureMaster(uint8_t gpio_pin) {
    PulseChannelConfig_t master_config = {
        .gpio_pin = gpio_pin,
        .phase_offset_deg = 0.0,  // Master has no phase offset
        .enabled = true,           // Master is always enabled
        .skip_count = 0
    };
    return configureChannel(MASTER_CHANNEL, master_config);
}

bool PulseGenerator::configureSlave(uint8_t slave_id, uint8_t gpio_pin, float phase_offset_deg, bool enabled) {
    if (slave_id == MASTER_CHANNEL || slave_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid slave ID %d (must be 1-%d)\n", slave_id, MAX_PULSE_CHANNELS - 1);
        return false;
    }

    PulseChannelConfig_t slave_config = {
        .gpio_pin = gpio_pin,
        .phase_offset_deg = phase_offset_deg,
        .enabled = enabled,
        .skip_count = 0
    };
    return configureChannel(slave_id, slave_config);
}

bool PulseGenerator::setFrequency(double frequency_hz) {
    if (frequency_hz <= 0 || frequency_hz > 40000000) {
        Serial.printf("PulseGenerator: Invalid frequency %.2f Hz\n", frequency_hz);
        return false;
    }

    _frequency = frequency_hz;
    Serial.printf("PulseGenerator: Setting frequency to %.2f Hz\n", frequency_hz);

    // Calculate period in microseconds for MCPWM
    uint32_t period_us = (uint32_t)(1000000.0 / frequency_hz);

    // Update all timers with the new frequency
    mcpwm_config_t pwm_config = {
        .frequency = (uint32_t)frequency_hz,
        .cmpr_a = 0,
        .cmpr_b = 0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_COUNTER,
    };

    // Configure each timer (TIMER_0, TIMER_1, TIMER_2)
    for (int timer_idx = 0; timer_idx < 3; timer_idx++) {
        mcpwm_init(_mcpwm_unit, (mcpwm_timer_t)timer_idx, &pwm_config);
    }

    // Reapply duty cycles after frequency change
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        if (_channels[i].enabled) {
            setDutyCycle(i, _duty_cycles[i]);
        }
    }

    return true;
}

bool PulseGenerator::setDutyCycle(uint8_t channel_id, float duty_cycle) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid channel ID %d\n", channel_id);
        return false;
    }

    if (duty_cycle < 0.0) duty_cycle = 0.0;
    if (duty_cycle > 1.0) duty_cycle = 1.0;

    _duty_cycles[channel_id] = duty_cycle;

    if (!_channels[channel_id].enabled) {
        return true; // Don't apply if channel is disabled
    }

    mcpwm_timer_t timer = _timers[channel_id];
    mcpwm_generator_t gen = _generators[channel_id];

    // Convert duty cycle to percentage for MCPWM API
    float duty_percent = duty_cycle * 100.0;

    mcpwm_set_duty(_mcpwm_unit, timer, gen, duty_percent);
    mcpwm_set_duty_type(_mcpwm_unit, timer, gen, MCPWM_DUTY_MODE_0);

    Serial.printf("PulseGenerator: Channel %d duty set to %.2f%%\n", channel_id, duty_percent);
    return true;
}

bool PulseGenerator::setAllDutyCycles(float duty_cycle) {
    bool success = true;
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        if (_channels[i].enabled) {
            success &= setDutyCycle(i, duty_cycle);
        }
    }
    return success;
}

bool PulseGenerator::start() {
    Serial.println("PulseGenerator: Starting all enabled channels...");

    // Setup synchronization
    if (!_setupSync()) {
        Serial.println("PulseGenerator: Failed to setup sync");
        return false;
    }

    // Start all timers
    for (int timer_idx = 0; timer_idx < 3; timer_idx++) {
        mcpwm_start(_mcpwm_unit, (mcpwm_timer_t)timer_idx);
    }

    // Apply phase offsets
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        if (_channels[i].enabled && _channels[i].phase_offset_deg != 0.0) {
            _applyPhaseOffset(i, _channels[i].phase_offset_deg);
        }
    }

    _is_running = true;
    Serial.println("PulseGenerator: All channels started");
    return true;
}

bool PulseGenerator::stop() {
    Serial.println("PulseGenerator: Stopping all channels...");

    // Stop all timers
    for (int timer_idx = 0; timer_idx < 3; timer_idx++) {
        mcpwm_stop(_mcpwm_unit, (mcpwm_timer_t)timer_idx);
    }

    // Set all outputs low
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        if (_channels[i].enabled && _channels[i].gpio_pin > 0) {
            setDutyCycle(i, 0.0);
        }
    }

    _is_running = false;
    Serial.println("PulseGenerator: All channels stopped");
    return true;
}

bool PulseGenerator::enableChannel(uint8_t channel_id, bool enabled) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return false;
    }

    // Master channel (channel 0) CANNOT be disabled
    if (channel_id == MASTER_CHANNEL && !enabled) {
        Serial.println("PulseGenerator: ERROR - Cannot disable master channel!");
        return false;
    }

    _channels[channel_id].enabled = enabled;

    if (channel_id == MASTER_CHANNEL) {
        Serial.println("PulseGenerator: Master channel remains enabled");
    } else {
        Serial.printf("PulseGenerator: Slave channel %d %s\n", channel_id, enabled ? "enabled" : "disabled");
    }

    if (!enabled && _is_running) {
        // If disabling while running, set duty to 0
        setDutyCycle(channel_id, 0.0);
    } else if (enabled && _is_running) {
        // If enabling while running, restore duty cycle
        setDutyCycle(channel_id, _duty_cycles[channel_id]);
    }

    return true;
}

bool PulseGenerator::enableSlave(uint8_t slave_id, bool enabled) {
    if (slave_id == MASTER_CHANNEL || slave_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid slave ID %d (must be 1-%d)\n", slave_id, MAX_PULSE_CHANNELS - 1);
        return false;
    }
    return enableChannel(slave_id, enabled);
}

void PulseGenerator::disableAllSlaves() {
    Serial.println("PulseGenerator: Disabling all slave channels...");
    for (uint8_t i = 1; i < MAX_PULSE_CHANNELS; i++) {  // Start from 1 to skip master
        enableChannel(i, false);
    }
}

void PulseGenerator::enableAllSlaves() {
    Serial.println("PulseGenerator: Enabling all slave channels...");
    for (uint8_t i = 1; i < MAX_PULSE_CHANNELS; i++) {  // Start from 1 to skip master
        // Only enable if the channel has been configured (has a valid pin)
        if (_channels[i].gpio_pin > 0) {
            enableChannel(i, true);
        }
    }
}

bool PulseGenerator::triggerSync() {
    Serial.println("PulseGenerator: Triggering software sync...");

    // Sync all timers to realign phases
    mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_0, MCPWM_SELECT_SYNC_INT0, 0);
    mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_1, MCPWM_SELECT_SYNC_INT0, 0);
    mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_2, MCPWM_SELECT_SYNC_INT0, 0);

    return true;
}

float PulseGenerator::getDutyCycle(uint8_t channel_id) const {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return 0.0;
    }
    return _duty_cycles[channel_id];
}

bool PulseGenerator::isChannelEnabled(uint8_t channel_id) const {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return false;
    }
    return _channels[channel_id].enabled;
}

uint8_t PulseGenerator::getChannelPin(uint8_t channel_id) const {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return 0;
    }
    return _channels[channel_id].gpio_pin;
}

uint8_t PulseGenerator::getEnabledSlaveCount() const {
    uint8_t count = 0;
    for (uint8_t i = 1; i < MAX_PULSE_CHANNELS; i++) {  // Start from 1 to skip master
        if (_channels[i].enabled && _channels[i].gpio_pin > 0) {
            count++;
        }
    }
    return count;
}

// Private helper functions

bool PulseGenerator::_initMCPWM() {
    Serial.println("PulseGenerator: Initializing MCPWM peripheral...");

    // Initialize with default configuration
    mcpwm_config_t pwm_config = {
        .frequency = (uint32_t)_frequency,
        .cmpr_a = 0,
        .cmpr_b = 0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_COUNTER,
    };

    // Initialize all three timers
    for (int timer_idx = 0; timer_idx < 3; timer_idx++) {
        mcpwm_init(_mcpwm_unit, (mcpwm_timer_t)timer_idx, &pwm_config);
    }

    Serial.println("PulseGenerator: MCPWM peripheral initialized");
    return true;
}

bool PulseGenerator::_setupSync() {
    Serial.println("PulseGenerator: Setting up timer synchronization...");

    // Use TIMER_0 as sync source for TIMER_1 and TIMER_2
    // This ensures all timers start at the same time
    mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_0, MCPWM_SELECT_SYNC_INT0, 0);
    mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_1, MCPWM_SELECT_SYNC_INT0, 0);
    mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_2, MCPWM_SELECT_SYNC_INT0, 0);

    return true;
}

bool PulseGenerator::_applyPhaseOffset(uint8_t channel_id, float phase_deg) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return false;
    }

    // Normalize phase to 0-360 degrees
    while (phase_deg < 0) phase_deg += 360.0;
    while (phase_deg >= 360.0) phase_deg -= 360.0;

    mcpwm_timer_t timer = _timers[channel_id];

    // Calculate phase as percentage of period
    uint32_t phase_val = (uint32_t)(phase_deg / 360.0 * 1000); // MCPWM phase in 0-1000 range

    // Set phase for this timer
    mcpwm_set_phase(_mcpwm_unit, timer, MCPWM_SELECT_SYNC_INT0, phase_val);

    Serial.printf("PulseGenerator: Channel %d phase offset set to %.1f° (val: %u)\n",
                 channel_id, phase_deg, phase_val);
    return true;
}

mcpwm_unit_t PulseGenerator::_getUnit(uint8_t channel_id) {
    return _mcpwm_unit;
}

mcpwm_timer_t PulseGenerator::_getTimer(uint8_t channel_id) {
    if (channel_id >= MAX_PULSE_CHANNELS) return MCPWM_TIMER_0;
    return _timers[channel_id];
}

mcpwm_generator_t PulseGenerator::_getGenerator(uint8_t channel_id) {
    if (channel_id >= MAX_PULSE_CHANNELS) return MCPWM_GEN_A;
    return _generators[channel_id];
}

mcpwm_io_signals_t PulseGenerator::_getIOSignal(uint8_t channel_id) {
    if (channel_id >= MAX_PULSE_CHANNELS) return MCPWM0A;
    return _io_signals[channel_id];
}
