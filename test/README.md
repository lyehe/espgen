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

### ✅ Completed

1. PlatformIO native test environment configured
2. Comprehensive mock infrastructure created
3. 27 test cases for TimingController written
4. Mock headers for all ESP32 dependencies

### ⚠️ Known Issues

**Build Conflicts**: The current setup has compilation conflicts between:
- Real library headers (PulseGenerator.h)
- Mock definitions (mocks.h)

Both are being included during compilation, causing:
- Duplicate type definitions (`PulseChannelConfig_t`)
- Class vs typedef conflicts (`PulseGenerator`)

### 🔧 Resolution Options

**Option 1: Conditional Compilation in Real Headers** (Recommended)
Add to `lib/SignalEngine/include/PulseGenerator.h`:
```cpp
#ifdef NATIVE_TEST
// For native tests, use mock from mocks.h
#include "mocks.h"
#else
// Real implementation
class PulseGenerator {
  ...
};
#endif
```

**Option 2: Separate Test Library**
Create `lib/SignalEngine_Test/` with only testable components:
- SignalState
- TimingController
- SignalPersistence
- SignalEventPublisher

Exclude:
- PulseGenerator (hardware-dependent)
- SignalEngine (depends on PulseGenerator)
- CommandDispatcher (depends on everything)

**Option 3: Manual Build Configuration**
Use `src_filter` in `platformio.ini` to explicitly list which `.cpp` files to compile for native tests.

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
