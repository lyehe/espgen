# ESP32 Trigger Architecture Analysis & Dependency Report

**Generated:** 2025-11-09
**Branch:** claude/replace-wled-with-pul-011CUqVWrW1hLpGXbLd7StJM

---

## Executive Summary

This codebase exhibits a **mostly clean layered architecture** with some **critical coupling violations**. The core domain (SignalEngine/PulseGenerator) is relatively well-isolated, but the presentation layers (WebFacade, SerialCLI) have **direct dependencies on the core domain**, creating tight coupling that violates the Dependency Inversion Principle (DIP).

**Key Findings:**
- ✅ Good: Clean domain model with well-defined interfaces (`signal_iface.h`)
- ✅ Good: Separation of concerns between modules
- ❌ Problem: Direct coupling from infrastructure to domain
- ❌ Problem: No abstraction layer for domain logic
- ❌ Problem: Multiple modules directly instantiate and manipulate SignalEngine
- ⚠️ Warning: Build-time coupling through compile flags

---

## 1. Current Module Dependencies

### 1.1 Dependency Graph (ASCII Visualization)

```
┌─────────────────────────────────────────────────────────────────┐
│                         Applications Layer                       │
│  ┌──────────────────┐  ┌──────────────────┐  ┌───────────────┐ │
│  │ production_fw/   │  │  signal_cli/     │  │  web_mock/    │ │
│  │   main.cpp       │  │   main.cpp       │  │   main.cpp    │ │
│  └────────┬─────────┘  └────────┬─────────┘  └───────┬───────┘ │
└───────────┼────────────────────┼─────────────────────┼─────────┘
            │                    │                     │
            ▼                    ▼                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Presentation/Infrastructure Layer             │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐           │
│  │ WebFacade    │  │ SerialCLI   │  │ OTABridge    │           │
│  │ ┌──────────┐ │  │             │  │              │           │
│  │ │ApiRouter │ │  │             │  │              │           │
│  │ │WifiMgr   │ │  │             │  │              │           │
│  │ │WSHub     │ │  │             │  │              │           │
│  └──────┬───────┘  └──────┬──────┘  └──────┬───────┘           │
└─────────┼───────────────┼─────────────────┼─────────────────────┘
          │               │                 │
          │  ┌───────────┴─────────────────┤
          │  │  TIGHT COUPLING (VIOLATION) │
          ▼  ▼                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                         Domain Layer                             │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  SignalEngine                                            │   │
│  │  ┌────────────────┐  ┌────────────────┐                 │   │
│  │  │ PulseGenerator │  │ Command Queue  │                 │   │
│  │  │    (MCPWM)     │  │  (FreeRTOS)    │                 │   │
│  │  └────────────────┘  └────────────────┘                 │   │
│  └──────────────────────────────────────────────────────────┘   │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                         Common/Shared Layer                      │
│  ┌───────────────┐  ┌────────────────┐  ┌─────────────────┐    │
│  │ signal_iface.h│  │ build_opts.h   │  │ param_helpers.h │    │
│  │ (Interfaces)  │  │ (Config)       │  │ (Utilities)     │    │
│  └───────────────┘  └────────────────┘  └─────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Module Dependency Matrix

| Module          | Depends On                                              | Depended By               |
|-----------------|--------------------------------------------------------|---------------------------|
| **SignalEngine**| PulseGenerator, signal_iface.h, build_opts.h, Preferences, param_helpers.h | WebFacade, SerialCLI, Apps |
| **PulseGenerator** | signal_iface.h, ESP32 MCPWM drivers               | SignalEngine              |
| **WebFacade**   | SignalEngine, ApiRouter, WifiMgr, WebSocketHub, OTAService | Apps                    |
| **ApiRouter**   | SignalEngine, signal_iface.h, ArduinoJson, ESPAsyncWebServer | WebFacade          |
| **WebSocketHub**| ESPAsyncWebServer, signal_iface.h, ArduinoJson         | WebFacade                 |
| **WifiMgr**     | WiFiManager, Preferences, preferences_keys.h           | WebFacade                 |
| **SerialCLI**   | SignalEngine, signal_iface.h                           | Apps                      |
| **OTAService**  | ElegantOTA, ESPAsyncWebServer                          | WebFacade                 |
| **signal_iface.h** | ESP Event Loop (esp_event.h)                       | All modules               |
| **build_opts.h**| None                                                   | SignalEngine, Apps        |

---

## 2. Detailed File-Level Dependencies

### 2.1 Core Domain Layer

#### `/home/user/espgen/lib/SignalEngine/include/SignalEngine.h`
**Dependencies:**
- `PulseGenerator.h` (composition)
- `signal_iface.h` (interface definitions)
- `freertos/FreeRTOS.h`, `freertos/task.h`, `freertos/queue.h` (OS primitives)
- `esp_event.h` (event system)

**Provides:**
- Command queue interface: `sendCommand(const SignalCmd&)`
- Status getters: `getCurrentFrequencyHz()`, `getCurrentDutyCycle()`, `isRunning()`
- Configuration getters: `getOutputPin()`, `getCurrentStatus(SignalStatus_t&)`

**Coupling Issues:**
- ❌ Publicly exposes concrete class (no interface abstraction)
- ❌ Tightly coupled to FreeRTOS primitives
- ✅ Good: Uses value objects (SignalCmd) for communication

#### `/home/user/espgen/lib/SignalEngine/include/PulseGenerator.h`
**Dependencies:**
- `driver/mcpwm.h`, `driver/gpio.h` (ESP32 HAL)
- `signal_iface.h` (PulseParams_t, SignalPolarity)

**Provides:**
- Multi-channel PWM generation (6 channels max)
- Master-slave synchronization
- Phase offset control

**Coupling Issues:**
- ✅ Good: Well-encapsulated hardware abstraction
- ✅ Good: Uses common interface types
- ⚠️ Warning: Direct hardware coupling (MCPWM-specific)

### 2.2 Presentation/Infrastructure Layer

#### `/home/user/espgen/lib/WebFacade/include/WebFacade.h`
**Dependencies:**
```cpp
#include "WifiMgr.h"
#include "SignalEngine.h"      // ❌ VIOLATION: Direct domain dependency
#include "ApiRouter.h"
#include "WebSocketHub.h"
#include "OTAService.h"
```

**Constructor:**
```cpp
WebFacade(SignalEngine& engine);  // ❌ Requires concrete SignalEngine
```

**Coupling Issues:**
- ❌ **CRITICAL**: Direct dependency on SignalEngine concrete class
- ❌ Constructor requires concrete reference (not interface)
- ❌ Composition pattern creates tight coupling
- ⚠️ Aggregates multiple services (God Object tendencies)

#### `/home/user/espgen/lib/WebFacade/include/ApiRouter.h`
**Dependencies:**
```cpp
#include "signal_iface.h"
#include "SignalEngine.h"      // ❌ VIOLATION: Direct domain dependency
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
```

**Constructor:**
```cpp
ApiRouter(SignalEngine& engine, AsyncWebServer& server);
```

**Methods:**
- `handleTriggerPost()` - Directly calls `_engine.sendCommand()`
- `handleStatusGet()` - Directly calls `_engine.getCurrentStatus()`
- `handleChannelPost()` - Direct manipulation of engine

**Coupling Issues:**
- ❌ **CRITICAL**: Directly calls domain methods
- ❌ No abstraction/use case layer
- ❌ Violates Dependency Inversion Principle
- ⚠️ Mixes HTTP concerns with domain logic

**Example Violation (from ApiRouter.cpp):**
```cpp
void ApiRouter::handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json) {
    // ... parse JSON ...
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = freqHz;
    cmd.dutyCycle = duty;
    _engine.sendCommand(cmd);  // ❌ Direct call to domain
}
```

#### `/home/user/espgen/lib/SerialCLI/include/SerialCLI.h`
**Dependencies:**
```cpp
#include "SignalEngine.h"      // ❌ VIOLATION: Direct domain dependency
```

**Constructor:**
```cpp
SerialCLI(SignalEngine& engine);
```

**Coupling Issues:**
- ❌ **CRITICAL**: Same violations as WebFacade
- ❌ parseAndExecute() directly manipulates SignalEngine
- ❌ No command abstraction layer

**Example Violation (from SerialCLI.cpp):**
```cpp
void SerialCLI::parseAndExecute() {
    SignalCmd cmd = {0};
    if (_inputBuffer == "start") {
        cmd.type = SIG_CMD_START;
        _engine.sendCommand(cmd);  // ❌ Direct call to domain
    }
}
```

### 2.3 Common/Shared Layer

#### `/home/user/espgen/common/signal_iface.h`
**Purpose:** Define domain interfaces and data structures

**Defines:**
- Enums: `SigCmdType`, `ParamMode`, `SignalPolarity`, `SigEvtId`, `SignalError`
- Structs: `SignalCmd`, `SignalEvtData`, `SignalStatus_t`, `PulseParams_t`
- Helper functions: Type conversions (frequency ↔ period, duty ↔ pulse width, etc.)

**Dependencies:**
```cpp
#include "esp_event.h"  // ⚠️ Couples to ESP-IDF event system
```

**Coupling Issues:**
- ⚠️ Mixed responsibilities: Domain types + framework types
- ✅ Good: C-compatible for cross-language use
- ⚠️ Union-based parameter specification is complex

#### `/home/user/espgen/common/build_opts.h`
**Purpose:** Compile-time configuration

**Defines:**
- Feature flags: `BUILD_WEB`, `BUILD_SERIAL_CLI`, `BUILD_OTA`
- Default values: `DEFAULT_OUTPUT_PIN`, `DEFAULT_FREQUENCY_HZ`, etc.

**Coupling Issues:**
- ❌ Build-time coupling (not runtime configuration)
- ⚠️ Hardcoded defaults spread across multiple files
- ⚠️ No central configuration management

---

## 3. Coupling Points Between Layers

### 3.1 Critical Coupling Violations

#### **Violation #1: Direct Domain Dependencies in Presentation Layer**

**Location:** WebFacade, ApiRouter, SerialCLI
**Severity:** 🔴 **CRITICAL**

**Problem:**
```cpp
// WebFacade.h
class WebFacade {
    SignalEngine& _engine;  // ❌ Direct reference to concrete class
};

// ApiRouter.h
class ApiRouter {
    SignalEngine& _engine;  // ❌ Direct reference to concrete class
    void handleTriggerPost(...) {
        _engine.sendCommand(cmd);  // ❌ Calls concrete methods
    }
};
```

**Why This Violates Clean Architecture:**
1. **Dependency Rule Violation**: Outer layers (WebFacade, SerialCLI) depend on inner layers (SignalEngine)
2. **No Abstraction**: Presentation layer depends on concrete implementation, not interface
3. **Tight Coupling**: Cannot swap SignalEngine implementation without modifying all presentation layers
4. **Testing Difficulty**: Cannot mock SignalEngine for unit tests

**Impact:**
- Cannot test WebFacade/ApiRouter without real SignalEngine
- Cannot replace SignalEngine with different implementation
- Changes to SignalEngine ripple through all dependent layers
- Violates Open/Closed Principle

---

#### **Violation #2: No Use Case Layer**

**Location:** Application architecture
**Severity:** 🔴 **CRITICAL**

**Problem:**
The architecture jumps directly from presentation (HTTP handlers, CLI) to domain (SignalEngine):

```
HTTP Request → ApiRouter → SignalEngine
CLI Command  → SerialCLI → SignalEngine
```

**Missing Layer:**
```
HTTP Request → ApiRouter → [USE CASE] → SignalEngine
                              ↑
                        MISSING LAYER
```

**Why This Is Bad:**
1. **No Business Logic Layer**: Domain logic is split between presentation and engine
2. **Code Duplication**: Same logic repeated in ApiRouter and SerialCLI
3. **No Transaction Boundaries**: Each handler directly manipulates state
4. **No Validation Layer**: Input validation scattered across handlers

**Example Duplication:**
```cpp
// ApiRouter.cpp
cmd.type = SIG_CMD_START;
cmd.frequencyHz = freqHz;
cmd.dutyCycle = duty;
_engine.sendCommand(cmd);

// SerialCLI.cpp (same logic!)
cmd.type = SIG_CMD_START;
cmd.frequencyHz = freqHz;
cmd.dutyCycle = duty;
_engine.sendCommand(cmd);
```

---

#### **Violation #3: Build-Time Feature Coupling**

**Location:** `build_opts.h`, `platformio.ini`, application main files
**Severity:** 🟡 **MODERATE**

**Problem:**
```cpp
// main.cpp
#if BUILD_WEB
#include "WebFacade.h"
WebFacade webFacade(signalEngine);
webFacade.begin();
#endif

#if BUILD_SERIAL_CLI
#include "SerialCLI.h"
SerialCLI serialCLI(signalEngine);
serialCLI.begin();
#endif
```

**Issues:**
1. Features compiled in/out at build time (not runtime configuration)
2. Requires separate builds for different configurations
3. Increases build matrix complexity
4. Cannot enable/disable features at runtime

---

#### **Violation #4: Event System Coupling**

**Location:** `signal_iface.h`, `WebSocketHub`
**Severity:** 🟡 **MODERATE**

**Problem:**
```cpp
// signal_iface.h
#include "esp_event.h"  // ❌ Couples interface to ESP-IDF framework
ESP_EVENT_DECLARE_BASE(SIGNAL_EVENTS);
```

**Issues:**
1. Domain interfaces coupled to ESP-IDF event system
2. Cannot port to different platform without ESP event loop
3. Event base declared in common header (global coupling)
4. WebSocketHub directly subscribes to ESP events

---

#### **Violation #5: Global State and Preferences**

**Location:** SignalEngine, WifiMgr
**Severity:** 🟡 **MODERATE**

**Problem:**
```cpp
// SignalEngine.cpp
Preferences preferences;
preferences.begin(NVS_NAMESPACE, false);
double loadedFreq = preferences.getDouble(NVS_KEY_FREQ, DEFAULT_FREQUENCY_HZ);
```

**Issues:**
1. Direct coupling to ESP32 NVS (Preferences library)
2. Global state management (no repository pattern)
3. Hardcoded namespace strings
4. Multiple modules write to same NVS (potential conflicts)

**Conflict Risk:**
- SignalEngine writes to `DEVICE_CFG_NAMESPACE`
- WifiMgr writes to `WIFI_NVS_NAMESPACE`
- No centralized storage management
- No transaction support

---

### 3.2 Dependency Flow Analysis

#### **Current Flow (WRONG):**
```
User Input (HTTP/CLI)
    ↓
Presentation Layer (ApiRouter/SerialCLI)
    ↓ [DIRECT CALL]
Domain Layer (SignalEngine)
    ↓
Hardware Abstraction (PulseGenerator)
    ↓
ESP32 MCPWM Driver
```

**Problems:**
- Presentation → Domain: **Direct coupling** ❌
- No abstraction boundaries
- Cannot intercept or modify behavior
- Hard to test

#### **Correct Flow (SHOULD BE):**
```
User Input (HTTP/CLI)
    ↓
Presentation Layer (Controllers)
    ↓ [INTERFACE]
Use Case Layer (Application Services)
    ↓ [INTERFACE]
Domain Layer (Engine Interface → SignalEngine Implementation)
    ↓ [INTERFACE]
Hardware Abstraction (PulseGenerator)
    ↓
ESP32 MCPWM Driver
```

**Benefits:**
- Each layer depends on interfaces (abstractions)
- Can inject mocks at each boundary
- Clear separation of concerns
- Testable in isolation

---

## 4. Clean Architecture Violations Summary

| Principle | Violation | Location | Severity |
|-----------|-----------|----------|----------|
| **Dependency Inversion** | Presentation depends on concrete domain | WebFacade, ApiRouter, SerialCLI | 🔴 Critical |
| **Single Responsibility** | WebFacade aggregates multiple services | WebFacade.h | 🟡 Moderate |
| **Open/Closed** | Cannot extend without modifying | ApiRouter, SerialCLI | 🟡 Moderate |
| **Interface Segregation** | No domain interface | SignalEngine.h | 🔴 Critical |
| **Separation of Concerns** | Business logic in presentation | ApiRouter::handleTriggerPost | 🔴 Critical |
| **Don't Repeat Yourself** | Command building duplicated | ApiRouter.cpp, SerialCLI.cpp | 🟡 Moderate |
| **Dependency Rule** | Outer layers depend on inner | All presentation layers | 🔴 Critical |

---

## 5. Recommended Refactoring Approach

### Phase 1: Introduce Domain Interfaces (CRITICAL)

**Goal:** Break direct coupling between presentation and domain

#### Step 1.1: Define Signal Control Interface

**Create:** `/home/user/espgen/common/ISignalController.h`

```cpp
#ifndef I_SIGNAL_CONTROLLER_H
#define I_SIGNAL_CONTROLLER_H

#include "signal_iface.h"

/**
 * @brief Abstract interface for signal control operations
 *
 * This interface defines the contract for controlling signal generation.
 * Presentation layers depend on this interface, not concrete implementations.
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

    // Configuration
    virtual int getOutputPin() const = 0;
};

#endif // I_SIGNAL_CONTROLLER_H
```

#### Step 1.2: Make SignalEngine Implement Interface

**Modify:** `/home/user/espgen/lib/SignalEngine/include/SignalEngine.h`

```cpp
#include "ISignalController.h"  // Add interface

class SignalEngine : public ISignalController {  // Inherit from interface
public:
    // Existing methods...

    // Explicitly override interface methods
    bool sendCommand(const SignalCmd& cmd) override;
    SignalError getStatus(SignalStatus_t& status) override;
    bool isRunning() const override;
    double getCurrentFrequencyHz() const override;
    float getCurrentDutyCycle() const override;
    int getOutputPin() const override;

    // ... rest of implementation
};
```

#### Step 1.3: Update Presentation Layers to Use Interface

**Before (ApiRouter.h):**
```cpp
class ApiRouter {
    SignalEngine& _engine;  // ❌ Concrete dependency
};
```

**After (ApiRouter.h):**
```cpp
class ApiRouter {
    ISignalController& _controller;  // ✅ Interface dependency
};
```

**Impact:**
- ✅ Presentation layers now depend on abstraction
- ✅ Can inject mock controllers for testing
- ✅ Can swap SignalEngine implementation
- ✅ Follows Dependency Inversion Principle

---

### Phase 2: Introduce Use Case Layer

**Goal:** Extract business logic from presentation layer

#### Step 2.1: Define Use Case Interfaces

**Create:** `/home/user/espgen/lib/UseCases/include/IStartSignalUseCase.h`

```cpp
#ifndef I_START_SIGNAL_USE_CASE_H
#define I_START_SIGNAL_USE_CASE_H

#include "signal_iface.h"

/**
 * @brief Input data for starting signal generation
 */
struct StartSignalInput {
    double frequencyHz;
    float dutyCycle;
    float durationSec;
    uint8_t channel;
    SignalPolarity polarity;
};

/**
 * @brief Output result from starting signal
 */
struct StartSignalOutput {
    bool success;
    SignalError error;
    String message;
};

/**
 * @brief Use case for starting signal generation
 *
 * Encapsulates business logic for validating input and starting signal.
 */
class IStartSignalUseCase {
public:
    virtual ~IStartSignalUseCase() = default;

    virtual StartSignalOutput execute(const StartSignalInput& input) = 0;
};

#endif // I_START_SIGNAL_USE_CASE_H
```

#### Step 2.2: Implement Use Case

**Create:** `/home/user/espgen/lib/UseCases/src/StartSignalUseCase.cpp`

```cpp
#include "StartSignalUseCase.h"
#include "param_helpers.h"

class StartSignalUseCase : public IStartSignalUseCase {
public:
    StartSignalUseCase(ISignalController& controller)
        : _controller(controller) {}

    StartSignalOutput execute(const StartSignalInput& input) override {
        // Validation logic (centralized!)
        if (input.frequencyHz <= 0 || input.frequencyHz > 8000000) {
            return {false, SIG_ERR_INVALID_PARAM, "Invalid frequency"};
        }
        if (input.dutyCycle < 0.0f || input.dutyCycle > 1.0f) {
            return {false, SIG_ERR_INVALID_PARAM, "Invalid duty cycle"};
        }

        // Build command using helper (no duplication!)
        SignalCmd cmd = createStartCmd_FreqDuty(
            input.frequencyHz,
            input.dutyCycle,
            input.durationSec
        );
        cmd.channel = input.channel;
        cmd.polarity = input.polarity;

        // Execute command
        bool success = _controller.sendCommand(cmd);

        return {success, success ? SIG_OK : SIG_ERR_QUEUE_FULL, ""};
    }

private:
    ISignalController& _controller;
};
```

#### Step 2.3: Update Presentation Layers to Use Use Cases

**Before (ApiRouter.cpp):**
```cpp
void ApiRouter::handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json) {
    // ❌ Business logic scattered in presentation layer
    double freqHz = json["frequency_hz"];
    float duty = json["duty_cycle"];

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = freqHz;
    cmd.dutyCycle = duty;

    _engine.sendCommand(cmd);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}
```

**After (ApiRouter.cpp):**
```cpp
void ApiRouter::handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json) {
    // ✅ Presentation layer only handles HTTP concerns
    StartSignalInput input;
    input.frequencyHz = json["frequency_hz"];
    input.dutyCycle = json["duty_cycle"];
    input.durationSec = json["duration_sec"] | 0.0f;
    input.channel = json["channel"] | 0;

    // Delegate to use case
    StartSignalOutput output = _startSignalUseCase.execute(input);

    if (output.success) {
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        request->send(400, "application/json",
            "{\"error\":\"" + output.message + "\"}");
    }
}
```

**Benefits:**
- ✅ Business logic centralized in use cases
- ✅ No duplication between ApiRouter and SerialCLI
- ✅ Easy to test use cases independently
- ✅ Presentation layers become thin adapters

---

### Phase 3: Decouple Event System

**Goal:** Remove ESP-IDF event coupling from domain interfaces

#### Step 3.1: Create Platform-Agnostic Event Interface

**Create:** `/home/user/espgen/common/IEventPublisher.h`

```cpp
#ifndef I_EVENT_PUBLISHER_H
#define I_EVENT_PUBLISHER_H

#include "signal_iface.h"

/**
 * @brief Generic event data
 */
struct SignalEvent {
    SigEvtId type;
    SignalEvtData data;
};

/**
 * @brief Callback type for event subscribers
 */
typedef void (*EventCallback)(const SignalEvent& event, void* context);

/**
 * @brief Platform-agnostic event publisher interface
 */
class IEventPublisher {
public:
    virtual ~IEventPublisher() = default;

    virtual void publish(const SignalEvent& event) = 0;
    virtual void subscribe(EventCallback callback, void* context) = 0;
    virtual void unsubscribe(EventCallback callback) = 0;
};

#endif // I_EVENT_PUBLISHER_H
```

#### Step 3.2: Implement ESP-IDF Adapter

**Create:** `/home/user/espgen/lib/Platform/EspEventAdapter.h`

```cpp
class EspEventAdapter : public IEventPublisher {
public:
    void publish(const SignalEvent& event) override {
        // Wrap generic event in ESP event
        esp_event_post(SIGNAL_EVENTS, event.type,
                      &event.data, sizeof(event.data), 0);
    }

    void subscribe(EventCallback callback, void* context) override {
        // Register ESP event handler that calls generic callback
        // ... implementation ...
    }
};
```

**Benefits:**
- ✅ Domain code doesn't depend on ESP-IDF
- ✅ Can replace with different event system (e.g., Qt signals)
- ✅ Easier to port to other platforms

---

### Phase 4: Introduce Configuration Abstraction

**Goal:** Decouple from ESP32 Preferences/NVS

#### Step 4.1: Define Storage Interface

**Create:** `/home/user/espgen/common/IConfigStorage.h`

```cpp
class IConfigStorage {
public:
    virtual ~IConfigStorage() = default;

    virtual bool getDouble(const char* key, double& value, double defaultVal) = 0;
    virtual bool setDouble(const char* key, double value) = 0;

    virtual bool getFloat(const char* key, float& value, float defaultVal) = 0;
    virtual bool setFloat(const char* key, float value) = 0;

    virtual bool getUChar(const char* key, uint8_t& value, uint8_t defaultVal) = 0;
    virtual bool setUChar(const char* key, uint8_t value) = 0;

    virtual bool commit() = 0;
};
```

#### Step 4.2: Implement NVS Adapter

**Create:** `/home/user/espgen/lib/Platform/NvsConfigAdapter.cpp`

```cpp
class NvsConfigAdapter : public IConfigStorage {
    Preferences _prefs;
    String _namespace;

public:
    NvsConfigAdapter(const char* ns) : _namespace(ns) {
        _prefs.begin(_namespace.c_str(), false);
    }

    bool getDouble(const char* key, double& value, double defaultVal) override {
        value = _prefs.getDouble(key, defaultVal);
        return true;
    }

    // ... implement other methods ...
};
```

#### Step 4.3: Inject Storage into SignalEngine

**Before:**
```cpp
// SignalEngine.cpp
Preferences preferences;
preferences.begin(NVS_NAMESPACE, false);
double loadedFreq = preferences.getDouble(NVS_KEY_FREQ, DEFAULT_FREQUENCY_HZ);
```

**After:**
```cpp
// SignalEngine.h
class SignalEngine : public ISignalController {
    IConfigStorage& _storage;
public:
    SignalEngine(IConfigStorage& storage, ...);
};

// SignalEngine.cpp
SignalEngine::begin() {
    double loadedFreq;
    _storage.getDouble(NVS_KEY_FREQ, loadedFreq, DEFAULT_FREQUENCY_HZ);
}
```

**Benefits:**
- ✅ Can inject mock storage for testing
- ✅ Can swap NVS for SD card, cloud, etc.
- ✅ Centralized storage management

---

### Phase 5: Runtime Feature Configuration

**Goal:** Replace compile-time flags with runtime configuration

#### Step 5.1: Create Feature Registry

**Create:** `/home/user/espgen/lib/Core/FeatureRegistry.h`

```cpp
class FeatureRegistry {
    std::map<String, bool> _features;

public:
    void enable(const String& feature) { _features[feature] = true; }
    void disable(const String& feature) { _features[feature] = false; }
    bool isEnabled(const String& feature) const {
        auto it = _features.find(feature);
        return (it != _features.end()) && it->second;
    }
};
```

#### Step 5.2: Use Registry in Main

**Before:**
```cpp
#if BUILD_WEB
WebFacade webFacade(signalEngine);
webFacade.begin();
#endif
```

**After:**
```cpp
FeatureRegistry features;
features.enable("web");
features.enable("serial_cli");

if (features.isEnabled("web")) {
    WebFacade webFacade(signalEngine);
    webFacade.begin();
}
```

**Benefits:**
- ✅ Single binary for all configurations
- ✅ Runtime enable/disable
- ✅ Simpler build process

---

## 6. Refactoring Priority Roadmap

### **🔴 High Priority (Do First)**

1. **Introduce ISignalController Interface** (Phase 1.1-1.3)
   - **Effort:** 4 hours
   - **Impact:** Breaks critical coupling violations
   - **Risk:** Low (additive change, doesn't break existing code)

2. **Create Use Case Layer** (Phase 2.1-2.3)
   - **Effort:** 8 hours
   - **Impact:** Eliminates code duplication, centralizes business logic
   - **Risk:** Medium (requires refactoring ApiRouter and SerialCLI)

### **🟡 Medium Priority (Do Second)**

3. **Abstract Event System** (Phase 3.1-3.2)
   - **Effort:** 4 hours
   - **Impact:** Improves portability
   - **Risk:** Low (adapter pattern, minimal changes)

4. **Abstract Configuration Storage** (Phase 4.1-4.3)
   - **Effort:** 6 hours
   - **Impact:** Improves testability, enables storage flexibility
   - **Risk:** Medium (touches SignalEngine initialization)

### **🟢 Low Priority (Do Third)**

5. **Runtime Feature Configuration** (Phase 5.1-5.2)
   - **Effort:** 4 hours
   - **Impact:** Simplifies build process
   - **Risk:** Low (doesn't affect architecture)

---

## 7. Testing Strategy After Refactoring

### Before Refactoring (Current State)
```cpp
// ❌ Cannot test ApiRouter without real SignalEngine
TEST(ApiRouterTest, HandleTriggerPost) {
    SignalEngine engine;  // Requires full initialization
    engine.begin();       // Starts tasks, hardware
    AsyncWebServer server(80);
    ApiRouter router(engine, server);
    // ... test HTTP handling (very slow, unreliable)
}
```

### After Refactoring (Target State)
```cpp
// ✅ Can test ApiRouter with mock controller
class MockSignalController : public ISignalController {
public:
    MOCK_METHOD(bool, sendCommand, (const SignalCmd&), (override));
    MOCK_METHOD(SignalError, getStatus, (SignalStatus_t&), (override));
    // ...
};

TEST(ApiRouterTest, HandleTriggerPost) {
    MockSignalController mockController;
    AsyncWebServer server(80);
    ApiRouter router(mockController, server);

    // Set expectations
    EXPECT_CALL(mockController, sendCommand(_))
        .WillOnce(Return(true));

    // Test HTTP handling (fast, isolated)
}
```

**Benefits:**
- ✅ Fast unit tests (no hardware/tasks)
- ✅ Isolated tests (no dependencies)
- ✅ Can verify business logic separately from infrastructure

---

## 8. Architecture Comparison: Before vs. After

### Current Architecture (Before)
```
┌─────────────┐
│   Apps      │
└──────┬──────┘
       │
┌──────▼──────────────────────────────┐
│  Presentation (WebFacade, CLI)      │
│  ┌────────────────────────────────┐ │
│  │ ❌ Direct calls to concrete    │ │
│  │ ❌ Business logic scattered    │ │
│  │ ❌ No abstraction              │ │
│  └────────────┬───────────────────┘ │
└───────────────┼─────────────────────┘
                │ TIGHT COUPLING
┌───────────────▼─────────────────────┐
│  Domain (SignalEngine)              │
│  ┌────────────────────────────────┐ │
│  │ ✅ Core logic                  │ │
│  │ ❌ No interface                │ │
│  │ ❌ ESP-IDF coupled             │ │
│  └────────────────────────────────┘ │
└─────────────────────────────────────┘
```

### Target Architecture (After)
```
┌─────────────┐
│   Apps      │
└──────┬──────┘
       │
┌──────▼──────────────────────────────┐
│  Presentation (Controllers)          │
│  ✅ Thin adapters (HTTP, CLI)       │
│  ✅ Depend on interfaces only       │
└──────┬──────────────────────────────┘
       │ ↓ Uses IStartSignalUseCase
┌──────▼──────────────────────────────┐
│  Use Cases (Application Services)    │
│  ✅ Business logic centralized      │
│  ✅ Validation, orchestration       │
└──────┬──────────────────────────────┘
       │ ↓ Uses ISignalController
┌──────▼──────────────────────────────┐
│  Domain (SignalEngine : IController) │
│  ✅ Implements interface            │
│  ✅ Pure domain logic               │
└──────┬──────────────────────────────┘
       │ ↓ Uses IEventPublisher, IConfigStorage
┌──────▼──────────────────────────────┐
│  Infrastructure Adapters             │
│  ✅ EspEventAdapter                 │
│  ✅ NvsConfigAdapter                │
│  ✅ Platform-specific code isolated │
└─────────────────────────────────────┘
```

---

## 9. Estimated Refactoring Effort

| Phase | Description | Files Affected | Effort | Risk |
|-------|-------------|----------------|--------|------|
| Phase 1 | Interface extraction | 8 files (headers + impls) | 4 hours | Low |
| Phase 2 | Use case layer | 12 files (new use cases + refactor) | 8 hours | Medium |
| Phase 3 | Event abstraction | 6 files (interface + adapters) | 4 hours | Low |
| Phase 4 | Config abstraction | 8 files (interface + adapters) | 6 hours | Medium |
| Phase 5 | Runtime config | 4 files (registry + main) | 4 hours | Low |
| **Total** | | **~38 files** | **26 hours** | **Medium** |

**Recommendation:** Implement in phases, with full testing after each phase.

---

## 10. Key Takeaways

### ✅ What's Good
1. **Clear module separation** - Distinct libraries for each concern
2. **Well-defined data contracts** - signal_iface.h provides clear types
3. **Hardware abstraction** - PulseGenerator encapsulates MCPWM
4. **Event-driven architecture** - Good foundation for reactive behavior

### ❌ What Needs Fixing
1. **No dependency inversion** - Presentation depends on concrete domain
2. **Missing use case layer** - Business logic scattered
3. **Framework coupling** - ESP-IDF dependencies leak into domain
4. **Build-time coupling** - Features not runtime-configurable
5. **No abstraction interfaces** - Cannot test in isolation

### 🎯 Primary Goal
**Invert dependencies** by introducing interfaces and use case layer, following the Dependency Inversion Principle and Clean Architecture patterns.

---

## Appendix A: Dependency Inversion Principle (DIP) Explained

### The Rule:
> "High-level modules should not depend on low-level modules. Both should depend on abstractions."

### Current Violation:
```cpp
// High-level module (WebFacade)
#include "SignalEngine.h"  // ❌ Depends on low-level module

class WebFacade {
    SignalEngine& _engine;  // ❌ Concrete dependency
};
```

### Correct Application:
```cpp
// High-level module (WebFacade)
#include "ISignalController.h"  // ✅ Depends on abstraction

class WebFacade {
    ISignalController& _controller;  // ✅ Interface dependency
};

// Low-level module (SignalEngine)
class SignalEngine : public ISignalController {  // ✅ Implements abstraction
    // ...
};
```

### Why This Matters:
1. **Testability** - Can inject mocks
2. **Flexibility** - Can swap implementations
3. **Maintainability** - Changes isolated to implementations
4. **Scalability** - Can add new implementations without changing callers

---

## Appendix B: File Path Reference

### Core Domain
- `/home/user/espgen/lib/SignalEngine/include/SignalEngine.h`
- `/home/user/espgen/lib/SignalEngine/include/PulseGenerator.h`
- `/home/user/espgen/lib/SignalEngine/src/SignalEngine.cpp`
- `/home/user/espgen/lib/SignalEngine/src/PulseGenerator.cpp`

### Presentation Layer
- `/home/user/espgen/lib/WebFacade/include/WebFacade.h`
- `/home/user/espgen/lib/WebFacade/include/ApiRouter.h`
- `/home/user/espgen/lib/WebFacade/include/WebSocketHub.h`
- `/home/user/espgen/lib/WebFacade/include/WifiMgr.h`
- `/home/user/espgen/lib/WebFacade/src/WebFacade.cpp`
- `/home/user/espgen/lib/WebFacade/src/ApiRouter.cpp`
- `/home/user/espgen/lib/SerialCLI/include/SerialCLI.h`
- `/home/user/espgen/lib/SerialCLI/src/SerialCLI.cpp`

### Infrastructure
- `/home/user/espgen/lib/OTABridge/include/OTAService.h`
- `/home/user/espgen/lib/OTABridge/src/OTAService.cpp`

### Common/Shared
- `/home/user/espgen/common/signal_iface.h`
- `/home/user/espgen/common/build_opts.h`
- `/home/user/espgen/common/param_helpers.h`
- `/home/user/espgen/common/preferences_keys.h`

### Applications
- `/home/user/espgen/apps/production_fw/main.cpp`
- `/home/user/espgen/apps/signal_cli/main.cpp`
- `/home/user/espgen/apps/web_mock/main.cpp`

### Configuration
- `/home/user/espgen/platformio.ini`

---

**End of Report**
