# What's Next - Development Roadmap

**Current Status**: ✅ Clean Architecture + Comprehensive API/Frontend Complete
**Last Update**: 2025-11-09

---

## 🎯 Immediate Priorities (Recommended Order)

### 1. **Add Backend Getters for Channel Info** ⚠️ HIGH PRIORITY
**Status**: Required for GET /api/channels to return real data

**Current Issue**:
- GET /api/channels returns dummy data (ApiRouter.cpp:531-539)
- Comment says: "TODO: Add getters to SignalEngine/PulseGenerator"

**What to Do**:
```cpp
// Add to PulseGenerator.h
bool getChannelConfig(uint8_t channel_id, PulseChannelConfig_t& config) const;
uint8_t getChannelPin(uint8_t channel_id) const;
float getPhaseOffset(uint8_t channel_id) const;
bool isChannelEnabled(uint8_t channel_id) const;
SignalPolarity getPolarity(uint8_t channel_id) const;
```

**Files to Modify**:
- `lib/SignalEngine/include/PulseGenerator.h` - Add getter declarations
- `lib/SignalEngine/src/PulseGenerator.cpp` - Implement getters
- `lib/SignalEngine/include/SignalEngine.h` - Expose getters
- `lib/SignalEngine/src/SignalEngine.cpp` - Delegate to PulseGenerator
- `lib/Application/SignalControllerAdapter.h` - Use new getters
- `lib/WebFacade/src/ApiRouter.cpp` - Return real channel data

**Estimated Time**: 2-3 hours

**Benefits**:
- ✅ Frontend can display actual channel configurations
- ✅ Complete REST API implementation
- ✅ Better debugging and monitoring

---

### 2. **Add Quick Channel Enable/Disable** ⚠️ MEDIUM PRIORITY
**Status**: SIG_CMD_ENABLE_CHANNEL exists but not exposed

**What to Do**:

**API Endpoint**:
```cpp
// Add to ApiRouter.cpp
POST /api/channel/{id}/enable
{
  "enabled": true
}
```

**Frontend UI**:
```html
<!-- Add to each channel tab -->
<label>
  <input type="checkbox" id="ch1-enabled-quick" onchange="toggleChannel(1, this.checked)">
  Quick Enable/Disable
</label>
```

**Serial CLI**:
```bash
> enable <channel> <0|1>
```

**Files to Modify**:
- `lib/WebFacade/src/ApiRouter.cpp` - Add endpoint
- `lib/WebFacade/data/index.html` - Add UI controls
- `lib/WebFacade/data/js/app-advanced.js` - Add toggleChannel()
- `lib/SerialCLI/src/SerialCLI.cpp` - Add enable command

**Estimated Time**: 1-2 hours

**Benefits**:
- ✅ Quick channel toggling without full reconfiguration
- ✅ Better user experience
- ✅ Complete SIG_CMD_ENABLE_CHANNEL exposure

---

### 3. **Hardware Testing** 🔴 CRITICAL
**Status**: Code is ready, needs validation on real ESP32

**Test Plan**:

**Phase 1: Basic Functionality**
- [ ] Compile and upload firmware to ESP32
- [ ] Test WiFi connection and provisioning
- [ ] Test mDNS (http://trigger.local)
- [ ] Test basic start/stop via web UI
- [ ] Test serial CLI basic commands

**Phase 2: Multi-Channel**
- [ ] Configure 2 channels with phase offset
- [ ] Verify phase relationship with oscilloscope
- [ ] Test sync trigger
- [ ] Measure timing accuracy

**Phase 3: Advanced Features**
- [ ] Test exact parameters (period, pulse width)
- [ ] Test pulse count mode
- [ ] Test polarity (active-high/low)
- [ ] Test indicator pin

**Phase 4: Edge Cases**
- [ ] Test invalid inputs (error handling)
- [ ] Test command queue overflow
- [ ] Test WiFi reconnection
- [ ] Test OTA updates

**Equipment Needed**:
- ESP32 development board
- Oscilloscope (2+ channels for multi-channel testing)
- Logic analyzer (optional, for precise timing)
- Test loads/LEDs

**Estimated Time**: 4-6 hours

---

## 🚀 Next Features (Priority Order)

### 4. **Add Unit Tests** 🟡 HIGH VALUE
**Status**: Clean architecture now enables testing

**What to Do**:

**Test Framework Setup**:
```ini
# Add to platformio.ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++11
```

**Example Tests**:
```cpp
// test/test_application/test_signal_service.cpp
#include <unity.h>
#include "ISignalController.h"

class MockController : public ISignalController {
public:
    int startCalls = 0;
    SignalCmd lastCmd;

    bool startSignal(const SignalCmd& cmd) override {
        startCalls++;
        lastCmd = cmd;
        return true;
    }
    // ... implement other methods
};

void test_start_command_increments_counter() {
    MockController mock;
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;

    mock.startSignal(cmd);

    TEST_ASSERT_EQUAL(1, mock.startCalls);
}

void test_start_command_records_parameters() {
    MockController mock;
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_START;
    cmd.frequencyHz = 1000;

    mock.startSignal(cmd);

    TEST_ASSERT_EQUAL(1000, mock.lastCmd.frequencyHz);
}
```

**Test Coverage Goals**:
- [ ] Application layer (adapters, services) - 80%+
- [ ] Presentation layer (ApiRouter, SerialCLI) - 60%+
- [ ] Domain layer (SignalEngine logic) - 40%+

**Files to Create**:
- `test/test_application/test_signal_service.cpp`
- `test/test_presentation/test_api_router.cpp`
- `test/test_presentation/test_serial_cli.cpp`
- `test/mocks/MockSignalController.h`

**Estimated Time**: 6-8 hours

**Benefits**:
- ✅ Catch bugs before hardware testing
- ✅ Refactor with confidence
- ✅ Faster development cycle
- ✅ Documentation via tests

---

### 5. **Performance Optimization** 🟢 LOW PRIORITY (Unless Issues Found)

**What to Measure**:
- Command processing latency
- WebSocket update frequency
- Memory usage (heap fragmentation)
- Task stack usage
- Command queue depth

**Tools**:
```cpp
// Add to SignalEngine.cpp
#define ENABLE_PERFORMANCE_METRICS

#if ENABLE_PERFORMANCE_METRICS
static uint32_t cmdProcessTime = 0;
static uint32_t maxCmdTime = 0;

void SignalEngine::cmdDispatcherTask() {
    uint32_t start = micros();
    // ... process command ...
    cmdProcessTime = micros() - start;
    if (cmdProcessTime > maxCmdTime) {
        maxCmdTime = cmdProcessTime;
        Serial.printf("New max cmd time: %lu us\n", maxCmdTime);
    }
}
#endif
```

**Estimated Time**: 2-4 hours (if needed)

---

### 6. **Enhanced Status Display & Monitoring** 🟢 NICE TO HAVE

**What to Add**:

**Real-time Pulse Counter**:
```javascript
// WebSocket event handler
if (message.event === 'pulse_tick') {
    document.getElementById('pulse-count').textContent = message.count;
}
```

**Runtime Timer**:
```javascript
setInterval(() => {
    if (isRunning) {
        runtime++;
        document.getElementById('runtime').textContent = formatTime(runtime);
    }
}, 1000);
```

**Channel Status Indicators**:
```html
<div class="channel-status">
    <div class="led" id="ch0-led"></div> CH0
    <div class="led" id="ch1-led"></div> CH1
    <div class="led" id="ch2-led"></div> CH2
    <!-- ... -->
</div>
```

**Estimated Time**: 2-3 hours

---

### 7. **Configuration Presets** 🟢 POWER USER FEATURE

**What to Add**:

**Backend**:
```cpp
// Add to common/signal_iface.h
struct SignalPreset {
    char name[32];
    SignalCmd config;
    uint8_t channelCount;
    PulseChannelConfig_t channels[MAX_PULSE_CHANNELS];
};

// Save/load with Preferences
bool savePreset(uint8_t slot, const SignalPreset& preset);
bool loadPreset(uint8_t slot, SignalPreset& preset);
```

**API**:
```
POST /api/preset/save
GET /api/preset/load/{slot}
GET /api/presets
```

**Frontend**:
```html
<select id="preset-selector">
    <option value="0">Custom</option>
    <option value="1">Quadrature 1kHz</option>
    <option value="2">Three-Phase 60Hz</option>
    <option value="3">Camera Trigger</option>
</select>
```

**Estimated Time**: 4-5 hours

---

## 🎓 Advanced Features (Future)

### 8. **Burst Mode Implementation**
**Status**: Defined in signal_iface.h but not implemented

```cpp
struct SignalCmd {
    // ...
    uint8_t burstCount;      // Pulses per burst
    uint32_t burstPeriodUs;  // Time between bursts
};
```

**Use Cases**:
- Camera flash synchronization
- Sonar/Radar pulse groups
- Communication protocols

**Estimated Time**: 6-8 hours

---

### 9. **Start Delay Implementation**
**Status**: Defined but not implemented

```cpp
struct SignalCmd {
    uint32_t startDelayUs; // Delay before first pulse
};
```

**Use Cases**:
- Synchronized multi-device triggers
- Event countdown
- Precise timing sequences

**Estimated Time**: 2-3 hours

---

### 10. **MQTT/IoT Integration**
**Status**: Clean architecture makes this easy now

**What to Add**:
```cpp
// lib/MqttBridge/MqttBridge.h
class MqttBridge {
public:
    MqttBridge(ISignalController& controller);
    void begin();
    void loop();
private:
    ISignalController& _controller;
    // MQTT client
};
```

**Topics**:
```
trigger/command    (subscribe)
trigger/status     (publish)
trigger/events     (publish)
```

**Estimated Time**: 4-6 hours

---

### 11. **Bluetooth Interface**
**Status**: Another presentation layer option

```cpp
// lib/BleBridge/BleBridge.h
class BleBridge {
public:
    BleBridge(ISignalController& controller);
    void begin();
private:
    ISignalController& _controller;
    // BLE characteristics
};
```

**Estimated Time**: 6-8 hours

---

### 12. **Web UI Enhancements**

**Features to Add**:
- [ ] Waveform visualization (SVG or Canvas)
- [ ] Configuration import/export (JSON)
- [ ] Dark mode toggle
- [ ] Responsive mobile layout
- [ ] Progressive Web App (PWA) support
- [ ] WebSerial API support (direct USB)

**Estimated Time**: 8-10 hours

---

## 📋 Maintenance & Documentation

### 13. **Complete API Documentation**
- [ ] OpenAPI/Swagger specification
- [ ] Example client code (Python, curl, JavaScript)
- [ ] Postman collection

### 14. **User Guide**
- [ ] Getting started tutorial
- [ ] Use case examples with diagrams
- [ ] Troubleshooting guide
- [ ] FAQ

### 15. **Developer Guide**
- [ ] Architecture deep-dive
- [ ] Adding new features guide
- [ ] Testing guide
- [ ] Contribution guidelines

---

## 🎯 Recommended Next Steps

**This Week**:
1. ✅ Add backend getters for channel info (2-3 hours)
2. ✅ Test on hardware (4-6 hours)
3. ✅ Fix any bugs found during testing

**Next Week**:
1. Add quick channel enable/disable (1-2 hours)
2. Start unit test framework (6-8 hours)
3. Enhanced status display (2-3 hours)

**Month 1**:
1. Complete unit test coverage (80%+)
2. Performance optimization (if needed)
3. Configuration presets

**Month 2+**:
1. Burst mode implementation
2. MQTT/IoT integration
3. Advanced UI features

---

## 🔥 Quick Wins (< 1 hour each)

If you want to make quick improvements:

1. **Add GET /api/indicator** - Return current indicator pin
2. **Add Serial CLI 'channels' command** - List all channel configs
3. **Add API versioning** - /api/v2/ for future changes
4. **Add request rate limiting** - Prevent API abuse
5. **Add CORS configuration** - Allow cross-origin requests
6. **Add API authentication** - Basic auth or API keys
7. **Add system info endpoint** - GET /api/system (firmware version, uptime, etc.)

---

## 📊 Success Metrics

Track these to measure progress:

- **Test Coverage**: Target 80%+
- **API Response Time**: < 50ms average
- **Memory Usage**: < 70% heap
- **Command Queue**: Never full
- **WiFi Stability**: > 99.9% uptime
- **WebSocket Latency**: < 100ms
- **User Issues**: < 5 per month
- **Documentation**: 100% API coverage

---

## 🎉 Summary

**Immediate**: Add getters → Hardware testing → Bug fixes
**Short-term**: Unit tests → Quick enable/disable → Enhanced UI
**Long-term**: Burst mode → MQTT → Advanced features

**Next Command**:
```bash
# Start with adding backend getters
# Open PulseGenerator.h and add the getter declarations
```

Let me know which direction you'd like to go, and I'll help implement it!
