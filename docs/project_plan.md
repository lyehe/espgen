Below is an **expanded, implementation‑ready plan** that weaves Wi‑Fi provisioning, a richer web stack, the Serial CLI, and an explicit phased roadmap into the modular architecture we sketched earlier. Every step can be compiled and bench‑tested on its own, so you never block the real‑time work while finishing the UI—or vice‑versa.

---

## 1 · Updated Repository Tree (granular)

```
esp32-trigger/
│
├─ platformio.ini
│   ├─ [env:signal‑test]      # libSignalEngine + Serial only
│   ├─ [env:web‑test]         # libWebFacade (mock engine)
│   └─ [env:prod]             # full stack + OTA
│       lib_deps =
│           ESP32Async/ESPAsyncWebServer @ ^3.6.0   # For web server
│           ESP32Async/AsyncTCP @ ^3.4.0            # Required by ESPAsyncWebServer
│           bblanchon/ArduinoJson @ ^7.4.1          # For parsing/generating JSON
│           tzapu/WiFiManager @ ^2.0.17             # For WiFi provisioning portal
│           ayushsharma82/ElegantOTA @ ^3.1.7       # For OTA updates (wrapper used)
│           khoih-prog/ESP32TimerInterrupt @ ^2.3.0 # (Existing dependency - keeping it)
│
├─ common/
│   ├─ signal_iface.h         # Cmd/Evt structs, error codes
│   └─ build_opts.h           # Global #defines (e.g., BUILD_WEB)
│
├─ lib/
│   ├─ SignalEngine/
│   │   ├─ include/
│   │   │   ├─ SignalEngine.h
│   │   │   ├─ LedcDriver.h
│   │   │   ├─ RmtDriver.h
│   │   │   └─ EngineConfig.h
│   │   └─ src/
│   │       ├─ SignalEngine.cpp
│   │       ├─ LedcDriver.cpp
│   │       ├─ RmtDriver.cpp
│   │       ├─ CmdDispatcher.cpp
│   │       └─ modulators/          # (EMPTY stubs for future PWM tricks)
│   │
│   ├─ SerialCLI/
│   │   ├─ include/SerialCLI.h
│   │   └─ src/{SerialCLI.cpp,CliHelpTable.cpp}
│   │
│   ├─ WebFacade/
│   │   ├─ include/
│   │   │   ├─ WebFacade.h
│   │   │   ├─ ApiRouter.h
│   │   │   ├─ WebSocketHub.h
│   │   │   └─ WifiMgr.h             # captive portal wrapper
│   │   ├─ src/
│   │   │   ├─ WebFacade.cpp
│   │   │   ├─ ApiRouter.cpp
│   │   │   ├─ WebSocketHub.cpp
│   │   │   └─ WifiMgr.cpp
│   │   ├─ data/                     # LittleFS site
│   │   │   ├─ index.html
│   │   │   ├─ app.js
│   │   │   ├─ css/style.css
│   │   │   └─ js/{api.js,ws.js}
│   │   └─ test/test_rest_parser.cpp
│   │
│   └─ OTABridge/ (optional)         # AsyncElegantOTA wrapper
│       ├─ include/OTAService.h
│       └─ src/OTAService.cpp
│
├─ apps/
│   ├─ signal_cli/main.cpp
│   ├─ web_mock/main.cpp
│   └─ production_fw/main.cpp
│
└─ docs/   # Doxygen configs, design RFCs, API schema (OpenAPI)
```

*New in this tree*: **WifiMgr.*​** files centralise provisioning and store credentials in NVS (`Preferences`), so WebFacade never touches Wi‑Fi directly. citeturn0search8turn0search3

---

## 2 · Web & Wi‑Fi Configuration Flow

1. **Boot**  
   * `WifiMgr.begin()` tries saved STA creds (10 s timeout).  
   * If success → mDNS "trigger.local", DHCP or static IP (config in `EngineConfig.h`).  
   * If fail → starts captive portal (`WiFi.softAP("TRIGGER‑SETUP")`) with WiFiManager UI. citeturn0search8

2. **Captive portal pages** (auto‑served by WiFiManager)  
   * `/wifi` – scan & select SSID, enter PSK.  
   * `/config` – extra fields the project injects (hostname, static‑IP toggle).  
   * On save: creds stored in NVS; ESP32 reboots.

3. **Operational Web UI** (`/index.html` served by Async FS)  
   * REST base path `/api/v1/…`; websocket `/ws`.  
   * **index.html** loads **app.js** → opens WebSocket → polls `/api/status` every 5 s as a fallback.  
   * All JS fetches go through **api.js** (easy future migration to MQTT).

4. **OTA page** (mounted at `/update`) via **OTABridge** (AsyncElegantOTA). citeturn0search2turn0search6

> Because `WifiMgr` hides AP/STA state, both the Serial CLI and Signal Engine work even when the network changes.

---

## 3 · Granular Implementation & Test Road‑map (≈ 9 Weeks)

| **Phase**                      | **When** | **Key Tasks**                                                                           | **Bench / CI test**                                                          |
| ------------------------------ | -------- | --------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| **0 · Bootstrap**              | Day 1    | Initialise Git + PlatformIO; commit empty repo; create `signal_iface.h`                 | CI `pio check` passes                                                        |
| **1 · Minimal PWM**            | Week 1   | • `SignalEngine.cpp` with LEDC init<br>• GPIO 18 1 kHz / 50 %<br>• Serial log heartbeat | Scope sees 1 kHz ±0.1 %; UART prints "alive"                                 |
| **2 · Command Queue**          | Week 1‑2 | • Add `xQueueCmd` + `CmdDispatcher` task<br>• Implement `Start/Stop/Update`             | Python script injects structs → waveform changes instantly                   |
| **3 · Serial CLI**             | Week 2   | • `SerialCLI.cpp` non‑blocking parser<br>• Commands mirror queue structs                | Type `start 0 2000 0.25` in terminal → 2 kHz 25 % on scope                   |
| **4 · Captive Wi‑Fi**          | Week 3   | • `WifiMgr.*`: STA connect + portal fallback<br>• Save creds with `Preferences`         | Power‑cycle: auto‑reconnect, else portal appears on phone                    |
| **5 · Static Web Server**      | Week 3   | • AsyncWebServer "Hello" page<br>• LittleFS upload script (`pio run -t uploadfs`)       | Browser fetches `/` under both AP & STA modes                                |
| **6 · REST API**               | Week 4   | • `ApiRouter.*` maps `/api/trigger` → queue<br>• Return JSON `{status:"queued"}`        | Postman test collection passes (HTTP 200)                                    |
| **7 · WebSocket Telemetry**    | Week 4‑5 | • `WebSocketHub.*` subscribes to `esp_event`<br>• Front‑end updates duty slider live    | Open two browsers; changes reflect in both within <50 ms citeturn0search1 |
| **8 · OTA & Secure Boot Flag** | Week 6   | • `OTABridge` wraps AsyncElegantOTA<br>• Add MD5 verify; doc secure‑boot steps          | Upload new build via `/update`; board reboots into new firmware              |
| **9 · Integration Soak**       | Week 7   | • `env:prod` build<br>• Stress: 100 REST calls / s for 24 h                             | Task WDT silent; jitter <1 µs on scope                                       |
| **10 · Future‑Ready Hooks**    | Week 8   | • Create empty `modulators/` dir<br>• Define `SigCmdType::Sweep` placeholder            | Compile flag `ENABLE_SWEEP` toggles off by default                           |
| **11 · Docs & Release**        | Week 9   | • Generate Doxygen<br>• Write OpenAPI schema<br>• Tag `v1.0.0`                          | All CI pipelines green                                                       |

Every phase ends with: **(a)** a PlatformIO build in GitHub CI, **(b)** a repeatable bench script (Serial, Postman, or scope capture), **(c)** an update to `CHANGELOG.md`.

---

## 4 · Per‑File Details (new & refined)

| **File**               | **What it does**                                                                                                                                                        |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **WifiMgr.h / .cpp**   | encapsulates WiFiManager calls, AP/STA switching, credential save/load, hostname & mDNS. Returns enum `WifiState { STA, AP, NO_NET }`. citeturn0search8turn0search0 |
| **ApiRouter.cpp**      | Registers REST endpoints, validates JSON with ArduinoJson, fills `SignalCmd`, enqueue; sends HTTP 400 on malformed input.                                               |
| **WebSocketHub.cpp**   | Singleton; on `SignalEvt` converts to tiny JSON `{ch:0,freq:1000,duty:0.5}` and broadcasts.                                                                             |
| **app.js** (UI)        | Main SPA controller in vanilla JS: connects WebSocket, binds sliders/buttons, falls back to `/api/status` on WS loss.                                                   |
| **js/api.js**          | Thin wrapper around `fetch()` with timeout + retry; central place if you swap REST for MQTT later.                                                                      |
| **js/ws.js**           | Auto‑reconnect logic, back‑off strategy.                                                                                                                                |
| **EngineConfig.h**     | Compile‑time knobs: default GPIO map, queue length, web‑root path, OTA enable.                                                                                          |
| **preferences_keys.h** | Shared string constants for all NVS keys.                                                                                                                               |
| **modulators/**        | Empty today; each future PWM algorithm gets one `.cpp` implementing `void applyMod(Channel&, const SweepCfg&)`.                                                         |
| **test/** trees        | Unity/GoogleTest; mocks hardware via `#ifdef HOST_TEST`.                                                                                                                |

---

## 5 · Module‑Level Test Matrix

| Module           | Host tests (CI)                                          | On‑board tests                                       | Tools                        |
| ---------------- | -------------------------------------------------------- | ---------------------------------------------------- | ---------------------------- |
| SignalEngine     | Duty/freq math, queue state machine                      | Scope: period, duty, phase; Timer capture for jitter | Oscilloscope, logic analyser |
| SerialCLI        | Parser table, error codes                                | Fuzz: random chars @ 115 kBd, ensure no crash        | Serial console               |
| WifiMgr          | STA/AP state transitions via mocked `WiFi`               | Real router + phone portal                           | Phone, router                |
| WebFacade (REST) | `test_rest_parser.cpp` posts JSON to AsyncWebServer mock | Postman collection on live board                     | Postman                      |
| WebSocketHub     | Jest tests for JS reconnection logic                     | Two browsers, verify broadcast sync                  | Laptop/phone                 |
| OTABridge        | MD5 mismatch abort                                       | Upload big binary, ensure never flashes              | Browser                      |

Each row can run *while the other modules are swapped out for stubs*, proving decoupling.

---

## 6 · Long‑Term Expandability Notes

* **Modulators directory** already in build graph; enabling a new PWM mode is: create `SweepDriver.cpp`, extend `SigCmdType`, add JSON schema entry—**no touch to WebFacade internals**.  
* If you want **MQTT** later, drop a new `MqttBridge` library that subscribes to `esp_event` exactly like WebSocketHub—no changes in SignalEngine.  
* The **SerialCLI** parser table is data‑driven (`CliHelpTable.cpp`), so new commands are a struct entry, not a regex rewrite.  
* Use **CMakeLists.txt** inside each lib if one day you port to ESP‑IDF: the source folders already match ESP‑IDF component layout.  
* `common/build_opts.h` lets you build *headless lab firmware* (`#define BUILD_WEB 0`) shaving ~120 kB RAM when the UI isn't needed.

---

### Fresh Sources for Wi‑Fi & Web
* WiFiManager captive‑portal workflow citeturn0search8  
* ESP32 WiFi library modes (STA/AP) citeturn0search3  
* AsyncWebServer + WebSocket tutorial citeturn0search1  
* OTA Web Updater / AsyncElegantOTA citeturn0search2turn0search6  
* Random Nerd Async server control example citeturn0search9  

---

**With these details you can start coding tomorrow, add features next month, and still sleep at night knowing every layer has its own tests, well‑defined interfaces, and an upgrade path for whatever "complicated PWM" ideas come next.**



Phase 0: Bootstrap (Day 1)
Initialize a Git repository.
Set up a PlatformIO project (platformio.ini).
Create the basic directory structure (common/, lib/, apps/, docs/).
Define the initial command/event interface in common/signal_iface.h.
Define global build options in common/build_opts.h.
Commit the initial structure.
Set up basic CI (e.g., GitHub Actions) to run pio check.
Phase 1: Minimal PWM (Week 1)
Create the SignalEngine library structure (lib/SignalEngine/, including include/ and src/).
Implement basic LEDC initialization in SignalEngine.cpp and LedcDriver.cpp.
Configure a default PWM output (e.g., GPIO 18, 1 kHz, 50% duty).
Add a simple serial output heartbeat (Serial.println("alive");).
Create a minimal test application (apps/signal_cli/main.cpp) to initialize and run the engine.
Test: Verify PWM output with an oscilloscope and check for the serial heartbeat.
Phase 2: Command Queue & Dispatcher (Week 1-2)
Add FreeRTOS queue (xQueueCmd) to SignalEngine for receiving commands.
Implement a CmdDispatcher task within SignalEngine to process commands from the queue.
Implement handlers for Start, Stop, and Update commands defined in signal_iface.h, modifying the LEDC/RMT output accordingly.
Test: Create a test script (e.g., Python) or modify signal_cli/main.cpp to send command structs to the engine and verify waveform changes.
Phase 3: Serial CLI (Week 2)
Create the SerialCLI library structure (lib/SerialCLI/).
Implement a non-blocking serial command parser in SerialCLI.cpp.
Define CLI commands (start, stop, update) that map to the SignalCmd structs. Use CliHelpTable.cpp for command definitions.
Integrate SerialCLI into apps/signal_cli/main.cpp to parse input and send commands to SignalEngine.
Test: Use a serial terminal to send commands and verify the PWM output changes correctly via oscilloscope.
Phase 4: Captive Wi-Fi Provisioning (Week 3)
Add the WiFiManager library as a dependency.
Create the WifiMgr component within lib/WebFacade/ (include/WifiMgr.h, src/WifiMgr.cpp).
Implement logic in WifiMgr to:
Attempt connection using saved credentials (from NVS via Preferences).
Fall back to AP mode and start the WiFiManager captive portal if connection fails.
Save new credentials to NVS.
Define NVS keys in common/preferences_keys.h.
Create a test application or integrate into apps/web_mock/main.cpp.
Test: Power cycle the ESP32. Verify it connects automatically if credentials exist, otherwise, verify the captive portal appears on a phone/laptop and allows connection configuration.
Phase 5: Static Web Server (Week 3)
Add ESPAsyncWebServer and AsyncTCP libraries.
Create the WebFacade library structure (lib/WebFacade/).
Implement basic AsyncWebServer setup in WebFacade.cpp.
Serve a simple placeholder HTML page (data/index.html) using LittleFS.
Create the data/ directory structure (css/, js/) and basic files (app.js, style.css, api.js, ws.js).
Add a PlatformIO script (platformio.ini) to upload the LittleFS filesystem (pio run -t uploadfs).
Test: Access the ESP32's IP address from a browser (in both STA and AP modes) and verify the placeholder page loads.
Phase 6: REST API (Week 4)
Add the ArduinoJson library.
Create ApiRouter.h/.cpp within lib/WebFacade/.
Implement REST endpoint handlers (e.g., POST /api/trigger) in ApiRouter.cpp.
Use ArduinoJson to parse incoming JSON requests.
Validate the request and enqueue the corresponding SignalCmd to SignalEngine.
Return appropriate HTTP responses (e.g., 200 OK with {status:"queued"}, 400 Bad Request).
Integrate ApiRouter into WebFacade.
Test: Use Postman or curl to send valid and invalid JSON payloads to the REST endpoint and verify responses and PWM output changes.
Phase 7: WebSocket Telemetry (Week 4-5)
Create WebSocketHub.h/.cpp within lib/WebFacade/.
Implement an AsyncWebSocket server in WebSocketHub.cpp.
Define SignalEvt structs (in common/signal_iface.h) for events (e.g., PWM state changes).
Use esp_event or a similar mechanism for SignalEngine to post events that WebSocketHub can subscribe to.
When WebSocketHub receives an event, format it as JSON (e.g., {ch:0, freq:1000, duty:0.5}) and broadcast it to connected WebSocket clients.
Implement WebSocket connection logic in the front-end (app.js, js/ws.js) to receive updates and display them (e.g., update a slider).
Test: Open the web UI in two browser windows. Make changes via REST or Serial CLI and verify both UIs update in near real-time.
Phase 8: OTA Updates (Week 6)
Add the AsyncElegantOTA library.
Create the OTABridge library (lib/OTABridge/).
Wrap AsyncElegantOTA setup within OTAService.cpp, mounting it at /update.
Integrate OTABridge into WebFacade or the main application.
(Optional) Add MD5 verification if feasible.
Document the secure boot process if applicable.
Test: Build a new version of the firmware. Access the /update URL in a browser and upload the new .bin file. Verify the device reboots with the updated firmware.
Phase 9: Integration Soak Test (Week 7)
Create the final production application apps/production_fw/main.cpp, integrating SignalEngine, WebFacade, SerialCLI (optional), and OTABridge.
Define the [env:prod] environment in platformio.ini.
Perform extended stress tests:
Send a high volume of REST requests.
Maintain multiple WebSocket connections.
Monitor for crashes, Task WDT resets, and memory leaks.
Measure signal jitter under load.
Test: Ensure stability over a long period (e.g., 24h) under load, check logs for errors, and verify signal quality remains high.
Phase 10: Future-Ready Hooks (Week 8)
Create the empty lib/SignalEngine/src/modulators/ directory.
Add placeholder definitions (e.g., SigCmdType::Sweep) in common/signal_iface.h.
Add relevant compile-time flags (e.g., #define ENABLE_SWEEP 0) to common/build_opts.h.
Test: Ensure the project compiles cleanly with the new placeholders and flags.
Phase 11: Documentation & Release (Week 9)
Configure Doxygen (docs/Doxyfile).
Generate API documentation.
Write an OpenAPI (Swagger) specification for the REST API (docs/openapi.yaml or similar).
Finalize README.md and CHANGELOG.md.
Ensure all CI checks pass.
Tag the release in Git (e.g., v1.0.0).
This step-by-step plan, derived directly from your project_plan.md, should guide the implementation process effectively. Remember to implement the corresponding tests at each phase as outlined in the plan.