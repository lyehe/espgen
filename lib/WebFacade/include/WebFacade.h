#ifndef WEBFACADE_H
#define WEBFACADE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "WifiMgr.h"      // Include the WiFi Manager we created in Phase 4
#include "SignalEngine.h" // Include SignalEngine (needed for ApiRouter)
#include "ApiRouter.h"    // Include the ApiRouter we just created
#include "WebSocketHub.h" // Include WebSocketHub
#include "OTAService.h"   // Include OTAService for OTA updates

class WebFacade {
public:
    // Constructor now requires a SignalEngine reference
    WebFacade(SignalEngine& engine);
    void begin(); 

private:
    SignalEngine& _engine;      // Reference to the signal engine instance
    AsyncWebServer _server;     // Web server instance
    WifiMgr _wifiMgr;           // WiFi Manager instance
    ApiRouter _apiRouter;       // API Router instance
    WebSocketHub _wsHub;         // Add WebSocketHub instance
    OTAService _otaService;     // OTA Service instance

    // --- Request Handlers ---
    void handleRoot(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);

    // --- LittleFS Initialization ---
    bool initLittleFS();
};

#endif // WEBFACADE_H 