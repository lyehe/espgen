# Architecture Review - Documentation Index

**Project:** ESP32 Trigger (Multi-Channel Pulse Generator)
**Review Date:** 2025-11-09
**Reviewer:** Claude Code
**Branch:** claude/replace-wled-with-pul-011CUqVWrW1hLpGXbLd7StJM

---

## Overview

This architecture review analyzes the current codebase structure, identifies coupling issues and clean architecture violations, and provides a detailed refactoring roadmap to improve testability, maintainability, and adherence to SOLID principles.

---

## Generated Documents

### 📊 1. ARCHITECTURE_ANALYSIS.md
**Size:** 38 KB | **Focus:** Comprehensive architectural analysis

**Contents:**
- Executive summary of architecture health
- Current module dependency graph (visual ASCII diagrams)
- Detailed file-level dependency analysis
- Coupling points and violations
- Clean Architecture principle violations
- Before/After architecture comparison
- Estimated refactoring effort (26 hours)
- Dependency Inversion Principle explanation
- Complete file path reference

**Key Findings:**
- ✅ Good: Well-defined interfaces (`signal_iface.h`)
- ✅ Good: Hardware abstraction (PulseGenerator)
- ❌ Critical: Direct coupling from presentation to domain
- ❌ Critical: No abstraction layer for SignalEngine
- ❌ Critical: Missing use case layer (business logic scattered)

**Start here if:** You want a comprehensive understanding of the current architecture and its problems.

**Path:** `/home/user/espgen/ARCHITECTURE_ANALYSIS.md`

---

### 🔗 2. DEPENDENCY_GRAPH.txt
**Size:** 32 KB | **Focus:** Detailed dependency tracking

**Contents:**
- Complete dependency graph (all modules)
- File-by-file dependency breakdown
- Include statement analysis
- Method-level coupling documentation
- Data flow analysis (command flow, event flow, config flow)
- Critical coupling hotspots
- Dependency metrics and statistics
- Testability scoring

**Key Metrics:**
- Total files analyzed: 38
- Total dependencies: 127
- Critical violations: 3 instances
- Testability score: 20/100 (F grade)
- Architecture conformance: 35/100 (D grade)

**Start here if:** You need to understand what depends on what, or need to trace a specific dependency chain.

**Path:** `/home/user/espgen/DEPENDENCY_GRAPH.txt`

---

### 🛠️ 3. REFACTORING_ROADMAP.md
**Size:** 32 KB | **Focus:** Step-by-step refactoring guide

**Contents:**
- 5 refactoring phases with detailed steps
- Code examples (before/after)
- Interface design patterns
- Use case layer implementation guide
- Platform abstraction strategies
- Dependency injection setup
- Testing strategy with examples
- Risk mitigation plan
- Success metrics

**Phases:**
1. **Interface Extraction** (4 hours, Critical Priority)
2. **Use Case Layer** (8 hours, Critical Priority)
3. **Platform Abstraction** (6 hours, Moderate Priority)
4. **Dependency Injection** (4 hours, Low Priority)
5. **Testing Infrastructure** (12 hours, Moderate Priority)

**Start here if:** You're ready to start refactoring and need step-by-step instructions.

**Path:** `/home/user/espgen/REFACTORING_ROADMAP.md`

---

## Quick Summary

### Current Architecture Problems

```
┌────────────────┐      ┌────────────────┐
│   WebFacade    │      │   SerialCLI    │
│  (HTTP/WS)     │      │   (UART)       │
└────────┬───────┘      └────────┬───────┘
         │                       │
         │   ❌ TIGHT COUPLING   │
         └───────────┬───────────┘
                     ↓
          ┌──────────────────┐
          │  SignalEngine    │
          │ (Concrete Class) │
          │ ❌ No Interface  │
          └──────────────────┘
```

**Problems:**
1. **Violation of Dependency Inversion Principle (DIP)**
   - Presentation layers depend on concrete `SignalEngine` class
   - Cannot inject mocks for testing
   - Cannot swap implementations

2. **Missing Use Case Layer**
   - Business logic scattered in HTTP handlers and CLI parser
   - Code duplication between `ApiRouter` and `SerialCLI`
   - Validation logic repeated

3. **Platform Coupling**
   - Domain layer directly uses ESP-IDF APIs (esp_event, Preferences)
   - Cannot port to other platforms
   - Cannot test without ESP32 hardware

4. **Build-Time Feature Flags**
   - Features compiled in/out via `#if BUILD_WEB`
   - Requires separate builds for different configurations

---

### Target Architecture

```
┌───────────────┐      ┌───────────────┐
│ HttpController│      │ UartController│
└───────┬───────┘      └───────┬───────┘
        │                      │
        ↓ Uses Interface       ↓
┌──────────────────────────────────┐
│    IStartSignalUseCase           │
│    IStopSignalUseCase            │
└───────────┬──────────────────────┘
            ↓ Uses Interface
┌──────────────────────────────────┐
│    ISignalController             │
└───────────┬──────────────────────┘
            ↓ Implements
┌──────────────────────────────────┐
│    SignalEngine                  │
│    : ISignalController           │
└──────────────────────────────────┘
```

**Benefits:**
- ✅ All dependencies point to interfaces (DIP)
- ✅ Can inject mocks at every layer
- ✅ Business logic centralized in use cases
- ✅ No code duplication
- ✅ Platform-agnostic domain

---

## Critical Files to Examine

### Domain Layer (Core Business Logic)
```
/home/user/espgen/lib/SignalEngine/include/SignalEngine.h
/home/user/espgen/lib/SignalEngine/src/SignalEngine.cpp
/home/user/espgen/lib/SignalEngine/include/PulseGenerator.h
```

**Issue:** No interface abstraction, tightly coupled to ESP-IDF

---

### Presentation Layer (Tight Coupling)
```
/home/user/espgen/lib/WebFacade/include/WebFacade.h
/home/user/espgen/lib/WebFacade/include/ApiRouter.h
/home/user/espgen/lib/WebFacade/src/ApiRouter.cpp
/home/user/espgen/lib/SerialCLI/include/SerialCLI.h
/home/user/espgen/lib/SerialCLI/src/SerialCLI.cpp
```

**Issue:** Direct dependency on `SignalEngine` concrete class

---

### Common Layer (Interface Definitions)
```
/home/user/espgen/common/signal_iface.h
/home/user/espgen/common/build_opts.h
/home/user/espgen/common/param_helpers.h
```

**Issue:** `signal_iface.h` couples to ESP-IDF (`esp_event.h`)

---

### Applications (Wiring)
```
/home/user/espgen/apps/production_fw/main.cpp
/home/user/espgen/apps/signal_cli/main.cpp
```

**Issue:** Manual dependency injection, build-time flags

---

## Priority Action Items

### 🔴 CRITICAL (Do First)

**1. Create ISignalController Interface**
- **File:** Create `/home/user/espgen/common/ISignalController.h`
- **Effort:** 2 hours
- **Impact:** Breaks direct coupling between presentation and domain
- **See:** REFACTORING_ROADMAP.md, Phase 1, Step 1.1

**2. Make SignalEngine Implement Interface**
- **Files:** Modify `lib/SignalEngine/include/SignalEngine.h`
- **Effort:** 1 hour
- **Impact:** Allows dependency injection
- **See:** REFACTORING_ROADMAP.md, Phase 1, Step 1.2

**3. Update Presentation Layers to Use Interface**
- **Files:** Modify `ApiRouter.h`, `SerialCLI.h`, `WebFacade.h`
- **Effort:** 1 hour
- **Impact:** Inverts dependencies (presentation → interface ← domain)
- **See:** REFACTORING_ROADMAP.md, Phase 1, Steps 1.3-1.5

---

### 🟡 HIGH (Do Second)

**4. Create Use Case Layer**
- **Files:** Create `lib/UseCases/include/I*UseCase.h` and implementations
- **Effort:** 8 hours
- **Impact:** Centralizes business logic, eliminates code duplication
- **See:** REFACTORING_ROADMAP.md, Phase 2

---

### 🟢 MEDIUM (Do Third)

**5. Abstract Platform Dependencies**
- **Files:** Create `IEventPublisher.h`, `IConfigStorage.h`, adapters
- **Effort:** 6 hours
- **Impact:** Enables platform portability, improves testability
- **See:** REFACTORING_ROADMAP.md, Phase 3

---

## Testing Improvements

### Current State
- **Unit tests:** 0% (cannot mock SignalEngine)
- **Integration tests:** ~30%
- **Testability:** Impossible to test presentation layer in isolation

### After Phase 1 Refactoring
- **Unit tests:** Can test ApiRouter with mock controller
- **Isolated tests:** HTTP logic separate from domain logic
- **Fast tests:** No hardware required for presentation layer tests

### Example Test (After Refactoring)
```cpp
// test/presentation/test_api_router.cpp
TEST(ApiRouterTest, HandleTriggerPost_ValidInput_Calls Controller) {
    MockSignalController mockController;
    AsyncWebServer server(80);
    ApiRouter router(mockController, server);

    EXPECT_CALL(mockController, sendCommand(_))
        .WillOnce(Return(true));

    // Simulate HTTP POST request
    // ... test passes without real hardware!
}
```

---

## Code Duplication Examples

### Current Duplication (Bad)

**ApiRouter.cpp:**
```cpp
void ApiRouter::handleTriggerPost(...) {
    // ❌ Validation logic
    if (freqHz <= 0 || freqHz > 8000000) { /* error */ }

    // ❌ Command building
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = freqHz;
    cmd.dutyCycle = duty;
}
```

**SerialCLI.cpp:**
```cpp
void SerialCLI::parseAndExecute() {
    // ❌ SAME validation logic (duplicated!)
    if (freqHz <= 0 || freqHz > 8000000) { /* error */ }

    // ❌ SAME command building (duplicated!)
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = freqHz;
    cmd.dutyCycle = duty;
}
```

---

### After Refactoring (Good)

**ApiRouter.cpp:**
```cpp
void ApiRouter::handleTriggerPost(...) {
    // ✅ Delegates to use case (no business logic)
    StartSignalInput input{freqHz, duty, duration};
    StartSignalOutput output = _startUseCase.execute(input);
}
```

**SerialCLI.cpp:**
```cpp
void SerialCLI::parseAndExecute() {
    // ✅ Uses SAME use case (no duplication!)
    StartSignalInput input{freqHz, duty, duration};
    StartSignalOutput output = _startUseCase.execute(input);
}
```

**StartSignalUseCase.cpp (SINGLE source of truth):**
```cpp
StartSignalOutput StartSignalUseCase::execute(const StartSignalInput& input) {
    // ✅ Validation in ONE place
    if (input.frequencyHz <= 0 || input.frequencyHz > 8000000) {
        return {false, SIG_ERR_INVALID_PARAM, "Invalid frequency"};
    }

    // ✅ Command building in ONE place
    SignalCmd cmd = createStartCmd_FreqDuty(input.frequencyHz, input.dutyCycle, input.durationSec);

    return {_controller.sendCommand(cmd), SIG_OK, "OK"};
}
```

---

## Metrics Comparison

| Metric | Before | After Phase 1 | After All Phases |
|--------|--------|---------------|------------------|
| **Testability Score** | 20/100 | 60/100 | 90/100 |
| **Architecture Conformance** | 35/100 | 65/100 | 85/100 |
| **Unit Test Coverage** | 0% | 40% | 80%+ |
| **Code Duplication** | High | Medium | None |
| **Platform Coupling** | High | High | Low |
| **Dependency Violations** | 3 critical | 0 critical | 0 |

---

## Recommended Reading Order

1. **Start here:** ARCHITECTURE_ANALYSIS.md (Executive Summary)
2. **Understand dependencies:** DEPENDENCY_GRAPH.txt (Section 3: Coupling Points)
3. **Ready to refactor:** REFACTORING_ROADMAP.md (Phase 1: Interface Extraction)
4. **Deep dive:** ARCHITECTURE_ANALYSIS.md (Section 5: Recommended Refactoring)
5. **Reference:** DEPENDENCY_GRAPH.txt (File-by-file dependencies)

---

## Key Takeaways

### What's Good ✅
1. Clear module separation (lib/SignalEngine, lib/WebFacade, etc.)
2. Well-defined data contracts (signal_iface.h)
3. Hardware abstraction (PulseGenerator)
4. Event-driven architecture foundation

### What Needs Fixing ❌
1. **No dependency inversion** - Presentation depends on concrete domain
2. **Missing use case layer** - Business logic scattered and duplicated
3. **Platform coupling** - ESP-IDF dependencies in domain layer
4. **No abstraction interfaces** - Cannot test in isolation
5. **Build-time configuration** - Should be runtime

### Primary Goal 🎯
**Invert all dependencies by introducing interfaces and use case layer.**

**Result:** Testable, maintainable, flexible architecture following Clean Architecture and SOLID principles.

---

## Questions?

**Q: Where do I start?**
A: Read ARCHITECTURE_ANALYSIS.md Executive Summary, then Phase 1 of REFACTORING_ROADMAP.md

**Q: Will this break existing code?**
A: No! Phase 1 is backward compatible. Existing apps continue to work unchanged.

**Q: How long will this take?**
A: Phase 1 (critical): 4 hours. All phases: 26 hours total.

**Q: What's the biggest win?**
A: Testability. After Phase 1, you can write unit tests for presentation layer without hardware.

**Q: What's the risk?**
A: Low. Each phase is independently deployable. Can ship after Phase 1 and still have improvements.

---

## Contact

For questions or clarification on this architecture review, reference:
- Branch: `claude/replace-wled-with-pul-011CUqVWrW1hLpGXbLd7StJM`
- Review Date: 2025-11-09
- Documents: This directory

---

**END OF ARCHITECTURE REVIEW**
