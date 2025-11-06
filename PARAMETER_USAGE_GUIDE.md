# Parameter Usage Guide - Corrected After Bug Fixes

## Overview

The pulse generator supports multiple ways to specify the same parameter. This guide shows the **correct** way to use parameter aliasing after bug fixes.

## Union Aliasing - How It Works

Parameters are stored in unions to save memory. **Only one field in each union should be used at a time**, selected by the `paramMode` bitflags.

```cpp
// Timing: Choose ONE
union {
    double frequencyHz;     // Use with PARAM_USE_FREQUENCY
    uint32_t periodUs;      // Use with PARAM_USE_PERIOD
};

// Pulse width: Choose ONE
union {
    float dutyCycle;        // Use with PARAM_USE_DUTY_CYCLE (0.0-1.0)
    uint32_t pulseWidthUs;  // Use with PARAM_USE_PULSE_WIDTH (microseconds)
};

// Run time: Choose ONE
union {
    float durationSec;      // Use with PARAM_USE_DURATION (seconds)
    uint64_t pulseCount;    // Use with PARAM_USE_PULSE_COUNT (exact count)
};

// Phase: Choose ONE
union {
    float phaseOffset;      // Use with PARAM_USE_PHASE_DEGREES (0-360)
    uint32_t phaseDelayUs;  // Use with PARAM_USE_PHASE_TIME (microseconds)
};
```

## Correct Usage Examples

### Example 1: Frequency + Duty Cycle + Duration (Classic Mode)
```cpp
#include "param_helpers.h"

// 1 kHz, 50% duty, run for 5 seconds
SignalCmd cmd = createStartCmd_FreqDuty(1000.0, 0.5, 5.0);
engine.sendCommand(cmd);

// Flags automatically set:
// PARAM_USE_FREQUENCY | PARAM_USE_DUTY_CYCLE | PARAM_USE_DURATION
```

### Example 2: Period + Pulse Width + Duration (Exact Timing)
```cpp
// 1000us period, 500us pulse width, run for 5 seconds
SignalCmd cmd = createStartCmd_PeriodWidth(1000, 500, 5.0);
engine.sendCommand(cmd);

// Flags automatically set:
// PARAM_USE_PERIOD | PARAM_USE_PULSE_WIDTH | PARAM_USE_DURATION
```

### Example 3: Frequency + Duty Cycle + Pulse Count (Exact Count)
```cpp
// 1 kHz, 50% duty, exactly 5000 pulses
SignalCmd cmd = createStartCmd_PulseCount(1000.0, 0.5, 5000);
engine.sendCommand(cmd);

// Flags automatically set:
// PARAM_USE_FREQUENCY | PARAM_USE_DUTY_CYCLE | PARAM_USE_PULSE_COUNT
```

### Example 4: Continuous Operation (Infinite)
```cpp
// 1 kHz, 50% duty, run forever
SignalCmd cmd = createStartCmd_FreqDuty(1000.0, 0.5, 0.0);  // 0.0 = infinite
engine.sendCommand(cmd);
```

### Example 5: Manual Command Construction (Advanced)
```cpp
SignalCmd cmd = {0};  // Zero-initialize
cmd.type = SIG_CMD_START;
cmd.channel = 0;

// Use exact period (microseconds)
cmd.periodUs = 2000;  // 2000us = 500 Hz
cmd.paramMode = PARAM_USE_PERIOD;

// Use exact pulse width (microseconds)
cmd.pulseWidthUs = 800;  // 800us pulse width
cmd.paramMode |= PARAM_USE_PULSE_WIDTH;

// Use exact pulse count
cmd.pulseCount = 10000;  // Exactly 10,000 pulses
cmd.paramMode |= PARAM_USE_PULSE_COUNT;

cmd.polarity = POLARITY_ACTIVE_HIGH;

// Validate before sending (recommended)
if (validateSignalCmd(&cmd)) {
    engine.sendCommand(cmd);
} else {
    Serial.println("Invalid command parameters!");
}
```

### Example 6: Configuring Slave Channels with Phase Delay
```cpp
// Configure master channel (channel 0) on GPIO 12
engine.sendCommand({
    .type = SIG_CMD_CONFIG_CHANNEL,
    .channel = 0,
    .pin = 12,
    .enabled = true,
    .paramMode = 0
});

// Configure slave channel 1 with 90-degree phase offset
SignalCmd cmd = createConfigChannel_PhaseDegrees(1, 13, 90.0, true);
engine.sendCommand(cmd);

// Configure slave channel 2 with 500us phase delay
SignalCmd cmd2 = createConfigChannel_TimeDelay(2, 14, 500, true);
engine.sendCommand(cmd2);
```

## Critical Rules (After Bug Fixes)

### ✅ DO:
1. **Always set `paramMode` flags** to indicate which union field is active
2. **Use helper functions** (`createStartCmd_*`) to avoid mistakes
3. **Validate commands** with `validateSignalCmd()` before sending
4. **Choose ONE field per union** (frequency OR period, not both)

### ❌ DON'T:
1. **Don't set both frequency AND period** in the same command
2. **Don't set both dutyCycle AND pulseWidthUs** in the same command
3. **Don't set both durationSec AND pulseCount** in the same command
4. **Don't access union fields without checking `paramMode`**

## What Changed in Bug Fixes

### 1. Fixed Type Mismatches
- `durationToPulseCount()` now returns `uint64_t` (was `uint32_t`)
- `pulseCountToDuration()` now accepts `uint64_t` (was `uint32_t`)
- Supports pulse counts > 4.3 billion

### 2. Fixed Union Aliasing Corruption
- START command handler now checks `paramMode` before accessing unions
- NVS storage now saves correct values based on mode
- Prevents garbage data corruption

### 3. Fixed PulseParams_t Structure
- Removed nested `struct` inside unions
- Now uses proper union aliasing (one field at a time)

### 4. Enhanced Validation
- Added range checks for period, phase, duration
- Added conflict detection for all parameter pairs
- Added bounds checking for all numeric ranges

### 5. Exact Parameter Support in START
- START command now handles `PARAM_USE_PERIOD`
- START command now handles `PARAM_USE_PULSE_WIDTH`
- START command now handles `PARAM_USE_PHASE_TIME`

## Conversion Between Representations

### Frequency ↔ Period
```cpp
uint32_t periodUs = freqToPeriodUs(1000.0);      // 1kHz → 1000us
double freqHz = periodToFreqHz(1000);             // 1000us → 1kHz
```

### Duty Cycle ↔ Pulse Width
```cpp
uint32_t pulseWidthUs = dutyToPulseWidthUs(0.5, 1000);  // 50% of 1000us → 500us
float duty = pulseWidthToDuty(500, 1000);               // 500us / 1000us → 0.5
```

### Duration ↔ Pulse Count
```cpp
uint64_t pulses = durationToPulseCount(5.0, 1000.0);  // 5s @ 1kHz → 5000 pulses
float duration = pulseCountToDuration(5000, 1000.0);   // 5000 pulses @ 1kHz → 5s
```

### Phase Degrees ↔ Phase Delay
```cpp
uint32_t delayUs = phaseToDelayUs(90.0, 1000);  // 90° of 1000us → 250us
float degrees = delayToPhaseDeg(250, 1000);      // 250us / 1000us → 90°
```

## Parameter Ranges

| Parameter | Range | Notes |
|-----------|-------|-------|
| `frequencyHz` | 0.001 - 40,000,000 Hz | Must be positive |
| `periodUs` | 25 - 1,000,000,000 us | 25us to 1000s |
| `dutyCycle` | 0.0 - 1.0 | 0% to 100% |
| `pulseWidthUs` | 0 - periodUs | Cannot exceed period |
| `durationSec` | 0.0 - ∞ | 0 = infinite |
| `pulseCount` | 0 - 2^64-1 | 0 = infinite |
| `phaseOffset` | 0.0 - 360.0 | Degrees |
| `phaseDelayUs` | 0 - periodUs | Wraps around if > period |

## Common Mistakes (Now Fixed)

### ❌ Before Bug Fixes
```cpp
// BUG: Would corrupt NVS if using pulse count
SignalCmd cmd = {0};
cmd.pulseCount = 5000;
cmd.paramMode = PARAM_USE_PULSE_COUNT;
// durationSec contains garbage! NVS corruption!
```

### ✅ After Bug Fixes
```cpp
// CORRECT: Handler checks paramMode before accessing union
SignalCmd cmd = createStartCmd_PulseCount(1000.0, 0.5, 5000);
// Only pulseCount is accessed, durationSec is properly zeroed
```

## Testing Your Commands

```cpp
SignalCmd cmd = createStartCmd_FreqDuty(1000.0, 0.5, 5.0);

// Validate before sending
if (!validateSignalCmd(&cmd)) {
    Serial.println("ERROR: Invalid command!");
    return;
}

// Send command
if (!engine.sendCommand(cmd)) {
    Serial.println("ERROR: Command queue full!");
    return;
}

Serial.println("Command sent successfully!");
```

## Summary

The bug fixes ensure:
- ✅ No union aliasing corruption
- ✅ Correct type sizes (uint64_t for pulse counts)
- ✅ Proper paramMode flag checking
- ✅ Full support for exact parameters
- ✅ Enhanced validation
- ✅ Clean, predictable behavior

**Always use the helper functions when possible** - they set up the parameters correctly and minimize errors.
