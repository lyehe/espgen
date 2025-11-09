#ifndef WEBFACADE_H
#define WEBFACADE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "WifiMgr.h"                    // WiFi Manager
#include "ISignalController.h"          // Application layer interface (Clean Architecture)
#include "SignalControllerAdapter.h"    // Adapter for existing SignalEngine
#include "SignalEngine.h"               // Domain layer (for adapter)
#include "ApiRouter.h"                  // API Router
#include "WebSocketHub.h"               // WebSocket Hub
#include "OTAService.h"                 // OTA Service

/**
 * @brief Web Facade (Composition Root)
 *
 * Wires together all components following Clean Architecture principles.
 * This is where dependency injection happens.
 *
 * Architecture:
 * WebFacade -> SignalControllerAdapter -> SignalEngine
 *           -> ApiRouter -> ISignalController (interface)
 *           -> WebSocketHub
 *           -> OTAService
 */
class WebFacade {
public:
    // Constructor requires SignalEngine (domain layer)
    // Creates adapter and injects it into presentation layer
    WebFacade(SignalEngine& engine);
    bool begin(); // Initialize web facade - returns false on critical failure

private:
    SignalEngine& _engine;              // Domain layer reference
    SignalControllerAdapter _adapter;   // Application layer adapter (bridges domain & presentation)
    AsyncWebServer _server;             // Web server instance
    WifiMgr _wifiMgr;                   // WiFi Manager instance
    ApiRouter _apiRouter;               // API Router (depends on ISignalController)
    WebSocketHub _wsHub;                // WebSocket Hub instance
    OTAService _otaService;             // OTA Service instance

    // --- Request Handlers ---
    void handleRoot(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);

    // --- LittleFS Initialization ---
    bool initLittleFS();
};

#endif // WEBFACADE_H 