# ESP32 Trigger - Refactoring Roadmap
**Generated:** 2025-11-09

This document provides a step-by-step guide to refactoring the ESP32 Trigger codebase from its current tightly-coupled architecture to a clean, testable, maintainable architecture following Clean Architecture and SOLID principles.

---

## Table of Contents
1. [Current State Assessment](#current-state-assessment)
2. [Target Architecture](#target-architecture)
3. [Refactoring Phases](#refactoring-phases)
4. [Implementation Steps](#implementation-steps)
5. [Testing Strategy](#testing-strategy)
6. [Risk Mitigation](#risk-mitigation)

---

## Current State Assessment

### Architecture Problems
```
┌─────────────────────────────────────────────────┐
│              CURRENT ARCHITECTURE               │
│                   (BROKEN)                      │
│                                                 │
│  ┌──────────────┐      ┌──────────────┐        │
│  │  WebFacade   │      │  SerialCLI   │        │
│  │ (HTTP/WS)    │      │  (UART)      │        │
│  └──────┬───────┘      └──────┬───────┘        │
│         │                     │                 │
│         │  ❌ TIGHT COUPLING  │                 │
│         └──────────┬──────────┘                 │
│                    ↓                            │
│         ┌──────────────────────┐                │
│         │   SignalEngine       │                │
│         │  (Concrete Class)    │                │
│         │  ❌ No Interface     │                │
│         └──────────┬───────────┘                │
│                    ↓                            │
│         ┌──────────────────────┐                │
│         │  PulseGenerator      │                │
│         │   (MCPWM HAL)        │                │
│         └──────────────────────┘                │
│                                                 │
│  Problems:                                      │
│  • Presentation depends on concrete domain      │
│  • Cannot mock SignalEngine for tests           │
│  • Business logic scattered in HTTP handlers    │
│  • Code duplication (ApiRouter vs SerialCLI)    │
│  • Platform coupling (ESP-IDF events, NVS)      │
└─────────────────────────────────────────────────┘
```

### Key Violations
| Violation | Files | Impact |
|-----------|-------|--------|
| **No domain interface** | SignalEngine.h | Cannot inject mocks, cannot swap implementations |
| **Presentation → Domain coupling** | WebFacade.h, ApiRouter.h, SerialCLI.h | Tight coupling, hard to test |
| **Missing Use Case layer** | ApiRouter.cpp, SerialCLI.cpp | Business logic duplication |
| **Platform coupling** | signal_iface.h (esp_event.h) | Cannot port to other platforms |
| **Direct NVS access** | SignalEngine.cpp, WifiMgr.cpp | Cannot test with mock storage |

---

## Target Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      TARGET ARCHITECTURE                            │
│                        (CLEAN)                                      │
│                                                                     │
│  ┌────────────────────┐       ┌────────────────────┐              │
│  │   HttpController   │       │   UartController   │              │
│  │  (Presentation)    │       │   (Presentation)   │              │
│  └─────────┬──────────┘       └─────────┬──────────┘              │
│            │                            │                          │
│            │  ✅ Depends on Interface  │                          │
│            └────────────┬───────────────┘                          │
│                         ↓                                          │
│            ┌────────────────────────────┐                          │
│            │   IStartSignalUseCase      │  ← Interface             │
│            │   IStopSignalUseCase       │  ← Interface             │
│            │   IGetStatusUseCase        │  ← Interface             │
│            └────────────┬───────────────┘                          │
│                         ↓  Uses                                    │
│            ┌────────────────────────────┐                          │
│            │  StartSignalUseCase        │  ← Implementation        │
│            │  (Business Logic)          │                          │
│            └────────────┬───────────────┘                          │
│                         ↓  Uses                                    │
│            ┌────────────────────────────┐                          │
│            │  ISignalController         │  ← Domain Interface      │
│            └────────────┬───────────────┘                          │
│                         ↓  Implemented by                          │
│            ┌────────────────────────────┐                          │
│            │    SignalEngine            │  ← Domain Implementation │
│            │  : ISignalController       │                          │
│            └────────────┬───────────────┘                          │
│                         ↓  Uses                                    │
│            ┌────────────────────────────┐                          │
│            │   IPulseGenerator          │  ← Hardware Interface    │
│            └────────────┬───────────────┘                          │
│                         ↓  Implemented by                          │
│            ┌────────────────────────────┐                          │
│            │  McpwmPulseGenerator       │  ← Hardware Impl         │
│            │   (ESP32 MCPWM)            │                          │
│            └────────────────────────────┘                          │
│                                                                     │
│  Benefits:                                                          │
│  ✅ All dependencies point inward (to interfaces)                  │
│  ✅ Can inject mocks at every layer                                │
│  ✅ Business logic centralized in use cases                        │
│  ✅ No code duplication                                            │
│  ✅ Platform-agnostic domain layer                                 │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Refactoring Phases

### Phase 1: Interface Extraction (Week 1)
**Goal:** Break the direct coupling between presentation and domain by introducing interfaces.

**Deliverables:**
- `common/ISignalController.h` - Domain interface
- `lib/SignalEngine/include/SignalEngine.h` - Implements ISignalController
- Updated: `lib/WebFacade/include/ApiRouter.h` - Depends on interface
- Updated: `lib/SerialCLI/include/SerialCLI.h` - Depends on interface

**Effort:** 4 hours
**Risk:** Low (additive changes)
**Priority:** 🔴 CRITICAL

---

### Phase 2: Use Case Layer (Week 2)
**Goal:** Extract business logic from presentation layer into dedicated use cases.

**Deliverables:**
- `lib/UseCases/include/IStartSignalUseCase.h`
- `lib/UseCases/include/IStopSignalUseCase.h`
- `lib/UseCases/include/IGetStatusUseCase.h`
- `lib/UseCases/src/StartSignalUseCase.cpp` (implementation)
- `lib/UseCases/src/StopSignalUseCase.cpp` (implementation)
- `lib/UseCases/src/GetStatusUseCase.cpp` (implementation)
- Updated: `lib/WebFacade/src/ApiRouter.cpp` - Uses use cases
- Updated: `lib/SerialCLI/src/SerialCLI.cpp` - Uses use cases

**Effort:** 8 hours
**Risk:** Medium (refactoring existing code)
**Priority:** 🔴 CRITICAL

---

### Phase 3: Platform Abstraction (Week 3)
**Goal:** Decouple from ESP-IDF-specific APIs (events, NVS).

**Deliverables:**
- `common/IEventPublisher.h` - Event interface
- `common/IConfigStorage.h` - Configuration storage interface
- `lib/Platform/EspEventAdapter.cpp` - ESP-IDF event adapter
- `lib/Platform/NvsConfigAdapter.cpp` - NVS storage adapter
- Updated: `lib/SignalEngine/src/SignalEngine.cpp` - Uses adapters

**Effort:** 6 hours
**Risk:** Medium (touches initialization code)
**Priority:** 🟡 MODERATE

---

### Phase 4: Dependency Injection (Week 4)
**Goal:** Replace manual dependency wiring with a DI container.

**Deliverables:**
- `lib/Core/ServiceLocator.h` - Simple DI container
- Updated: `apps/production_fw/main.cpp` - Uses DI container

**Effort:** 4 hours
**Risk:** Low (improves existing wiring)
**Priority:** 🟢 LOW

---

### Phase 5: Testing Infrastructure (Week 5)
**Goal:** Add comprehensive unit tests using the new architecture.

**Deliverables:**
- `test/mocks/MockSignalController.h`
- `test/mocks/MockEventPublisher.h`
- `test/mocks/MockConfigStorage.h`
- `test/use_cases/test_start_signal_use_case.cpp`
- `test/presentation/test_api_router.cpp`
- `test/domain/test_signal_engine.cpp`

**Effort:** 12 hours
**Risk:** Low (new code, doesn't affect production)
**Priority:** 🟡 MODERATE

---

## Implementation Steps

### Phase 1: Interface Extraction (Detailed)

#### Step 1.1: Create Domain Interface

**Create file:** `/home/user/espgen/common/ISignalController.h`

```cpp
#ifndef I_SIGNAL_CONTROLLER_H
#define I_SIGNAL_CONTROLLER_H

#include "signal_iface.h"

/**
 * @brief Abstract interface for signal control operations
 *
 * This interface defines the contract for controlling signal generation.
 * All presentation layers depend on this interface, not concrete implementations.
 */
class ISignalController {
public:
    virtual ~ISignalController() = default;

    // Command interface
    virtual bool sendCommand(const SignalCmd& cmd) = 0;

    // Status queries
    virtual SignalError getStatus(SignalStatus_t& status) = 0;
    virtual bool isRunning() const = 0;
    virtual double getCurrentFrequencyHz() const = 0;
    virtual float getCurrentDutyCycle() const = 0;
    virtual int getOutputPin() const = 0;

    // Configuration (optional - can be moved to separate interface)
    virtual double getLastAppliedFrequencyHz() const = 0;
    virtual float getLastAppliedDutyCycle() const = 0;
    virtual float getLastAppliedDurationSec() const = 0;
};

#endif // I_SIGNAL_CONTROLLER_H
```

**Why this design:**
- Pure virtual methods (abstract interface)
- No implementation details
- Platform-agnostic (no ESP-IDF types)
- Uses value objects (SignalCmd, SignalStatus_t)
- Virtual destructor for proper cleanup

---

#### Step 1.2: Make SignalEngine Implement Interface

**Modify:** `/home/user/espgen/lib/SignalEngine/include/SignalEngine.h`

**Before:**
```cpp
class SignalEngine {
public:
    SignalEngine();
    void begin();
    bool sendCommand(const SignalCmd& cmd);
    // ... methods ...
};
```

**After:**
```cpp
#include "ISignalController.h"

class SignalEngine : public ISignalController {
public:
    SignalEngine();
    void begin();

    // Implement ISignalController interface
    bool sendCommand(const SignalCmd& cmd) override;
    SignalError getStatus(SignalStatus_t& status) override;
    bool isRunning() const override;
    double getCurrentFrequencyHz() const override;
    float getCurrentDutyCycle() const override;
    int getOutputPin() const override;
    double getLastAppliedFrequencyHz() const override;
    float getLastAppliedDutyCycle() const override;
    float getLastAppliedDurationSec() const override;

    // SignalEngine-specific methods (not in interface)
    void loop();  // Keep for periodic tasks

private:
    // ... existing private members ...
};
```

**Changes:**
1. Add `#include "ISignalController.h"`
2. Change class declaration: `class SignalEngine : public ISignalController`
3. Add `override` keyword to all interface methods
4. Keep `begin()` and `loop()` as SignalEngine-specific

**Why:**
- Existing code continues to work (backward compatible)
- Interface methods are now virtual and overridable
- Can now pass SignalEngine as ISignalController&

---

#### Step 1.3: Update ApiRouter to Use Interface

**Modify:** `/home/user/espgen/lib/WebFacade/include/ApiRouter.h`

**Before:**
```cpp
#include "SignalEngine.h"

class ApiRouter {
public:
    ApiRouter(SignalEngine& engine, AsyncWebServer& server);
private:
    SignalEngine& _engine;  // ❌ Concrete dependency
};
```

**After:**
```cpp
#include "ISignalController.h"  // ✅ Interface dependency

class ApiRouter {
public:
    ApiRouter(ISignalController& controller, AsyncWebServer& server);
private:
    ISignalController& _controller;  // ✅ Interface reference
};
```

**Modify:** `/home/user/espgen/lib/WebFacade/src/ApiRouter.cpp`

**Before:**
```cpp
ApiRouter::ApiRouter(SignalEngine& engine, AsyncWebServer& server) :
    _engine(engine), _server(server) {}

void ApiRouter::handleTriggerPost(...) {
    _engine.sendCommand(cmd);  // ❌ Calls concrete class
}
```

**After:**
```cpp
ApiRouter::ApiRouter(ISignalController& controller, AsyncWebServer& server) :
    _controller(controller), _server(server) {}

void ApiRouter::handleTriggerPost(...) {
    _controller.sendCommand(cmd);  // ✅ Calls through interface
}
```

**Changes:**
1. Replace `SignalEngine&` with `ISignalController&`
2. Rename `_engine` to `_controller` (clearer naming)
3. Update all references from `_engine` to `_controller`

---

#### Step 1.4: Update WebFacade to Use Interface

**Modify:** `/home/user/espgen/lib/WebFacade/include/WebFacade.h`

**Before:**
```cpp
#include "SignalEngine.h"

class WebFacade {
public:
    WebFacade(SignalEngine& engine);
private:
    SignalEngine& _engine;
    ApiRouter _apiRouter;
};
```

**After:**
```cpp
#include "ISignalController.h"

class WebFacade {
public:
    WebFacade(ISignalController& controller);
private:
    ISignalController& _controller;
    ApiRouter _apiRouter;
};
```

**Modify:** `/home/user/espgen/lib/WebFacade/src/WebFacade.cpp`

**Before:**
```cpp
WebFacade::WebFacade(SignalEngine& engine) :
    _engine(engine),
    _server(80),
    _apiRouter(_engine, _server),  // Pass engine to ApiRouter
    // ...
{}
```

**After:**
```cpp
WebFacade::WebFacade(ISignalController& controller) :
    _controller(controller),
    _server(80),
    _apiRouter(_controller, _server),  // Pass controller interface
    // ...
{}
```

---

#### Step 1.5: Update SerialCLI to Use Interface

**Modify:** `/home/user/espgen/lib/SerialCLI/include/SerialCLI.h`

**Before:**
```cpp
#include "SignalEngine.h"

class SerialCLI {
public:
    SerialCLI(SignalEngine& engine);
private:
    SignalEngine& _engine;
};
```

**After:**
```cpp
#include "ISignalController.h"

class SerialCLI {
public:
    SerialCLI(ISignalController& controller);
private:
    ISignalController& _controller;
};
```

**Modify:** `/home/user/espgen/lib/SerialCLI/src/SerialCLI.cpp`

(Similar changes as ApiRouter)

---

#### Step 1.6: Update Applications (No Breaking Changes!)

**Note:** Applications can continue to use SignalEngine directly, because SignalEngine now implements ISignalController.

**File:** `/home/user/espgen/apps/production_fw/main.cpp`

**Before:**
```cpp
SignalEngine signalEngine;
SerialCLI serialCLI(signalEngine);
WebFacade webFacade(signalEngine);
```

**After (NO CHANGE NEEDED):**
```cpp
SignalEngine signalEngine;  // Concrete class
SerialCLI serialCLI(signalEngine);  // Implicit upcast to ISignalController&
WebFacade webFacade(signalEngine);  // Implicit upcast to ISignalController&
```

**Why this works:**
- `SignalEngine` IS-A `ISignalController` (inheritance)
- C++ allows implicit upcasting (derived → base)
- Existing code works without modification!

---

### Phase 2: Use Case Layer (Detailed)

#### Step 2.1: Define Use Case Interfaces

**Create file:** `/home/user/espgen/lib/UseCases/include/IStartSignalUseCase.h`

```cpp
#ifndef I_START_SIGNAL_USE_CASE_H
#define I_START_SIGNAL_USE_CASE_H

#include "signal_iface.h"
#include <Arduino.h>  // For String

/**
 * @brief Input data for starting signal generation
 */
struct StartSignalInput {
    double frequencyHz;
    float dutyCycle;
    float durationSec;
    uint8_t channel;
    SignalPolarity polarity;

    // Default values
    StartSignalInput() :
        frequencyHz(1000.0),
        dutyCycle(0.5f),
        durationSec(0.0f),
        channel(0),
        polarity(POLARITY_ACTIVE_HIGH) {}
};

/**
 * @brief Output result from starting signal
 */
struct StartSignalOutput {
    bool success;
    SignalError errorCode;
    String message;

    StartSignalOutput(bool s = false, SignalError err = SIG_OK, const String& msg = "") :
        success(s), errorCode(err), message(msg) {}
};

/**
 * @brief Use case for starting signal generation
 *
 * Encapsulates business logic for:
 * - Validating input parameters
 * - Building signal command
 * - Starting signal generation
 * - Returning structured result
 */
class IStartSignalUseCase {
public:
    virtual ~IStartSignalUseCase() = default;

    virtual StartSignalOutput execute(const StartSignalInput& input) = 0;
};

#endif // I_START_SIGNAL_USE_CASE_H
```

**Design notes:**
- Input/Output structs for clear contracts
- Validation logic will be in implementation
- Returns structured result (not just bool)
- Includes error messages for debugging

---

#### Step 2.2: Implement Use Case

**Create file:** `/home/user/espgen/lib/UseCases/src/StartSignalUseCase.cpp`

```cpp
#include "StartSignalUseCase.h"
#include "ISignalController.h"
#include "param_helpers.h"

class StartSignalUseCase : public IStartSignalUseCase {
public:
    StartSignalUseCase(ISignalController& controller)
        : _controller(controller) {}

    StartSignalOutput execute(const StartSignalInput& input) override {
        // Validation logic (centralized, no duplication!)
        if (input.frequencyHz <= 0 || input.frequencyHz > 8000000) {
            return StartSignalOutput(
                false,
                SIG_ERR_INVALID_PARAM,
                "Invalid frequency: must be between 0.001 Hz and 8 MHz"
            );
        }

        if (input.dutyCycle < 0.0f || input.dutyCycle > 1.0f) {
            return StartSignalOutput(
                false,
                SIG_ERR_INVALID_PARAM,
                "Invalid duty cycle: must be between 0.0 and 1.0"
            );
        }

        if (input.durationSec < 0.0f) {
            return StartSignalOutput(
                false,
                SIG_ERR_INVALID_PARAM,
                "Invalid duration: cannot be negative (use 0 for infinite)"
            );
        }

        if (input.channel > 5) {
            return StartSignalOutput(
                false,
                SIG_ERR_INVALID_PARAM,
                "Invalid channel: must be 0-5"
            );
        }

        // Build command using centralized helper (no duplication!)
        SignalCmd cmd = createStartCmd_FreqDuty(
            input.frequencyHz,
            input.dutyCycle,
            input.durationSec
        );
        cmd.channel = input.channel;
        cmd.polarity = input.polarity;

        // Execute command through domain interface
        bool success = _controller.sendCommand(cmd);

        if (!success) {
            return StartSignalOutput(
                false,
                SIG_ERR_QUEUE_FULL,
                "Failed to send command: queue full"
            );
        }

        return StartSignalOutput(
            true,
            SIG_OK,
            "Signal started successfully"
        );
    }

private:
    ISignalController& _controller;
};
```

**Benefits:**
1. **Single Responsibility:** Only handles "start signal" business logic
2. **No Duplication:** Validation/command building in ONE place
3. **Testable:** Can inject mock controller
4. **Clear Contract:** Input → Output with error messages

---

#### Step 2.3: Update ApiRouter to Use Use Case

**Modify:** `/home/user/espgen/lib/WebFacade/src/ApiRouter.cpp`

**Before (Business logic in presentation layer):**
```cpp
void ApiRouter::handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json) {
    // ❌ Validation in presentation layer
    double freqHz = json["frequency_hz"];
    if (freqHz <= 0 || freqHz > 8000000) {
        request->send(400, "application/json", "{\"error\":\"Invalid frequency\"}");
        return;
    }

    float duty = json["duty_cycle"];
    if (duty < 0 || duty > 1.0) {
        request->send(400, "application/json", "{\"error\":\"Invalid duty\"}");
        return;
    }

    // ❌ Command building in presentation layer
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = freqHz;
    cmd.dutyCycle = duty;
    cmd.durationSec = json["duration_sec"] | 0.0f;

    // ❌ Direct call to domain
    _controller.sendCommand(cmd);

    request->send(200, "application/json", "{\"status\":\"ok\"}");
}
```

**After (Presentation layer delegates to use case):**
```cpp
void ApiRouter::handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json) {
    // ✅ Presentation layer only handles HTTP concerns

    // Parse HTTP request to use case input
    StartSignalInput input;
    input.frequencyHz = json["frequency_hz"] | 1000.0;
    input.dutyCycle = json["duty_cycle"] | 0.5f;
    input.durationSec = json["duration_sec"] | 0.0f;
    input.channel = json["channel"] | 0;
    input.polarity = json["polarity"] | POLARITY_ACTIVE_HIGH;

    // Delegate to use case (business logic)
    StartSignalOutput output = _startSignalUseCase.execute(input);

    // Format HTTP response based on use case result
    if (output.success) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        JsonDocument doc;
        doc["error"] = output.message;
        doc["error_code"] = output.errorCode;
        String response;
        serializeJson(doc, response);
        request->send(400, "application/json", response);
    }
}
```

**Changes to ApiRouter class:**

**Header:** `/home/user/espgen/lib/WebFacade/include/ApiRouter.h`
```cpp
#include "IStartSignalUseCase.h"

class ApiRouter {
public:
    ApiRouter(IStartSignalUseCase& startUseCase,
              IStopSignalUseCase& stopUseCase,
              IGetStatusUseCase& statusUseCase,
              AsyncWebServer& server);
private:
    IStartSignalUseCase& _startSignalUseCase;
    IStopSignalUseCase& _stopSignalUseCase;
    IGetStatusUseCase& _statusUseCase;
    AsyncWebServer& _server;
};
```

**Benefits:**
1. **No Business Logic:** ApiRouter only does HTTP ↔ DTO conversion
2. **No Duplication:** SerialCLI can use the SAME use cases
3. **Better Error Messages:** Use case provides detailed messages
4. **Testable:** Can test ApiRouter with mock use cases

---

#### Step 2.4: Update SerialCLI to Use Use Case

**Modify:** `/home/user/espgen/lib/SerialCLI/src/SerialCLI.cpp`

**Before:**
```cpp
void SerialCLI::parseAndExecute() {
    // ❌ Duplicates ApiRouter logic
    if (_inputBuffer.startsWith("start")) {
        double freq = parseFrequency();
        float duty = parseDuty();

        SignalCmd cmd = {0};
        cmd.type = SIG_CMD_START;
        cmd.frequencyHz = freq;
        cmd.dutyCycle = duty;

        _controller.sendCommand(cmd);
    }
}
```

**After:**
```cpp
void SerialCLI::parseAndExecute() {
    // ✅ Uses same business logic as ApiRouter (no duplication!)
    if (_inputBuffer.startsWith("start")) {
        double freq = parseFrequency();
        float duty = parseDuty();

        StartSignalInput input;
        input.frequencyHz = freq;
        input.dutyCycle = duty;

        StartSignalOutput output = _startSignalUseCase.execute(input);

        if (output.success) {
            Serial.println("OK: Signal started");
        } else {
            Serial.printf("ERROR: %s\n", output.message.c_str());
        }
    }
}
```

**Result:** ApiRouter and SerialCLI now share the SAME validation and command building logic!

---

### Phase 3: Platform Abstraction (Summary)

**Goal:** Remove ESP-IDF dependencies from domain layer.

**Key Changes:**

1. **Create IEventPublisher:**
   - Abstract event publishing (replaces direct esp_event_post calls)
   - SignalEngine uses IEventPublisher& instead of ESP event loop
   - Create EspEventAdapter for ESP-IDF implementation

2. **Create IConfigStorage:**
   - Abstract persistent storage (replaces direct Preferences access)
   - SignalEngine uses IConfigStorage& instead of Preferences
   - Create NvsConfigAdapter for ESP32 NVS implementation

3. **Inject Adapters:**
   - `SignalEngine(IConfigStorage& storage, IEventPublisher& events)`
   - Adapters created in main.cpp and injected

**Benefits:**
- Domain layer has zero ESP-IDF dependencies
- Can port to Arduino, STM32, Linux, etc.
- Can test with mock storage and mock events

---

### Phase 4: Dependency Injection (Summary)

**Current Wiring (Manual):**
```cpp
// main.cpp
SignalEngine engine;
StartSignalUseCase startUseCase(engine);
ApiRouter apiRouter(startUseCase, ...);
WebFacade webFacade(apiRouter, ...);
```

**Target Wiring (DI Container):**
```cpp
// main.cpp
ServiceLocator services;

// Register implementations
services.registerSingleton<ISignalController>(new SignalEngine(...));
services.registerSingleton<IStartSignalUseCase>(new StartSignalUseCase(
    services.get<ISignalController>()
));

// Resolve and start
auto& webFacade = services.get<WebFacade>();
webFacade.begin();
```

**Benefits:**
- Centralized dependency management
- Easier to swap implementations
- Clear dependency graph visualization

---

## Testing Strategy

### Unit Tests (with Mocks)

**Example:** Test ApiRouter without real SignalEngine

```cpp
// test/presentation/test_api_router.cpp
#include <gtest/gtest.h>
#include "ApiRouter.h"
#include "mocks/MockStartSignalUseCase.h"

class ApiRouterTest : public ::testing::Test {
protected:
    MockStartSignalUseCase mockUseCase;
    AsyncWebServer server{80};
    ApiRouter router{mockUseCase, server};
};

TEST_F(ApiRouterTest, HandleTriggerPost_ValidInput_ReturnsOk) {
    // Arrange
    StartSignalOutput expectedOutput(true, SIG_OK, "OK");
    EXPECT_CALL(mockUseCase, execute(_))
        .WillOnce(Return(expectedOutput));

    // Act
    // ... simulate HTTP POST request ...

    // Assert
    // ... verify response is 200 OK ...
}

TEST_F(ApiRouterTest, HandleTriggerPost_InvalidFrequency_Returns400) {
    // Arrange
    StartSignalOutput expectedOutput(false, SIG_ERR_INVALID_PARAM, "Invalid frequency");
    EXPECT_CALL(mockUseCase, execute(_))
        .WillOnce(Return(expectedOutput));

    // Act & Assert
    // ... verify 400 error response ...
}
```

**Benefits:**
- Fast (no hardware, no tasks)
- Isolated (tests only HTTP logic)
- Reliable (no timing issues)

---

### Integration Tests (with Real Components)

**Example:** Test SignalEngine with real PulseGenerator

```cpp
// test/integration/test_signal_engine_integration.cpp
TEST(SignalEngineIntegration, StartStop_ValidParams_GeneratesPulses) {
    // Arrange
    MockConfigStorage storage;
    MockEventPublisher events;
    SignalEngine engine(storage, events);
    engine.begin();

    // Act
    SignalCmd cmd = createStartCmd_FreqDuty(1000, 0.5, 0);
    engine.sendCommand(cmd);
    delay(100);  // Let it run

    // Assert
    EXPECT_TRUE(engine.isRunning());
    // ... measure actual GPIO output (hardware test) ...
}
```

---

## Risk Mitigation

### Risks and Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|------------|------------|
| **Breaking existing functionality** | High | Medium | • Implement incrementally<br>• Add tests before refactoring<br>• Use feature flags |
| **Performance degradation** | Medium | Low | • Profile before/after<br>• Virtual function overhead is negligible<br>• Optimize hot paths if needed |
| **Increased complexity** | Low | Medium | • Document architecture clearly<br>• Provide examples<br>• Code reviews |
| **Team resistance** | Medium | Medium | • Demonstrate benefits with examples<br>• Show improved testability<br>• Provide training |

### Rollback Plan

Each phase is designed to be independently deployable:
- Phase 1 complete → Can ship with interfaces (backward compatible)
- Phase 2 complete → Can ship with use cases (old code still works)
- Phase 3 complete → Can ship with adapters (ESP-IDF still works)

If issues arise, revert individual phases without losing all progress.

---

## Success Metrics

### Before Refactoring
- Unit test coverage: 0%
- Integration test coverage: ~30%
- Testability score: 20/100
- Architecture conformance: 35/100
- Code duplication: High (ApiRouter vs SerialCLI)

### After Refactoring (Target)
- Unit test coverage: >80%
- Integration test coverage: >60%
- Testability score: 90/100
- Architecture conformance: 85/100
- Code duplication: None (shared use cases)

---

## Appendix A: Quick Reference

### Key Files Created

```
common/
  ISignalController.h        (Domain interface)
  IEventPublisher.h          (Event abstraction)
  IConfigStorage.h           (Storage abstraction)

lib/UseCases/
  include/
    IStartSignalUseCase.h
    IStopSignalUseCase.h
    IGetStatusUseCase.h
  src/
    StartSignalUseCase.cpp
    StopSignalUseCase.cpp
    GetStatusUseCase.cpp

lib/Platform/
  EspEventAdapter.h/.cpp
  NvsConfigAdapter.h/.cpp

lib/Core/
  ServiceLocator.h/.cpp

test/
  mocks/
    MockSignalController.h
    MockEventPublisher.h
    MockConfigStorage.h
  use_cases/
    test_start_signal_use_case.cpp
  presentation/
    test_api_router.cpp
  integration/
    test_signal_engine_integration.cpp
```

### Key Concepts

**Dependency Inversion Principle (DIP):**
> "High-level modules should not depend on low-level modules. Both should depend on abstractions."

**Use Case Pattern:**
> "Encapsulate all business logic for a single user goal in a dedicated class."

**Adapter Pattern:**
> "Convert the interface of a class into another interface clients expect."

---

**END OF ROADMAP**
