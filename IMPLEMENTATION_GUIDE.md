# Implementation Guide: API Endpoints and CLI Commands

This document provides complete implementation details for adding API endpoints and CLI commands for Performance Metrics and Configuration Presets.

## Status Summary

### ✅ Completed (2 commits)
1. **Critical Architecture Fixes** (commit 94408b6):
   - SignalEngine uses IPulseGenerator interface (DIP compliance)
   - SignalStatus_t fully populated (periodUs, pulseWidthUs)
   - PerformanceMonitor integrated into CommandDispatcher

2. **Application Layer** (commit c78f388):
   - IPerformanceService and IPresetService interfaces
   - SignalEngine preset methods
   - SignalControllerAdapter implementations

### ⏳ TODO (Presentation Layer)
3. **API Endpoints** - Add to ApiRouter.cpp
4. **CLI Commands** - Add to SerialCLI.cpp
5. **Minor Fixes** - WebFacade, documentation

---

## Part 1: API Endpoints

### File to Modify
`/home/user/espgen/lib/WebFacade/src/ApiRouter.cpp`

### 1.1 Add GET /api/metrics

**Location**: In `registerRoutes()` method, add:

```cpp
// GET /api/metrics - Performance metrics
_server.on("/api/metrics", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handleMetricsGet(request);
});
```

**Add handler method** (in same file, after existing handlers):

```cpp
void ApiRouter::handleMetricsGet(AsyncWebServerRequest *request) {
    PerformanceMetrics metrics;
    _controller.getPerformanceMetrics(metrics);

    JsonDocument doc;
    doc["uptime_ms"] = metrics.uptimeMs;
    doc["uptime_hours"] = metrics.uptimeMs / 3600000.0;

    JsonObject commands = doc["commands"].to<JsonObject>();
    commands["total"] = metrics.totalCommands;
    commands["avg_latency_us"] = metrics.avgCommandLatencyUs;
    commands["min_latency_us"] = metrics.minCommandLatencyUs;
    commands["max_latency_us"] = metrics.maxCommandLatencyUs;

    JsonObject events = doc["events"].to<JsonObject>();
    events["total"] = metrics.totalEvents;
    events["failed"] = metrics.failedEvents;
    if (metrics.totalEvents > 0) {
        events["success_rate"] = (metrics.totalEvents - metrics.failedEvents) * 100.0 / metrics.totalEvents;
    }

    JsonObject memory = doc["memory"].to<JsonObject>();
    memory["free_heap"] = metrics.freeHeap;
    memory["min_free_heap"] = metrics.minFreeHeap;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
    Serial.println("Sent GET /api/metrics response");
}
```

**Add to ApiRouter.h**:

```cpp
void handleMetricsGet(AsyncWebServerRequest *request);
```

---

### 1.2 Add GET /api/presets

**In registerRoutes()**:

```cpp
// GET /api/presets - List all presets
_server.on("/api/presets", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->handlePresetsGet(request);
});
```

**Handler**:

```cpp
void ApiRouter::handlePresetsGet(AsyncWebServerRequest *request) {
    char buffer[256];
    int count = _controller.listPresets(buffer, sizeof(buffer));

    JsonDocument doc;
    JsonArray presets = doc["presets"].to<JsonArray>();
    doc["count"] = count;

    if (count > 0) {
        // Split comma-separated list into array
        String listStr = String(buffer);
        int startIdx = 0;
        while (startIdx < listStr.length()) {
            int commaIdx = listStr.indexOf(',', startIdx);
            if (commaIdx == -1) {
                // Last item
                presets.add(listStr.substring(startIdx));
                break;
            } else {
                presets.add(listStr.substring(startIdx, commaIdx));
                startIdx = commaIdx + 1;
            }
        }
    }

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
    Serial.println("Sent GET /api/presets response");
}
```

---

### 1.3 Add POST /api/preset/save

**In registerRoutes()**:

```cpp
// POST /api/preset/save - Save current config as preset
_server.on("/api/preset/save", HTTP_POST,
    [](AsyncWebServerRequest *request){
        // Header handler
        if (!request->_tempObject) {
            request->_tempObject = new RequestBodyState();
        }
    },
    NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Body handler
        RequestBodyState* state = static_cast<RequestBodyState*>(request->_tempObject);
        if (state) {
            if (index == 0) {
                state->buffer.clear();
                state->buffer.reserve(total + 1);
            }
            state->buffer.insert(state->buffer.end(), data, data + len);
        }

        if (index + len == total) {
            if (state) {
                state->buffer.push_back(0);

                JsonDocument jsonDoc;
                DeserializationError error = deserializeJson(jsonDoc, state->buffer.data());

                if (error) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                } else {
                    JsonVariant jsonVariant = jsonDoc.as<JsonVariant>();
                    this->handlePresetSavePost(request, jsonVariant);
                }

                delete state;
                request->_tempObject = nullptr;
            }
        }
    }
);
```

**Handler**:

```cpp
void ApiRouter::handlePresetSavePost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();

    if (!obj || !obj["name"].is<const char*>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'name' field\"}");
        return;
    }

    const char* name = obj["name"];

    if (strlen(name) == 0 || strlen(name) > 15) {
        request->send(400, "application/json", "{\"error\":\"Preset name must be 1-15 characters\"}");
        return;
    }

    if (_controller.savePreset(name)) {
        JsonDocument doc;
        doc["status"] = "saved";
        doc["name"] = name;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
        Serial.printf("Preset '%s' saved\n", name);
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save preset\"}");
    }
}
```

---

### 1.4 Add POST /api/preset/load

**In registerRoutes()** (similar pattern to save):

```cpp
// POST /api/preset/load - Load and apply a preset
_server.on("/api/preset/load", HTTP_POST,
    [](AsyncWebServerRequest *request){ /* same as save */ },
    NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Same body handler pattern as save
        // Call handlePresetLoadPost at the end
    }
);
```

**Handler**:

```cpp
void ApiRouter::handlePresetLoadPost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();

    if (!obj || !obj["name"].is<const char*>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'name' field\"}");
        return;
    }

    const char* name = obj["name"];

    if (!_controller.presetExists(name)) {
        request->send(404, "application/json", "{\"error\":\"Preset not found\"}");
        return;
    }

    if (_controller.loadPreset(name)) {
        JsonDocument doc;
        doc["status"] = "loaded";
        doc["name"] = name;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
        Serial.printf("Preset '%s' loaded and applied\n", name);
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to load preset\"}");
    }
}
```

---

### 1.5 Add DELETE /api/preset/delete

**In registerRoutes()**:

```cpp
// DELETE /api/preset/delete - Delete a preset
_server.on("/api/preset/delete", HTTP_DELETE,
    [](AsyncWebServerRequest *request){ /* same pattern */ },
    NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Same body handler, call handlePresetDeletePost
    }
);
```

**Handler**:

```cpp
void ApiRouter::handlePresetDeletePost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();

    if (!obj || !obj["name"].is<const char*>()) {
        request->send(400, "application/json", "{\"error\":\"Missing 'name' field\"}");
        return;
    }

    const char* name = obj["name"];

    if (_controller.deletePreset(name)) {
        JsonDocument doc;
        doc["status"] = "deleted";
        doc["name"] = name;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
        Serial.printf("Preset '%s' deleted\n", name);
    } else {
        request->send(404, "application/json", "{\"error\":\"Preset not found\"}");
    }
}
```

---

### ApiRouter.h Updates

Add these method declarations to the class:

```cpp
void handleMetricsGet(AsyncWebServerRequest *request);
void handlePresetsGet(AsyncWebServerRequest *request);
void handlePresetSavePost(AsyncWebServerRequest *request, JsonVariant &json);
void handlePresetLoadPost(AsyncWebServerRequest *request, JsonVariant &json);
void handlePresetDeletePost(AsyncWebServerRequest *request, JsonVariant &json);
```

---

## Part 2: CLI Commands

### File to Modify
`/home/user/espgen/lib/SerialCLI/src/SerialCLI.cpp`

### 2.1 Add "metrics" Command

**In parseAndExecute()**, after existing commands:

```cpp
} else if (_inputBuffer == "metrics") {
    PerformanceMetrics metrics;
    _controller.getPerformanceMetrics(metrics);

    Serial.println("========== Performance Metrics ==========");
    Serial.printf("Uptime: %llu ms (%.2f hours)\n",
                  metrics.uptimeMs,
                  metrics.uptimeMs / 3600000.0);
    Serial.println();

    Serial.println("Commands:");
    Serial.printf("  Total: %llu\n", metrics.totalCommands);
    if (metrics.totalCommands > 0) {
        Serial.printf("  Avg Latency: %llu us (%.2f ms)\n",
                      metrics.avgCommandLatencyUs,
                      metrics.avgCommandLatencyUs / 1000.0);
        Serial.printf("  Min Latency: %llu us\n", metrics.minCommandLatencyUs);
        Serial.printf("  Max Latency: %llu us\n", metrics.maxCommandLatencyUs);
    }
    Serial.println();

    Serial.println("Events:");
    Serial.printf("  Total: %llu\n", metrics.totalEvents);
    Serial.printf("  Failed: %llu\n", metrics.failedEvents);
    if (metrics.totalEvents > 0) {
        Serial.printf("  Success Rate: %.1f%%\n",
                      (metrics.totalEvents - metrics.failedEvents) * 100.0 / metrics.totalEvents);
    }
    Serial.println();

    Serial.println("Memory:");
    Serial.printf("  Free Heap: %u bytes (%.2f KB)\n",
                  metrics.freeHeap,
                  metrics.freeHeap / 1024.0);
    Serial.printf("  Min Free Heap: %u bytes (%.2f KB)\n",
                  metrics.minFreeHeap,
                  metrics.minFreeHeap / 1024.0);
    Serial.println("=========================================");
```

---

### 2.2 Add "savepreset <name>" Command

```cpp
} else if (_inputBuffer.startsWith("savepreset ")) {
    String name = _inputBuffer.substring(11); // Skip "savepreset "
    name.trim();

    if (name.length() == 0 || name.length() > 15) {
        Serial.println("Error: Preset name must be 1-15 characters");
        return;
    }

    if (_controller.savePreset(name.c_str())) {
        Serial.printf("Preset '%s' saved successfully\n", name.c_str());
    } else {
        Serial.println("Error: Failed to save preset");
    }
```

---

### 2.3 Add "loadpreset <name>" Command

```cpp
} else if (_inputBuffer.startsWith("loadpreset ")) {
    String name = _inputBuffer.substring(11);
    name.trim();

    if (name.length() == 0) {
        Serial.println("Error: Preset name required");
        return;
    }

    if (!_controller.presetExists(name.c_str())) {
        Serial.printf("Error: Preset '%s' not found\n", name.c_str());
        return;
    }

    if (_controller.loadPreset(name.c_str())) {
        Serial.printf("Preset '%s' loaded and applied\n", name.c_str());
    } else {
        Serial.println("Error: Failed to load preset");
    }
```

---

### 2.4 Add "delpreset <name>" Command

```cpp
} else if (_inputBuffer.startsWith("delpreset ")) {
    String name = _inputBuffer.substring(10);
    name.trim();

    if (name.length() == 0) {
        Serial.println("Error: Preset name required");
        return;
    }

    if (_controller.deletePreset(name.c_str())) {
        Serial.printf("Preset '%s' deleted\n", name.c_str());
    } else {
        Serial.printf("Error: Preset '%s' not found\n", name.c_str());
    }
```

---

### 2.5 Add "listpresets" Command

```cpp
} else if (_inputBuffer == "listpresets") {
    char buffer[256];
    int count = _controller.listPresets(buffer, sizeof(buffer));

    if (count == 0) {
        Serial.println("No presets saved");
    } else {
        Serial.printf("Saved presets (%d):\n", count);

        String listStr = String(buffer);
        int startIdx = 0;
        int num = 1;
        while (startIdx < listStr.length()) {
            int commaIdx = listStr.indexOf(',', startIdx);
            if (commaIdx == -1) {
                Serial.printf("  %d. %s\n", num, listStr.substring(startIdx).c_str());
                break;
            } else {
                Serial.printf("  %d. %s\n", num, listStr.substring(startIdx, commaIdx).c_str());
                startIdx = commaIdx + 1;
                num++;
            }
        }
    }
```

---

### 2.6 Update Help Text

In the `help` command handler, add:

```cpp
Serial.println("Performance:");
Serial.println("  metrics            - Show performance metrics");
Serial.println();
Serial.println("Presets:");
Serial.println("  savepreset <name>  - Save current config as preset");
Serial.println("  loadpreset <name>  - Load and apply a preset");
Serial.println("  delpreset <name>   - Delete a preset");
Serial.println("  listpresets        - List all saved presets");
```

---

## Part 3: Minor Fixes

### 3.1 Fix WebFacade LittleFS TODO

**File**: `/home/user/espgen/lib/WebFacade/src/WebFacade.cpp`

**Line 30**: Replace the TODO with proper error handling:

```cpp
if (!LittleFS.begin(true)) {  // true = format if mount fails
    Serial.println("CRITICAL: LittleFS mount failed even after format!");
    Serial.println("Web interface files will be unavailable");
    Serial.println("Check if LittleFS partition is defined in partition table");
    // Continue anyway - server will run but files won't be served
    // You could also return false here to fail initialization
}
```

---

### 3.2 Update WHATS_NEXT.md

**File**: `/home/user/espgen/WHATS_NEXT.md`

Update the completed items:

```markdown
## Recently Completed ✅

### Architecture Improvements
- ✅ SignalEngine uses IPulseGenerator interface (DIP compliance)
- ✅ PerformanceMonitor fully integrated
- ✅ Configuration presets system implemented
- ✅ Backend getters completed (all channel info available)
- ✅ SignalStatus_t fully populated
- ✅ Thread safety verified and documented

### New Features
- ✅ Performance monitoring (command latency, memory, events)
- ✅ Configuration presets (save/load/delete/list)
- ✅ Application layer interfaces (IPerformanceService, IPresetService)

## Next Priorities

### High Priority
1. **API Integration** - Add preset and metrics endpoints
2. **CLI Integration** - Add preset and metrics commands
3. **Hardware Testing** - Test on real ESP32

### Medium Priority
4. **Unit Tests** - Expand coverage to 80%+
5. **Documentation** - API documentation for new endpoints
```

---

## Testing Checklist

After implementing API and CLI:

### API Tests
- [ ] GET /api/metrics returns valid JSON
- [ ] GET /api/presets lists saved presets
- [ ] POST /api/preset/save creates preset
- [ ] POST /api/preset/load applies preset
- [ ] DELETE /api/preset/delete removes preset

### CLI Tests
- [ ] `metrics` command shows performance data
- [ ] `savepreset test1` saves preset
- [ ] `listpresets` shows saved presets
- [ ] `loadpreset test1` applies preset
- [ ] `delpreset test1` removes preset
- [ ] `help` shows new commands

---

## Estimated Implementation Time

| Task | Time |
|------|------|
| API Endpoints (5) | 2-3 hours |
| CLI Commands (5) | 1-2 hours |
| Testing | 1 hour |
| Minor Fixes | 30 min |
| **Total** | **4-6 hours** |

---

## Architecture Summary

```
Presentation Layer (API/CLI)
         ↓
  ISignalController
         ↓
SignalControllerAdapter
         ↓
    SignalEngine
         ↓
   _commandDispatcher (PerformanceMonitor)
   _persistence (Presets)
```

All the hard work is done - just need to wire up the presentation layer!
