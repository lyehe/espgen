# Bug Report and Review Findings

## Critical Bugs (Data Corruption / Incorrect Behavior)

### 1. Union Aliasing Corruption in NVS Storage
**Files**: `SignalEngine.cpp:310, 325-329`, `signal_iface.h`
**Severity**: CRITICAL
**Description**: When using pulse count mode, `durationSec` contains garbage data (it's aliased with `pulseCount`), but the code saves `durationSec` to NVS, causing data corruption.

**Current Code**:
```cpp
// Line 310: If using PARAM_USE_PULSE_COUNT, durationSec is garbage!
engine->_lastAppliedDurationSec = receivedCmd.durationSec;
// Line 328: Saves corrupted data
preferences.putFloat(NVS_KEY_DUR, engine->_lastAppliedDurationSec);
```

**Fix**: Check `paramMode` before accessing union fields:
```cpp
if (receivedCmd.paramMode & PARAM_USE_DURATION) {
    engine->_lastAppliedDurationSec = receivedCmd.durationSec;
} else if (receivedCmd.paramMode & PARAM_USE_PULSE_COUNT) {
    engine->_lastAppliedPulseCount = receivedCmd.pulseCount;
}
```

---

### 2. Wrong Structure in PulseParams_t
**File**: `signal_iface.h:136-176`
**Severity**: CRITICAL
**Description**: Uses nested `struct` inside `union`, which defeats union aliasing - both fields are accessible simultaneously.

**Current Code**:
```cpp
union {
    struct {  // BUG: Both fields accessible simultaneously!
        double frequencyHz;
        uint32_t periodUs;
    };
};
```

**Fix**:
```cpp
union {
    double frequencyHz;
    uint32_t periodUs;
};
```

---

### 3. Type Mismatch in Helper Functions
**File**: `signal_iface.h:234, 245`
**Severity**: CRITICAL
**Description**: `pulseCount` is `uint64_t` but helper functions use `uint32_t`, causing truncation for counts > 4.3 billion.

**Current Code**:
```cpp
static inline uint32_t durationToPulseCount(float durationSec, double freqHz);
static inline float pulseCountToDuration(uint32_t pulseCount, double freqHz);
```

**Fix**:
```cpp
static inline uint64_t durationToPulseCount(float durationSec, double freqHz);
static inline float pulseCountToDuration(uint64_t pulseCount, double freqHz);
```

---

### 4. Race Conditions in loop()
**File**: `SignalEngine.cpp:158-187`
**Severity**: CRITICAL
**Description**: `loop()` reads shared variables without mutex protection while `cmdDispatcherTask` modifies them.

**Fix**: Add mutex protection or use atomic operations for shared state variables.

---

## Serious Bugs (Incorrect Calculations)

### 5. Pulse Width Corruption on Frequency Change
**File**: `PulseGenerator.cpp:120-154`
**Severity**: HIGH
**Description**: When frequency changes, exact pulse widths become invalid because they were calculated with the old period.

**Scenario**:
```
1. User sets exact pulse width of 500us at 1kHz (period 1000us) → duty = 0.5
2. Frequency changes to 2kHz (period 500us)
3. Code reapplies duty = 0.5 → pulse width becomes 250us (WRONG!)
   User expected 500us to be preserved
```

**Fix**: Track which parameter is "primary" (duty vs pulse width) and preserve it during frequency changes.

---

### 6. Tick Accumulation with Wrong Frequency
**File**: `SignalEngine.cpp:339-343`
**Severity**: HIGH
**Description**: When START is received while running, ticks are accumulated with OLD frequency before new frequency is applied.

**Current Code**:
```cpp
uint64_t cycles_just_elapsed = calculateCycles(..., engine->_currentFrequencyHz);  // OLD
engine->_accumulatedTicks += cycles_just_elapsed;
engine->pulseGen.setFrequency(receivedCmd.frequencyHz);  // NEW
```

**Fix**: This is actually correct - we want to calculate elapsed cycles with the frequency that was active during that time period. Not a bug.

---

## Moderate Bugs (Missing Features)

### 7. Exact Parameters Not Handled in START Command
**File**: `SignalEngine.cpp:305-385`
**Severity**: MODERATE
**Description**: START command handler doesn't check `PARAM_USE_PERIOD`, `PARAM_USE_PULSE_WIDTH`, `PARAM_USE_PHASE_TIME` flags.

**Impact**: Can't use exact microsecond parameters via START command, only frequency/duty.

**Fix**: Add parameter mode checking:
```cpp
if (receivedCmd.paramMode & PARAM_USE_PERIOD) {
    engine->pulseGen.setPeriod(receivedCmd.periodUs);
} else if (receivedCmd.paramMode & PARAM_USE_FREQUENCY) {
    engine->pulseGen.setFrequency(receivedCmd.frequencyHz);
}
```

---

### 8. Useless normalizeSignalCmd()
**File**: `param_helpers.h:128-147`
**Severity**: LOW
**Description**: Function calculates conversions but can't store them due to unions.

**Fix**: Remove the function or make it actually useful by converting IN PLACE:
```cpp
static inline void normalizeSignalCmd(SignalCmd* cmd) {
    // If frequency is set, also calculate and overwrite period
    if (cmd->paramMode & PARAM_USE_FREQUENCY) {
        // Convert to period mode
        cmd->periodUs = freqToPeriodUs(cmd->frequencyHz);
        cmd->paramMode = (cmd->paramMode & ~PARAM_USE_FREQUENCY) | PARAM_USE_PERIOD;
    }
    // etc...
}
```

---

### 9. Incomplete Validation
**File**: `param_helpers.h:155-193`
**Severity**: LOW
**Description**: `validateSignalCmd()` missing critical checks.

**Missing Checks**:
- Zero period when `PARAM_USE_PERIOD` is set
- Zero frequency when `PARAM_USE_FREQUENCY` is set
- Pulse width > period
- Negative phase delay

---

## Clean Structure Issues

### 10. Inconsistent Parameter Synchronization
**File**: `PulseGenerator.cpp:157-270`
**Description**: When enabling a channel, only duty cycle is restored, not exact pulse width.

**Current Code** (line 266):
```cpp
} else if (enabled && _is_running) {
    setDutyCycle(channel_id, _duty_cycles[channel_id]);  // What if pulse width was set?
}
```

**Fix**: Track which parameter is primary and restore the correct one.

---

### 11. Redundant Calculations
**File**: `PulseGenerator.cpp:417-430`
**Description**: `setPeriod()` calls `setFrequency()` which recalculates period.

**Current Code**:
```cpp
_period_us = period_us;
_frequency = periodToFreqHz(period_us);
return setFrequency(_frequency);  // This recalculates _period_us again!
```

**Fix**: Apply period directly to hardware without round-trip conversion.

---

## Summary

- **Critical Bugs**: 4 (must fix immediately)
- **Serious Bugs**: 1 confirmed, 1 false positive
- **Moderate Bugs**: 3
- **Structure Issues**: 2

**Priority**: Fix critical bugs first, especially union aliasing corruption and type mismatches.
