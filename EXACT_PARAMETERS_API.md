# Exact Parameters API

This document describes the comprehensive parameter API for precise control of PWM/trigger signals.

## Overview

The pulse generator now supports **exact parameter specification** using multiple methods:
- **Frequency OR Period** - Specify timing in Hz or microseconds
- **Duty Cycle OR Pulse Width** - Specify as percentage or exact microseconds
- **Duration OR Pulse Count** - Specify run time or exact number of pulses
- **Phase in Degrees OR Time Delay** - Specify offset as degrees or microseconds
- **Signal Polarity** - Active high or active low
- **Start Delay** - Delay before first pulse (future feature)

## Parameter Specification Methods

### Method 1: Legacy API (Backward Compatible)

```cpp
// Using frequency (Hz) and duty cycle (0.0-1.0)
pulseGen.setFrequency(1000);       // 1 kHz
pulseGen.setDutyCycle(0, 0.5);     // 50% duty cycle
```

### Method 2: Exact Microsecond Timing

```cpp
// Using period and pulse width in microseconds
pulseGen.setPeriod(1000);           // 1000 us = 1 ms period = 1 kHz
pulseGen.setPulseWidth(0, 100);     // 100 us pulse width = 10% duty

// For slave channels with precise time delays
pulseGen.configureSlave(1, 19, 0.0, true);
pulseGen.setPhaseDelay(1, 2500);    // 2.5 ms delay
```

### Method 3: Comprehensive Parameters Structure

```cpp
// Define exact parameters for a channel
PulseParams_t params = {
    .periodUs = 10000,              // 10 ms period (100 Hz)
    .pulseWidthUs = 500,            // 500 us pulse width (5% duty)
    .phaseDelayUs = 2500,           // 2.5 ms phase delay
    .startDelayUs = 1000,           // 1 ms start delay (future)
    .paramMode = PARAM_USE_PERIOD | PARAM_USE_PULSE_WIDTH | PARAM_USE_PHASE_TIME,
    .polarity = POLARITY_ACTIVE_HIGH,
    .pulseCount = 1000,             // Generate exactly 1000 pulses (future)
    .burstCount = 0,                // Continuous mode
    .burstPeriodUs = 0
};

pulseGen.setParams(1, params);      // Apply to slave channel 1
```

## Parameter Modes (Bitflags)

Use `paramMode` to indicate which parameters to use:

```cpp
// Timing specification
PARAM_USE_FREQUENCY     // Use frequencyHz field
PARAM_USE_PERIOD        // Use periodUs field instead

// Pulse width specification
PARAM_USE_DUTY_CYCLE    // Use dutyCycle field (0.0-1.0)
PARAM_USE_PULSE_WIDTH   // Use pulseWidthUs field instead

// Duration specification
PARAM_USE_DURATION      // Use durationSec field
PARAM_USE_PULSE_COUNT   // Use pulseCount field instead (future)

// Phase specification
PARAM_USE_PHASE_DEGREES // Use phaseOffset field (degrees)
PARAM_USE_PHASE_TIME    // Use phaseDelayUs field instead

// Example: Use period and pulse width
params.paramMode = PARAM_USE_PERIOD | PARAM_USE_PULSE_WIDTH;
```

## Complete API Reference

### Exact Timing Methods

```cpp
// Period (alternative to frequency)
bool setPeriod(uint32_t period_us);
uint32_t getPeriodUs() const;

// Pulse width (alternative to duty cycle)
bool setPulseWidth(uint8_t channel_id, uint32_t pulse_width_us);
bool setAllPulseWidths(uint32_t pulse_width_us);
uint32_t getPulseWidthUs(uint8_t channel_id) const;

// Phase delay (alternative to degrees)
bool setPhaseDelay(uint8_t channel_id, uint32_t delay_us);
uint32_t getPhaseDelayUs(uint8_t channel_id) const;

// Polarity control
bool setPolarity(uint8_t channel_id, SignalPolarity polarity);

// Comprehensive parameter setting
bool setParams(uint8_t channel_id, const PulseParams_t& params);
```

### Helper Functions (in signal_iface.h)

```cpp
// Frequency <-> Period conversion
uint32_t freqToPeriodUs(double freqHz);
double periodToFreqHz(uint32_t periodUs);

// Duty Cycle <-> Pulse Width conversion
uint32_t dutyToPulseWidthUs(float dutyCycle, uint32_t periodUs);
float pulseWidthToDuty(uint32_t pulseWidthUs, uint32_t periodUs);

// Duration <-> Pulse Count conversion
uint32_t durationToPulseCount(float durationSec, double freqHz);
float pulseCountToDuration(uint32_t pulseCount, double freqHz);

// Phase Degrees <-> Time Delay conversion
uint32_t phaseToDelayUs(float phaseDeg, uint32_t periodUs);
float delayToPhaseDeg(uint32_t phaseDelayUs, uint32_t periodUs);
```

## Practical Examples

### Example 1: Exact Camera Trigger Timing

```cpp
// Need: 100 Hz triggers, 1 ms pulse, camera 2 delayed 5.5 ms

// Master camera
pulseGen.configureMaster(18);
pulseGen.setPeriod(10000);           // 10 ms period = 100 Hz
pulseGen.setPulseWidth(0, 1000);     // 1 ms pulse

// Slave camera with exact 5.5 ms delay
pulseGen.configureSlave(1, 19, 0.0, true);
pulseGen.setPhaseDelay(1, 5500);     // 5.5 ms delay
pulseGen.setPulseWidth(1, 1000);     // 1 ms pulse

pulseGen.start();
```

### Example 2: Laser Pulse with Exact Width

```cpp
// Need: Single 250 ns pulse at 1 MHz rate

pulseGen.configureMaster(18);
pulseGen.setPeriod(1000);            // 1 us period = 1 MHz
pulseGen.setPulseWidth(0, 0.25);     // 250 ns pulse (0.25 us)

// Note: MCPWM resolution may limit sub-microsecond precision
// Actual pulse width depends on timer resolution
```

### Example 3: Multi-Channel with Different Widths

```cpp
// Different pulse widths on each channel

pulseGen.setFrequency(10000);        // 10 kHz for all channels

pulseGen.setPulseWidth(0, 10);       // Master: 10 us pulse
pulseGen.setPulseWidth(1, 20);       // Slave 1: 20 us pulse
pulseGen.setPulseWidth(2, 5);        // Slave 2: 5 us pulse

// All synchronized, different pulse widths
```

### Example 4: Precise Stepper Motor Timing

```cpp
// Stepper motor: 1600 steps/rev, 60 RPM = 1600 steps/sec
// Step pulse: 5 us minimum width

double stepsPerSec = 1600.0;
uint32_t periodUs = freqToPeriodUs(stepsPerSec);  // 625 us

pulseGen.setPeriod(periodUs);
pulseGen.setPulseWidth(0, 5);        // 5 us step pulse

// For multi-axis with phase offset
pulseGen.setPhaseDelay(1, 312);      // Axis 2: 312 us delay (half period)
pulseGen.setPulseWidth(1, 5);
```

## Parameter Validation and Constraints

### Period Constraints

```cpp
// Period range: 25 us to 1,000,000,000 us (1 MHz to 0.001 Hz)
setPeriod(25);          // ✅ OK - 40 kHz
setPeriod(1000);        // ✅ OK - 1 kHz
setPeriod(1000000);     // ✅ OK - 1 Hz
setPeriod(10);          // ❌ Error - too fast
```

### Pulse Width Constraints

```cpp
// Pulse width must be ≤ period
setPulseWidth(0, 1000);  // ✅ OK if period ≥ 1000 us
setPulseWidth(0, 500);   // ✅ OK if period ≥ 500 us
setPulseWidth(0, 2000);  // ❌ Error if period < 2000 us
```

### Phase Delay Constraints

```cpp
// Phase delay wraps around if > period
setPhaseDelay(1, 500);   // ✅ OK - 500 us delay
setPhaseDelay(1, 15000); // ⚠️ Wraps to (15000 % period) us
```

## Conversion Examples

### Convert Between Representations

```cpp
// You have: 2.5 kHz, 40% duty, want exact microseconds
double freq = 2500.0;
float duty = 0.40;

uint32_t period_us = freqToPeriodUs(freq);        // 400 us
uint32_t pulse_us = dutyToPulseWidthUs(duty, period_us);  // 160 us

// Apply exact timing
pulseGen.setPeriod(period_us);
pulseGen.setPulseWidth(0, pulse_us);

// Verify values
Serial.printf("Period: %u us, Pulse: %u us\n",
              pulseGen.getPeriodUs(),
              pulseGen.getPulseWidthUs(0));
// Output: Period: 400 us, Pulse: 160 us
```

### Calculate Required Phase Delay

```cpp
// Want slave delayed by 1/4 period (90°)
uint32_t period_us = 1000;  // 1 ms period
float phase_deg = 90.0;

uint32_t delay_us = phaseToDelayUs(phase_deg, period_us);  // 250 us

pulseGen.setPhaseDelay(1, delay_us);
```

## Future Features (Not Yet Implemented)

These parameters are defined but not yet fully implemented:

```cpp
// Exact pulse count (alternative to duration in seconds)
params.pulseCount = 1000;           // Generate exactly 1000 pulses
params.paramMode |= PARAM_USE_PULSE_COUNT;

// Start delay (delay before first pulse)
params.startDelayUs = 5000;         // Wait 5 ms before starting

// Burst mode (groups of pulses with gaps)
params.burstCount = 10;             // 10 pulses per burst
params.burstPeriodUs = 50000;       // 50 ms between bursts

// Polarity (currently accepted but not applied to hardware)
params.polarity = POLARITY_ACTIVE_LOW;  // Inverted pulses
```

To use pulse count now, use SignalEngine's duration feature:
```cpp
// Generate 1000 pulses at 1 kHz (1 second)
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 1000.0,
    .dutyCycle = 0.5,
    .durationSec = 1.0  // 1000 pulses at 1 kHz
};
```

## Hardware Precision Notes

### MCPWM Resolution

The ESP32 MCPWM peripheral has hardware limitations:
- **Frequency range**: ~1 Hz to 40 MHz
- **Period resolution**: Depends on timer frequency
- **Minimum pulse width**: ~25 ns (theoretical), ~1 us (practical)
- **Phase resolution**: ~1-10 us depending on frequency

### Timing Accuracy

```cpp
// High precision at low frequencies
setPeriod(100000);      // 100 ms - very accurate
setPulseWidth(0, 5000); // 5 ms - accurate to ~1 us

// Lower precision at high frequencies
setPeriod(25);          // 25 us (40 kHz) - accurate to ~100 ns
setPulseWidth(0, 2);    // 2 us - may be rounded

// Sub-microsecond timing has limitations
setPulseWidth(0, 0.5);  // 500 ns - may not be achievable
```

### Verification

Always verify actual timing with an oscilloscope:
```cpp
pulseGen.setPulseWidth(0, 1);  // Request 1 us
Serial.printf("Requested: 1 us, Actual: %u us\n", pulseGen.getPulseWidthUs(0));
// Measure with oscilloscope to confirm actual pulse width
```

## Migration from Legacy API

Old code using frequency/duty cycle continues to work:

```cpp
// Old API - still works
pulseGen.setFrequency(1000);
pulseGen.setDutyCycle(0, 0.5);

// Internally converted to exact values
uint32_t period = pulseGen.getPeriodUs();      // 1000 us
uint32_t width = pulseGen.getPulseWidthUs(0);  // 500 us
```

New code can mix both styles:
```cpp
// Set frequency first (convenience)
pulseGen.setFrequency(1000);

// Then adjust pulse width precisely
pulseGen.setPulseWidth(0, 473);  // Exact 473 us pulse

// Check effective duty cycle
float duty = pulseGen.getDutyCycle(0);  // ~0.473 (47.3%)
```

## Summary

| Feature | Method 1 (Legacy) | Method 2 (Exact) | Method 3 (Params) |
|---------|-------------------|------------------|-------------------|
| Frequency | `setFrequency(Hz)` | `setPeriod(us)` | `params.periodUs` |
| Duty | `setDutyCycle(0-1)` | `setPulseWidth(us)` | `params.pulseWidthUs` |
| Phase | `degrees` in config | `setPhaseDelay(us)` | `params.phaseDelayUs` |
| Control | Simple, high-level | Precise, low-level | Comprehensive |
| Use Case | General PWM | Exact timing | Complex setups |

Choose the method that best fits your precision requirements!
