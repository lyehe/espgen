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