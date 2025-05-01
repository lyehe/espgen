# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- Initial project structure.
- **Phase 1: Minimal PWM:** Basic SignalEngine with LEDC output and serial heartbeat.
- **Phase 2: Command Queue:** Added FreeRTOS queue and dispatcher task to SignalEngine for Start/Stop/Update commands.
- **Phase 3: Serial CLI:** Implemented non-blocking SerialCLI library to parse commands and control SignalEngine.
- **Phase 4: Captive Wi-Fi:** Added WifiMgr class using WiFiManager library for connection/provisioning via captive portal; integrated into WebFacade. 
- **Phase 5: Static Web Server:** Added basic AsyncWebServer in WebFacade, serving static files from LittleFS via `data/` directory. Included `pio run -t uploadfs` script.
- **Phase 6: REST API:** Implemented `ApiRouter` within WebFacade to handle POST requests to `/api/trigger`, parse JSON, and queue commands to SignalEngine.
- **Phase 7: WebSocket Telemetry:** Implemented `WebSocketHub` to broadcast `SignalEngine` events (status updates) to connected web clients via WebSocket at `/ws`. Updated frontend JS (`ws.js`, `app.js`) to handle WebSocket connection and messages.
- **Phase 8: OTA Updates:** Integrated `ElegantOTA` library via `OTABridge` component, accessible at `/update`, allowing wireless firmware updates for the `prod` environment. Updated static file serving in `WebFacade` to avoid conflicts.
- **Phase 9: Integration Soak Test:** Performed stress testing on the `prod` environment using multiple WebSocket clients and repeated API calls. Monitored serial output and web UI responsiveness for stability. Ensured no crashes or watchdog resets under moderate load. 
- **Feature: Configurable Output Pin:** Added functionality to set the PWM output pin (GPIO 12-19).
    - Added `SIG_CMD_SET_PIN` command and `pin` field to `SignalCmd` (`common/signal_iface.h`).
    - Added NVS key `OUTPUT_PIN_KEY` in `DEVICE_CFG_NAMESPACE` (`common/preferences_keys.h`).
    - `SignalEngine` now loads the pin from NVS on startup (default `DEFAULT_OUTPUT_PIN` in `EngineConfig.h`).
    - `SignalEngine` handles `SIG_CMD_SET_PIN` to update NVS and re-attach `LedcDriver` to the new pin.
    - Added `setpin <gpio>` command to `SerialCLI`.
    - Added `POST /api/v1/config/output_pin` endpoint to `ApiRouter` (expects JSON `{"pin": <num>}`).

### Changed
- `LedcDriver` now has a `reAttachPin(int newPin)` method.
- `SignalEngine` initializes `LedcDriver` using the pin loaded from NVS.

### Fixed
- N/A 