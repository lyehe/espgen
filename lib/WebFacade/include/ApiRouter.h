#ifndef API_ROUTER_H
#define API_ROUTER_H

#include <ArduinoJson.h>        // For JSON parsing
#include <ESPAsyncWebServer.h>  // For web server types
#include "signal_iface.h"       // For SignalCmd struct
#include "SignalEngine.h"       // For SignalEngine reference

class ApiRouter {
public:
    // Constructor takes SignalEngine and the server instance
    ApiRouter(SignalEngine& engine, AsyncWebServer& server);

    // Method to register all API routes with the server
    void registerRoutes();

private:
    SignalEngine& _engine;      // Reference to the signal engine
    AsyncWebServer& _server;    // Reference to the web server

    // --- Request Handlers ---
    // Handler for POST requests to /api/trigger
    void handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json);
    
    // Handler for GET requests to /api/status
    void handleStatusGet(AsyncWebServerRequest *request);

    // Handler for GET requests to /api/discovery
    void handleDiscoveryGet(AsyncWebServerRequest *request);

    // Handler for POST requests to /api/v1/config/output_pin
    void handleSetOutputPinPost(AsyncWebServerRequest *request, JsonVariant &json);

    // Static handler wrapper needed for AsyncWebServer library with JSON body
    static void handleTriggerPostWrapper(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
};

#endif // API_ROUTER_H 