#include "PulseGenerator.h"
#include <Arduino.h>
#include "soc/mcpwm_periph.h"

// Default configuration
#define DEFAULT_FREQUENCY_HZ 1000.0
#define DEFAULT_DUTY_CYCLE 0.5
#define MCPWM_RESOLUTION_HZ 1000000  // 1 MHz base frequency for good resolution

PulseGenerator::PulseGenerator() :
    _frequency(DEFAULT_FREQUENCY_HZ),
    _period_us(freqToPeriodUs(DEFAULT_FREQUENCY_HZ)),
    _is_running(false),
    _mcpwm_unit(MCPWM_UNIT_0),
    _indicator_pin(0)
{
    // Initialize channel configurations
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        _channels[i].gpio_pin = 0;
        _channels[i].phase_offset_deg = 0.0;
        _channels[i].enabled = (i == MASTER_CHANNEL);  // Master is always enabled
        _channels[i].skip_count = 0;
        _duty_cycles[i] = DEFAULT_DUTY_CYCLE;
        _pulse_widths_us[i] = dutyToPulseWidthUs(DEFAULT_DUTY_CYCLE, _period_us);
        _phase_delays_us[i] = 0;
        _polarities[i] = POLARITY_ACTIVE_HIGH;
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

    // Setup timer synchronization (Timer 0 = master, Timers 1&2 = slaves)
    if (!_setupSync()) {
        Serial.println("PulseGenerator: WARNING - Failed to setup timer synchronization");
        // Continue anyway, but channels may not be perfectly synchronized
    }

    Serial.println("PulseGenerator: Initialization complete");
    return true;
}

bool PulseGenerator::configureChannel(uint8_t channel_id, const PulseChannelConfig_t& config) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: ERROR - Invalid channel ID %d (max %d)\n", channel_id, MAX_PULSE_CHANNELS - 1);
        return false;
    }

    // Validate GPIO pin (ESP32 has GPIO 0-39, but not all are usable for MCPWM)
    // GPIO 34-39 are input-only, so they can't be used for MCPWM output
    if (config.gpio_pin > 0) {
        if (config.gpio_pin > 33) {
            Serial.printf("PulseGenerator: ERROR - GPIO %d is invalid for MCPWM (use GPIO 0-33)\n", config.gpio_pin);
            return false;
        }
    }

    // Validate phase offset (already normalized in _applyPhaseOffset, but check for invalid values)
    if (!isfinite(config.phase_offset_deg)) {
        Serial.printf("PulseGenerator: ERROR - Invalid phase offset (NaN or infinity) for channel %d\n", channel_id);
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
        esp_err_t err = mcpwm_gpio_init(_mcpwm_unit, _io_signals[channel_id], config.gpio_pin);
        if (err != ESP_OK) {
            Serial.printf("PulseGenerator: ERROR - Failed to configure GPIO %d for channel %d: %s\n",
                         config.gpio_pin, channel_id, esp_err_to_name(err));
            // Don't mark channel as disabled, just return error
            // The user may retry with a different pin
            return false;
        }
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
    // ESP32 MCPWM timer resolution limits: ~8MHz maximum practical frequency
    if (frequency_hz <= 0 || frequency_hz > 8000000) {
        Serial.printf("PulseGenerator: Invalid frequency %.2f Hz (max 8MHz for ESP32 MCPWM)\n", frequency_hz);
        return false;
    }

    _frequency = frequency_hz;
    _period_us = freqToPeriodUs(frequency_hz);  // Update period
    Serial.printf("PulseGenerator: Setting frequency to %.2f Hz (Period: %u us)\n", frequency_hz, _period_us);

    // Calculate period in microseconds for MCPWM
    uint32_t period_us = _period_us;

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
        esp_err_t err = mcpwm_init(_mcpwm_unit, (mcpwm_timer_t)timer_idx, &pwm_config);
        if (err != ESP_OK) {
            Serial.printf("PulseGenerator: ERROR - Failed to configure timer %d: %s\n",
                         timer_idx, esp_err_to_name(err));
            return false;
        }
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
    _pulse_widths_us[channel_id] = dutyToPulseWidthUs(duty_cycle, _period_us);  // Keep pulse width in sync

    if (!_channels[channel_id].enabled) {
        return true; // Don't apply if channel is disabled
    }

    mcpwm_timer_t timer = _timers[channel_id];
    mcpwm_generator_t gen = _generators[channel_id];

    // Apply polarity setting (duty mode)
    mcpwm_duty_type_t duty_type = (_polarities[channel_id] == POLARITY_ACTIVE_HIGH)
                                   ? MCPWM_DUTY_MODE_0
                                   : MCPWM_DUTY_MODE_1;

    esp_err_t err = mcpwm_set_duty_type(_mcpwm_unit, timer, gen, duty_type);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: ERROR - Failed to set duty type for channel %d: %s\n",
                     channel_id, esp_err_to_name(err));
        return false;
    }

    // Convert duty cycle to percentage for MCPWM API
    float duty_percent = duty_cycle * 100.0;

    err = mcpwm_set_duty(_mcpwm_unit, timer, gen, duty_percent);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: ERROR - Failed to set duty for channel %d: %s\n",
                     channel_id, esp_err_to_name(err));
        return false;
    }

    Serial.printf("PulseGenerator: Channel %d duty set to %.2f%% (polarity=%s)\n",
                 channel_id, duty_percent,
                 _polarities[channel_id] == POLARITY_ACTIVE_HIGH ? "HIGH" : "LOW");
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

    // Apply phase offsets BEFORE starting (so sync config is ready)
    for (int i = 0; i < MAX_PULSE_CHANNELS; i++) {
        if (_channels[i].enabled && _channels[i].phase_offset_deg != 0.0) {
            _applyPhaseOffset(i, _channels[i].phase_offset_deg);
        }
    }

    // Start all timers in sequence (fast as possible for best sync)
    for (int timer_idx = 0; timer_idx < 3; timer_idx++) {
        esp_err_t err = mcpwm_start(_mcpwm_unit, (mcpwm_timer_t)timer_idx);
        if (err != ESP_OK) {
            Serial.printf("PulseGenerator: ERROR - Failed to start timer %d: %s\n",
                         timer_idx, esp_err_to_name(err));
            return false;
        }
    }

    // Trigger sync to align all timers
    triggerSync();

    _is_running = true;

    // Set status indicator HIGH (signal active)
    if (_indicator_pin > 0) {
        digitalWrite(_indicator_pin, HIGH);
        Serial.printf("PulseGenerator: Status indicator ON (GPIO %d)\n", _indicator_pin);
    }

    Serial.println("PulseGenerator: All channels started and synchronized");
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

    // Set status indicator LOW (signal inactive)
    if (_indicator_pin > 0) {
        digitalWrite(_indicator_pin, LOW);
        Serial.printf("PulseGenerator: Status indicator OFF (GPIO %d)\n", _indicator_pin);
    }

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

    // To trigger a sync event in ESP32 MCPWM, we use the software sync trigger
    // The SYNC_INT0 signal needs to be pulsed to trigger slave timers

    // Method: Use MCPWM sync configuration to trigger TEZ-based sync
    // This resets slave timers (1 and 2) to align with master (0)

    // Configure sync signal generation from Timer 0 TEZ (Timer Equal Zero)
    // This makes Timer 0 send a sync pulse when it reaches zero
    mcpwm_sync_config_t sync_conf = {
        .sync_sig = MCPWM_SELECT_SYNC_INT0,
        .timer_val = 0,
        .count_direction = MCPWM_TIMER_DIRECTION_UP,
    };

    // Apply sync configuration to trigger realignment
    esp_err_t err;

    // Slave timers receive the sync pulse and reset to their configured phase
    err = mcpwm_sync_configure(_mcpwm_unit, MCPWM_TIMER_1, &sync_conf);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: WARNING - Failed to sync Timer 1: %s\n",
                     esp_err_to_name(err));
    }

    err = mcpwm_sync_configure(_mcpwm_unit, MCPWM_TIMER_2, &sync_conf);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: WARNING - Failed to sync Timer 2: %s\n",
                     esp_err_to_name(err));
    }

    Serial.println("PulseGenerator: Sync triggered");
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

    // Timer 0 is the MASTER - disable sync input (free-running)
    esp_err_t err = mcpwm_sync_disable(_mcpwm_unit, MCPWM_TIMER_0);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: ERROR - Failed to disable sync on Timer 0: %s\n",
                     esp_err_to_name(err));
        return false;
    }
    Serial.println("PulseGenerator: Timer 0 configured as MASTER (sync disabled)");

    // Timers 1 and 2 are SLAVES - sync to internal sync signal 0
    // Phase offsets will be applied later via _applyPhaseOffset()
    err = mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_1, MCPWM_SELECT_SYNC_INT0, 0);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: ERROR - Failed to enable sync on Timer 1: %s\n",
                     esp_err_to_name(err));
        return false;
    }
    Serial.println("PulseGenerator: Timer 1 configured as SLAVE (sync enabled)");

    err = mcpwm_sync_enable(_mcpwm_unit, MCPWM_TIMER_2, MCPWM_SELECT_SYNC_INT0, 0);
    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: ERROR - Failed to enable sync on Timer 2: %s\n",
                     esp_err_to_name(err));
        return false;
    }
    Serial.println("PulseGenerator: Timer 2 configured as SLAVE (sync enabled)");

    Serial.println("PulseGenerator: Timer synchronization setup complete");
    return true;
}

bool PulseGenerator::_applyPhaseOffset(uint8_t channel_id, float phase_deg) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: ERROR - Invalid channel ID %d\n", channel_id);
        return false;
    }

    // Validate phase input (check for NaN, infinity)
    if (!isfinite(phase_deg)) {
        Serial.printf("PulseGenerator: ERROR - Invalid phase value (NaN or infinity) for channel %d\n", channel_id);
        return false;
    }

    // Normalize phase to 0-360 degrees using fmod (safer than while loop)
    phase_deg = fmodf(phase_deg, 360.0f);
    if (phase_deg < 0) {
        phase_deg += 360.0f;
    }

    mcpwm_timer_t timer = _timers[channel_id];

    // Calculate phase as percentage of period (0-999 range per ESP-IDF spec)
    float phase_percent = phase_deg / 360.0f;
    uint32_t phase_val = (uint32_t)(phase_percent * 999.0f);

    // Bounds check for ESP-IDF API (must be 0-999)
    if (phase_val > 999) {
        phase_val = 999;
        Serial.printf("PulseGenerator: WARNING - Phase value clamped to 999 for channel %d\n", channel_id);
    }

    // Store the phase delay for later retrieval
    _phase_delays_us[channel_id] = phaseToDelayUs(phase_deg, _period_us);
    _channels[channel_id].phase_offset_deg = phase_deg;

    // Set phase using mcpwm_sync_enable with phase parameter
    // Note: This sets the sync phase, not a persistent phase offset
    esp_err_t err = mcpwm_sync_enable(_mcpwm_unit, timer, MCPWM_SELECT_SYNC_INT0, phase_val);

    if (err != ESP_OK) {
        Serial.printf("PulseGenerator: ERROR - Failed to set phase for channel %d: %s\n",
                     channel_id, esp_err_to_name(err));
        return false;
    }

    Serial.printf("PulseGenerator: Channel %d phase offset set to %.1f° (val: %u, delay: %u us)\n",
                 channel_id, phase_deg, phase_val, _phase_delays_us[channel_id]);
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

// ===== EXACT PARAMETER METHOD IMPLEMENTATIONS =====

bool PulseGenerator::setPeriod(uint32_t period_us) {
    if (period_us < 25 || period_us > 1000000000) {
        Serial.printf("PulseGenerator: Invalid period %u us (must be 25-1000000000)\n", period_us);
        return false;
    }

    _period_us = period_us;
    _frequency = periodToFreqHz(period_us);

    Serial.printf("PulseGenerator: Setting period to %u us (Frequency: %.2f Hz)\n", period_us, _frequency);

    // Use setFrequency to apply the change (it will update _period_us again, but that's ok)
    return setFrequency(_frequency);
}

bool PulseGenerator::setPulseWidth(uint8_t channel_id, uint32_t pulse_width_us) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid channel ID %d\n", channel_id);
        return false;
    }

    // Validate period is set
    if (_period_us == 0) {
        Serial.printf("PulseGenerator: ERROR - Period not set, cannot set pulse width\n");
        return false;
    }

    // Validate pulse width range
    // Minimum: 1 us (hardware limitation)
    // Maximum: period (100% duty cycle)
    if (pulse_width_us > _period_us) {
        Serial.printf("PulseGenerator: ERROR - Pulse width %u us exceeds period %u us\n", pulse_width_us, _period_us);
        return false;
    }

    // Allow 0 pulse width (0% duty = OFF)
    // No minimum check needed since 0 is valid

    _pulse_widths_us[channel_id] = pulse_width_us;
    _duty_cycles[channel_id] = pulseWidthToDuty(pulse_width_us, _period_us);

    Serial.printf("PulseGenerator: Channel %d pulse width set to %u us (Duty: %.2f%%)\n",
                  channel_id, pulse_width_us, _duty_cycles[channel_id] * 100.0);

    // Apply the duty cycle to hardware
    return setDutyCycle(channel_id, _duty_cycles[channel_id]);
}

bool PulseGenerator::setAllPulseWidths(uint32_t pulse_width_us) {
    bool success = true;
    for (uint8_t i = 0; i < MAX_PULSE_CHANNELS; i++) {
        if (_channels[i].enabled) {
            success &= setPulseWidth(i, pulse_width_us);
        }
    }
    return success;
}

bool PulseGenerator::setParams(uint8_t channel_id, const PulseParams_t& params) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid channel ID %d\n", channel_id);
        return false;
    }

    Serial.printf("PulseGenerator: Setting comprehensive parameters for channel %d\n", channel_id);

    // Apply frequency/period
    if (params.paramMode & PARAM_USE_PERIOD) {
        setPeriod(params.periodUs);
    } else if (params.paramMode & PARAM_USE_FREQUENCY) {
        setFrequency(params.frequencyHz);
    }

    // Apply duty/pulse width
    if (params.paramMode & PARAM_USE_PULSE_WIDTH) {
        setPulseWidth(channel_id, params.pulseWidthUs);
    } else if (params.paramMode & PARAM_USE_DUTY_CYCLE) {
        setDutyCycle(channel_id, params.dutyCycle);
    }

    // Apply phase offset/delay
    if (params.paramMode & PARAM_USE_PHASE_TIME) {
        setPhaseDelay(channel_id, params.phaseDelayUs);
    } else if (params.paramMode & PARAM_USE_PHASE_DEGREES) {
        _applyPhaseOffset(channel_id, params.phaseOffsetDeg);
    }

    // Apply polarity
    setPolarity(channel_id, params.polarity);

    // Note: Pulse count is handled by SignalEngine, not PulseGenerator
    // Start delay and burst mode not yet implemented
    if (params.startDelayUs > 0) {
        Serial.println("PulseGenerator: Warning - startDelayUs not yet implemented");
    }
    if (params.pulseCount > 0) {
        Serial.println("PulseGenerator: Note - pulseCount is handled by SignalEngine (via START command)");
    }
    if (params.burstCount > 0) {
        Serial.println("PulseGenerator: Warning - burst mode not yet implemented");
    }

    return true;
}

bool PulseGenerator::setPhaseDelay(uint8_t channel_id, uint32_t delay_us) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid channel ID %d\n", channel_id);
        return false;
    }

    if (delay_us > _period_us) {
        Serial.printf("PulseGenerator: Warning - Phase delay %u us exceeds period %u us, wrapping around\n",
                     delay_us, _period_us);
        delay_us = delay_us % _period_us;
    }

    _phase_delays_us[channel_id] = delay_us;

    // Convert to degrees and apply
    float phase_deg = delayToPhaseDeg(delay_us, _period_us);
    _channels[channel_id].phase_offset_deg = phase_deg;

    Serial.printf("PulseGenerator: Channel %d phase delay set to %u us (%.1f°)\n",
                  channel_id, delay_us, phase_deg);

    return _applyPhaseOffset(channel_id, phase_deg);
}

bool PulseGenerator::setPolarity(uint8_t channel_id, SignalPolarity polarity) {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        Serial.printf("PulseGenerator: Invalid channel ID %d\n", channel_id);
        return false;
    }

    _polarities[channel_id] = polarity;

    Serial.printf("PulseGenerator: Channel %d polarity set to %s\n",
                  channel_id, polarity == POLARITY_ACTIVE_HIGH ? "ACTIVE_HIGH" : "ACTIVE_LOW");

    // Apply polarity to MCPWM hardware if channel is enabled
    if (_channels[channel_id].enabled) {
        mcpwm_timer_t timer = _timers[channel_id];
        mcpwm_generator_t gen = _generators[channel_id];

        // Set duty type based on polarity
        // MCPWM_DUTY_MODE_0 = Active High (output high during duty cycle)
        // MCPWM_DUTY_MODE_1 = Active Low (output low during duty cycle)
        mcpwm_duty_type_t duty_type = (polarity == POLARITY_ACTIVE_HIGH)
                                       ? MCPWM_DUTY_MODE_0
                                       : MCPWM_DUTY_MODE_1;

        esp_err_t err = mcpwm_set_duty_type(_mcpwm_unit, timer, gen, duty_type);
        if (err != ESP_OK) {
            Serial.printf("PulseGenerator: ERROR - Failed to set polarity for channel %d: %s\n",
                         channel_id, esp_err_to_name(err));
            return false;
        }

        Serial.printf("PulseGenerator: Polarity applied to MCPWM hardware (duty_type=%d)\n", duty_type);
    }

    return true;
}

uint32_t PulseGenerator::getPeriodUs() const {
    return _period_us;
}

uint32_t PulseGenerator::getPulseWidthUs(uint8_t channel_id) const {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return 0;
    }
    return _pulse_widths_us[channel_id];
}

uint32_t PulseGenerator::getPhaseDelayUs(uint8_t channel_id) const {
    if (channel_id >= MAX_PULSE_CHANNELS) {
        return 0;
    }
    return _phase_delays_us[channel_id];
}

// ===== STATUS INDICATOR IMPLEMENTATION =====

bool PulseGenerator::setIndicatorPin(uint8_t gpio_pin) {
    // Disable old indicator if set
    if (_indicator_pin > 0) {
        pinMode(_indicator_pin, INPUT);  // Reset to input
        digitalWrite(_indicator_pin, LOW);
    }

    _indicator_pin = gpio_pin;

    // Configure new indicator if not disabled
    if (_indicator_pin > 0) {
        pinMode(_indicator_pin, OUTPUT);
        // Set initial state based on running status
        digitalWrite(_indicator_pin, _is_running ? HIGH : LOW);
        Serial.printf("PulseGenerator: Status indicator configured on GPIO %d\n", _indicator_pin);
    } else {
        Serial.println("PulseGenerator: Status indicator disabled");
    }

    return true;
}
