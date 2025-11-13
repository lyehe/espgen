#ifndef WEBFACADE_H
#define WEBFACADE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "WifiMgr.h"                    // WiFi Manager
#include "ISignalController.h"          // Application layer interface (Clean Architecture)
#include "ApiRouter.h"                  // API Router
#include "WebSocketHub.h"               // WebSocket Hub
#include "OTAService.h"                 // OTA Service

/**
 * @brief Web Facade (Presentation Layer)
 *
 * Wires together web-related components following Clean Architecture.
 * Depends ONLY on application layer interfaces, not domain layer.
 *
 * Architecture:
 * WebFacade -> ApiRouter -> ISignalController (interface)
 *           -> WebSocketHub
 *           -> OTAService
 *
 * Note: Composition root (adapter creation) happens in main.cpp
 */
class WebFacade {
public:
    // Constructor requires application layer interface (Clean Architecture)
    WebFacade(ISignalController& controller);
    bool begin(); // Initialize web facade - returns false on critical failure

private:
    ISignalController& _controller;     // Application layer interface (Clean Architecture)
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