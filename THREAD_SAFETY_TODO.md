# Thread Safety - Critical Issue Requiring Future Fix

## 🔴 CRITICAL: No Thread Safety Protection

**Status:** DOCUMENTED BUT NOT YET FIXED
**Severity:** CRITICAL
**Priority:** MUST FIX BEFORE PRODUCTION USE

---

## Problem Summary

The `SignalEngine` class has **NO thread safety protection** for shared variables accessed by both:
- `loop()` (called from main task)
- `cmdDispatcherTask()` (FreeRTOS task)

This WILL cause data corruption and unpredictable behavior.

---

## Affected Variables (8 Total)

All accessed without mutex/atomic protection:

| Variable | Type | Size | Issue |
|----------|------|------|-------|
| `_accumulatedTicks` | uint64_t | 8 bytes | Data tearing on ESP32 (32-bit) |
| `_requestedPulseCount` | uint64_t | 8 bytes | Data tearing |
| `_startTimeMicros` | uint64_t | 8 bytes | Data tearing |
| `_durationStartTimeMicros` | uint64_t | 8 bytes | Data tearing |
| `_currentFrequencyHz` | double | 8 bytes | Data tearing |
| `_isRunning` | bool | 1 byte | Lost writes |
| `_usePulseCount` | bool | 1 byte | Lost writes |
| `_requestedDurationSec` | float | 4 bytes | Potential corruption |

---

## Why This is Critical

### ESP32 is 32-bit

Reading/writing 64-bit values requires TWO instructions:
```
// Writing uint64_t value = 0x123456789ABCDEF0
MOV [addr], 0x9ABCDEF0      // Lower 32 bits
MOV [addr+4], 0x12345678    // Upper 32 bits
```

If `loop()` reads between these two writes, it gets corrupted data:
- Lower 32 bits: NEW value
- Upper 32 bits: OLD value
- Result: Garbage

---

## Example Race Condition

```cpp
// cmdDispatcherTask() writes:
engine->_requestedPulseCount = 5000;
// Instruction 1: Write 5000 to lower 32 bits
// <<< INTERRUPT: loop() runs here >>>
// Instruction 2: Write 0 to upper 32 bits

// loop() reads (between instructions):
if (currentPulses >= _requestedPulseCount) {  // Reads corrupted value!
    // Might see 4294972296 instead of 5000
}
```

---

## Required Fix

### Option 1: Mutex Protection (Recommended)

Add FreeRTOS mutex:

```cpp
// In SignalEngine.h - Add member:
SemaphoreHandle_t _stateMutex;

// In SignalEngine::begin():
_stateMutex = xSemaphoreCreateMutex();
if (_stateMutex == NULL) {
    Serial.println("ERROR: Failed to create state mutex!");
    return;
}

// In SignalEngine::loop():
void SignalEngine::loop() {
    if (xSemaphoreTake(_stateMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; // Skip if can't get lock quickly
    }

    // ... access shared variables ...

    xSemaphoreGive(_stateMutex);
}

// In cmdDispatcherTask (each case):
case SIG_CMD_START:
    if (xSemaphoreTake(engine->_stateMutex, portMAX_DELAY) == pdTRUE) {
        // ... modify shared variables ...
        xSemaphoreGive(engine->_stateMutex);
    }
    break;
```

**Pros:**
- Standard FreeRTOS approach
- Clean and well-understood
- Works for all variable types

**Cons:**
- Adds overhead (mutex lock/unlock)
- Risk of priority inversion
- Must carefully avoid deadlocks

---

### Option 2: Critical Sections (For Short Operations)

```cpp
// Faster than mutex, but disables interrupts
taskENTER_CRITICAL();
uint64_t count = _requestedPulseCount;  // Atomic read
taskEXIT_CRITICAL();
```

**Pros:**
- Very fast
- No blocking

**Cons:**
- Disables interrupts (bad for long operations)
- Not suitable for loop()

---

### Option 3: Atomic Variables (Partial Solution)

```cpp
#include <atomic>

std::atomic<bool> _isRunning;
std::atomic<bool> _usePulseCount;
std::atomic<uint64_t> _requestedPulseCount;
std::atomic<uint64_t> _accumulatedTicks;
```

**Pros:**
- No blocking
- Very fast
- Lock-free

**Cons:**
- Requires C++11
- Doesn't protect composite operations (read-modify-write)
- Not all ESP-IDF versions support std::atomic

---

## Implementation Plan

### Phase 1: Add Mutex
1. Add `SemaphoreHandle_t _stateMutex` to SignalEngine.h
2. Create mutex in `begin()`
3. Wrap all shared variable access in loop()
4. Wrap all shared variable access in cmdDispatcherTask()

### Phase 2: Test Thoroughly
1. Run for extended periods (hours)
2. Test concurrent commands + loop()
3. Test high-frequency pulse counting
4. Verify no deadlocks
5. Check for priority inversion

### Phase 3: Optimize
1. Profile mutex overhead
2. Consider critical sections for very short reads
3. Add timeout handling
4. Document locking order

---

## Estimated Effort

- Implementation: 2-3 hours
- Testing: 4-6 hours
- **Total: 6-9 hours**

---

## Workarounds (Temporary)

Until fixed, minimize risk by:

1. **Don't use pulse count mode** - Duration mode is less affected
2. **Don't change frequency while running** - Reduces race window
3. **Don't send rapid commands** - Give time between commands
4. **Test extensively** - Catch corruption early

**But these are NOT solutions** - the bug will still occasionally occur.

---

## Why Not Fixed Yet

Thread safety requires:
1. Careful mutex placement (avoid deadlocks)
2. Extensive testing (race conditions are non-deterministic)
3. Performance validation (ensure no excessive overhead)
4. Integration with existing code paths

This is a substantial change that needs dedicated focus and testing.

---

## Next Steps

1. Create separate branch for thread-safety fixes
2. Implement mutex protection
3. Run extended stress tests
4. Merge after validation

**Target:** Fix in next iteration after current critical bugs are resolved.
