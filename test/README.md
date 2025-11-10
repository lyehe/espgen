# Unit Testing for ESP32 Signal Generator

## Overview

This directory contains unit tests for the Signal Engine components. The testing infrastructure uses PlatformIO's native testing framework with Unity test runner.

## Test Structure

```
test/
├── mocks/                      # Mock implementations for ESP32 dependencies
│   ├── mocks.h                 # Main mock header (MockPreferences, MockPulseGenerator)
│   ├── mocks.cpp               # Mock implementations
│   ├── Arduino.h               # Mock Arduino framework
│   ├── Preferences.h           # Mock NVS/Preferences
│   ├── esp_event.h             # Mock ESP event system
│   ├── esp_timer.h             # Mock ESP timer
│   ├── freertos/               # Mock FreeRTOS headers
│   │   ├── FreeRTOS.h
│   │   ├── semphr.h
│   │   ├── task.h
│   │   └── queue.h
│   └── driver/                 # Mock ESP-IDF drivers
│       ├── mcpwm.h
│       └── gpio.h
└── test_timing_controller/     # TimingController unit tests
    └── test_timing_controller.cpp
```

## Mock Infrastructure

### Mocks Provided

1. **MockSerial** - Arduino Serial communication
2. **MockPreferences** - NVS (Non-Volatile Storage) with in-memory storage
3. **MockPulseGenerator** - Hardware pulse generation abstraction
4. **FreeRTOS** - Task, queue, mutex, semaphore mocks
5. **ESP-IDF** - Timer, event system, GPIO, MCPWM mocks

### Mock Features

- **Controllable Time**: Tests can control `esp_timer_get_time()` by defining their own implementation
- **In-Memory NVS**: MockPreferences stores data in std::map for testing persistence logic
- **State Inspection**: Mock classes provide test accessors (e.g., `MockPulseGenerator::isRunning()`)

## Test Cases

### test_timing_controller.cpp

Comprehensive tests for `TimingController` class:

- **calculateCycles() Tests** (6 tests)
  - Basic cycle calculations
  - Fractional cycles (truncation behavior)
  - High frequency signals
  - Time offset handling
  - Edge cases (zero time, negative values)
  - Overflow protection

- **onStart/onStop/onFrequencyChange Tests** (6 tests)
  - Accumulator reset vs. preservation
  - Cycle accumulation on stop
  - Frequency change with old frequency tracking

- **getEstimatedCycleCount Tests** (3 tests)
  - Stopped state
  - Running state with accumulation
  - Edge cases

- **checkTimeouts - Duration Tests** (5 tests)
  - No timeout when stopped
  - Duration not expired
  - Duration expired (exactly and exceeded)
  - Infinite duration (duration = 0)

- **checkTimeouts - Pulse Count Tests** (5 tests)
  - Pulse count not reached
  - Pulse count reached (exactly and exceeded)
  - Pulse count with accumulator
  - Infinite pulse count (count = 0)

- **checkTimeouts - Priority Tests** (2 tests)
  - Pulse count has priority over duration
  - Duration ignored when using pulse count mode

**Total**: 27 test cases for TimingController

## Running Tests

### Prerequisites

```bash
pip install platformio
```

### Run All Native Tests

```bash
platformio test -e native
```

### Run Specific Test

```bash
platformio test -e native -f test_timing_controller
```

### Verbose Output

```bash
platformio test -e native -v
```

## Current Status

### ✅ All Tests Passing!

**27/27 tests passing** for TimingController

1. PlatformIO native test environment configured
2. Comprehensive mock infrastructure created
3. 27 test cases for TimingController written and **all passing**
4. Mock headers for all ESP32 dependencies
5. Build conflicts resolved using direct source inclusion
6. Test timing issues fixed (non-zero start times)

### 🎉 Resolution Implemented

**Approach Used: Direct Source Inclusion** (Option 3 variant)

The build conflicts were resolved by:

1. **Disabled automatic library building**: Set `lib_ignore = SignalEngine` in platformio.ini
2. **Direct source inclusion**: Test files directly include only the `.cpp` files they need:
   ```cpp
   #include "../mocks/mocks.cpp"
   #include "../../lib/SignalEngine/src/SignalState.cpp"
   #include "../../lib/SignalEngine/src/TimingController.cpp"
   ```
3. **Explicit dependency control**: Using `lib_ldf_mode = chain+` to find Unity but not auto-build libraries

**Benefits:**
- ✅ Tests compile and run successfully
- ✅ No modifications to production code needed
- ✅ Full control over which components are tested
- ✅ Fast build times (only compiles what's needed)
- ✅ Easy to extend for other components

### Test Results

```
================= 27 test cases: 27 succeeded in 00:00:02.323 =================

 ✓ calculateCycles() - 6 tests
 ✓ State management (onStart/onStop/onFrequencyChange) - 6 tests
 ✓ Cycle counting (getEstimatedCycleCount) - 3 tests
 ✓ Duration-based auto-stop - 5 tests
 ✓ Pulse count-based auto-stop - 5 tests
 ✓ Priority logic - 2 tests
```

## Future Tests

### Planned Test Files

- `test_signal_state/` - State management and thread safety
- `test_signal_persistence/` - NVS load/save logic
- `test_signal_event_publisher/` - Event publishing
- `test_command_dispatcher/` - Command handling (requires mock refactoring)
- `test_signal_engine_integration/` - End-to-end integration tests

### Test Coverage Goals

- **Unit Tests**: Test each component in isolation with mocks
- **Integration Tests**: Test component interactions
- **Thread Safety Tests**: Verify mutex protection (challenging in unit tests)
- **Edge Case Tests**: Overflow, invalid inputs, error conditions

## Best Practices

### Writing New Tests

1. **Include order matters**:
   ```cpp
   #include <unity.h>
   #include "mocks/mocks.h"  // Mocks FIRST
   #include "Component.h"     // Real headers AFTER
   ```

2. **Control mock time**:
   ```cpp
   extern uint64_t g_mockTimeMicros;
   g_mockTimeMicros = 1000000; // 1 second
   ```

3. **Reset state in setUp()**:
   ```cpp
   void setUp(void) {
       g_mockTimeMicros = 0;
       MockPreferences::clearAllStores();
   }
   ```

4. **Use clear test names**:
   ```cpp
   void test_calculateCycles_overflow_protection() { ... }
   ```

### Mock Customization

To add new mock behavior, edit `test/mocks/mocks.h`:

```cpp
class MockPulseGenerator {
public:
    // Add new mock methods as needed
    void setMockFailure(bool fail) { _mockFail = fail; }

private:
    bool _mockFail = false;
};
```

## Architecture Benefits

The SOLID refactoring makes testing much easier:

- **Single Responsibility**: Each class has focused, testable logic
- **Dependency Injection**: Easy to inject mocks (e.g., CommandDispatcher)
- **Interface Segregation**: Minimal dependencies between components
- **No Hardware Coupling**: Logic separated from ESP32-specific code

## Continuous Integration

### GitHub Actions (Future)

```yaml
name: Native Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-python@v2
      - run: pip install platformio
      - run: platformio test -e native
```

## References

- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [ESP-IDF Mocking Strategies](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)

---

**Last Updated**: 2025-11-10
**Test Framework**: Unity 2.6.0
**Status**: Infrastructure complete, resolving build conflicts
