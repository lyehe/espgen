# Duration vs Pulse Count Control

This document explains how to control pulse generation using **duration (time-based)** or **pulse count (exact number)**.

## Overview

You can specify how long pulses should run in **two ways**:

1. **Duration** (time-based) - Run for a specific amount of time
2. **Pulse Count** (count-based) - Generate an exact number of pulses

## Method 1: Duration (Time-Based) - Default

This is the **original behavior** and remains the **default** for backward compatibility.

### Using Duration

```cpp
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .channel = 0,
    .frequencyHz = 1000,         // 1 kHz
    .dutyCycle = 0.5,            // 50% duty
    .durationSec = 5.0,          // Run for 5 seconds
    .paramMode = 0               // Default mode (no flags = use duration)
};
engine.sendCommand(cmd);

// Result: Generates pulses for 5 seconds, then auto-stops
// At 1 kHz for 5 seconds = approximately 5000 pulses
```

### Infinite Duration

```cpp
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 1000,
    .dutyCycle = 0.5,
    .durationSec = 0,            // 0 = infinite (run until manually stopped)
};
engine.sendCommand(cmd);

// Result: Runs continuously until SIG_CMD_STOP is sent
```

## Method 2: Pulse Count (Count-Based) - New

Generate an **exact number of pulses**, regardless of frequency changes.

### Using Pulse Count

```cpp
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .channel = 0,
    .frequencyHz = 1000,         // 1 kHz
    .dutyCycle = 0.5,            // 50% duty
    .pulseCount = 5000,          // Generate exactly 5000 pulses
    .paramMode = PARAM_USE_PULSE_COUNT  // Use pulse count mode
};
engine.sendCommand(cmd);

// Result: Generates exactly 5000 pulses, then auto-stops
// At 1 kHz, this takes 5 seconds
// At 500 Hz, this takes 10 seconds
// At 2 kHz, this takes 2.5 seconds
```

### Infinite Pulse Count

```cpp
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 1000,
    .dutyCycle = 0.5,
    .pulseCount = 0,             // 0 = infinite (run until manually stopped)
    .paramMode = PARAM_USE_PULSE_COUNT
};
engine.sendCommand(cmd);

// Result: Runs continuously (same as infinite duration)
```

## Comparison

| Feature | Duration Mode | Pulse Count Mode |
|---------|---------------|------------------|
| Specifies | Time to run | Number of pulses |
| Auto-stop | After N seconds | After N pulses |
| Frequency change | Changes pulse count | Doesn't change pulse count |
| Precision | Time-based | Count-based |
| Use Case | Timed operations | Exact sequences |
| Default | ✅ Yes | No (requires flag) |

## Practical Examples

### Example 1: Camera Trigger Sequence

**Need:** Trigger camera exactly 100 times, regardless of timing

```cpp
// Using pulse count - EXACT 100 photos
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 2.0,          // 2 Hz (one photo every 500ms)
    .dutyCycle = 0.1,            // 50ms trigger pulse
    .pulseCount = 100,           // Exactly 100 photos
    .paramMode = PARAM_USE_PULSE_COUNT
};
engine.sendCommand(cmd);

// Result: Exactly 100 trigger pulses (50 seconds at 2 Hz)
// If frequency changes to 1 Hz, takes 100 seconds but still 100 pulses
```

### Example 2: Timed LED Flash

**Need:** Flash LED for 10 seconds

```cpp
// Using duration - TIME-BASED
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 5.0,          // 5 Hz (5 flashes/second)
    .dutyCycle = 0.5,            // 50% duty
    .durationSec = 10.0,         // Flash for 10 seconds
    // .paramMode = 0            // Default (duration mode)
};
engine.sendCommand(cmd);

// Result: Flashes for exactly 10 seconds (~50 flashes at 5 Hz)
// If frequency changes to 10 Hz during run, more flashes but still 10 seconds
```

### Example 3: Stepper Motor Exact Steps

**Need:** Move stepper motor exactly 1600 steps (one revolution)

```cpp
// Using pulse count - EXACT positioning
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 800,          // 800 steps/second
    .dutyCycle = 0.5,            // 50% duty step pulses
    .pulseCount = 1600,          // Exactly 1600 steps = 1 full rotation
    .paramMode = PARAM_USE_PULSE_COUNT
};
engine.sendCommand(cmd);

// Result: Motor rotates exactly one full turn (2 seconds at 800 Hz)
// Critical for positioning - pulse count never drifts
```

### Example 4: Ultrasonic Burst

**Need:** 10 pulses at 40 kHz for ultrasonic ranging

```cpp
// Using pulse count - EXACT burst
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 40000,        // 40 kHz ultrasonic
    .dutyCycle = 0.5,            // 50% duty
    .pulseCount = 10,            // Exactly 10 pulses
    .paramMode = PARAM_USE_PULSE_COUNT
};
engine.sendCommand(cmd);

// Result: 10 pulse burst (250 microseconds total)
// Then auto-stop, leaving time to listen for echo
```

### Example 5: Timed Alarm

**Need:** Beep for 3 seconds

```cpp
// Using duration - TIME-BASED
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 2000,         // 2 kHz tone
    .dutyCycle = 0.5,
    .durationSec = 3.0,          // Beep for 3 seconds
};
engine.sendCommand(cmd);

// Result: 3-second beep (exactly 6000 cycles)
```

## Conversion Between Modes

You can convert between duration and pulse count using helper functions:

```cpp
// Calculate pulse count from duration
double freq = 1000.0;  // 1 kHz
float duration = 5.0;   // 5 seconds
uint32_t pulses = durationToPulseCount(duration, freq);  // 5000 pulses

// Calculate duration from pulse count
uint32_t pulses = 5000;
double freq = 1000.0;
float duration = pulseCountToDuration(pulses, freq);  // 5.0 seconds
```

## Which Mode to Use?

**Use Duration (time-based) when:**
- ✅ You need timed operations (alarms, timeouts, delays)
- ✅ Exact timing is more important than exact count
- ✅ Backward compatibility with existing code
- ✅ User-facing durations ("flash for 5 seconds")

**Use Pulse Count (count-based) when:**
- ✅ You need exact number of operations (steps, photos, measurements)
- ✅ Positioning or sequencing is critical
- ✅ Frequency might change during operation
- ✅ Protocol requires exact pulse counts

## Implementation Details

### How Pulse Count Works

The SignalEngine tracks generated pulses using `getEstimatedCycleCount()`:

```cpp
void SignalEngine::loop() {
    if (_usePulseCount && _requestedPulseCount > 0) {
        uint64_t currentPulses = getEstimatedCycleCount();

        if (currentPulses >= _requestedPulseCount) {
            // Pulse count reached, auto-stop
            sendCommand({.type = SIG_CMD_STOP});
        }
    }
}
```

### Pulse Count Accuracy

- **Tracking Method:** Time-based calculation from start time and frequency
- **Accuracy:** ±1 pulse (depends on main loop() call frequency)
- **Overhead:** Minimal (simple comparison in loop)

### Edge Cases

**Changing Frequency During Run:**

```cpp
// Duration mode - duration stays constant, pulse count changes
cmd.durationSec = 10.0;        // Fixed 10 seconds
cmd.frequencyHz = 1000;        // Initially 1 kHz = 10,000 pulses
// Later: change to 2 kHz during run → 20,000 pulses in same 10 seconds

// Pulse count mode - pulse count stays constant, duration changes
cmd.pulseCount = 10000;        // Fixed 10,000 pulses
cmd.frequencyHz = 1000;        // Initially 1 kHz = 10 seconds
// Later: change to 2 kHz during run → 10,000 pulses in 5 seconds
```

**Manual Stop:**

Both modes can be manually stopped before completion:

```cpp
// Start with either mode
engine.sendCommand(startCmd);

// Later: manually stop (works for both modes)
engine.sendCommand({.type = SIG_CMD_STOP});
```

## Backward Compatibility

**All existing code continues to work** without changes:

```cpp
// Old code (no paramMode specified) - Uses duration mode
SignalCmd cmd = {
    .type = SIG_CMD_START,
    .frequencyHz = 1000,
    .dutyCycle = 0.5,
    .durationSec = 5.0,
    // No paramMode field = defaults to duration mode
};
// ✅ Works exactly as before
```

## Serial CLI Examples

```bash
# Duration mode (default)
start 1000 0.5 5.0      # 1 kHz, 50% duty, 5 seconds

# Future: CLI support for pulse count
# start 1000 0.5 -c 5000  # 1 kHz, 50% duty, 5000 pulses (not yet implemented)
```

## Summary

| Parameter | Duration Mode | Pulse Count Mode |
|-----------|---------------|------------------|
| **Field** | `.durationSec` | `.pulseCount` |
| **Flag** | Default (0) | `PARAM_USE_PULSE_COUNT` |
| **Value** | Seconds (float) | Pulses (uint64_t) |
| **Infinite** | 0 seconds | 0 pulses |
| **Auto-stop** | After time expires | After count reached |
| **Tracking** | `esp_timer_get_time()` | `getEstimatedCycleCount()` |

Choose the mode that best fits your application requirements!
