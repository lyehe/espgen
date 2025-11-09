#ifndef API_ROUTER_H
#define API_ROUTER_H

#include <ArduinoJson.h>        // For JSON parsing
#include <ESPAsyncWebServer.h>  // For web server types
#include "signal_iface.h"       // For SignalCmd struct
#include "ISignalController.h"  // Application layer interface (Clean Architecture)

/**
 * @brief API Router (Presentation Layer)
 *
 * Handles HTTP requests and routes them to application services.
 * Follows Clean Architecture by depending on interfaces, not concrete implementations.
 *
 * Responsibilities:
 * - Parse HTTP requests
 * - Validate input (presentation-level validation)
 * - Call application services
 * - Format HTTP responses
 */
class ApiRouter {
public:
    // Constructor takes application service interface (Dependency Inversion)
    ApiRouter(ISignalController& controller, AsyncWebServer& server);

    // Method to register all API routes with the server
    void registerRoutes();

private:
    ISignalController& _controller; // Application service interface (not concrete class!)
    AsyncWebServer& _server;        // Reference to the web server

    // --- Request Handlers ---
    // Handler for POST requests to /api/trigger
    void handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json);
    
    // Handler for GET requests to /api/status
    void handleStatusGet(AsyncWebServerRequest *request);

    // Handler for GET requests to /api/discovery
    void handleDiscoveryGet(AsyncWebServerRequest *request);

    // Handler for POST requests to /api/v1/config/output_pin
    void handleSetOutputPinPost(AsyncWebServerRequest *request, JsonVariant &json);

    // Handler for POST requests to /api/setindicator
    void handleSetIndicatorPost(AsyncWebServerRequest *request, JsonVariant &json);

    // --- New Multi-Channel and Advanced Feature Handlers ---
    // Handler for POST requests to /api/channel (configure slave channels)
    void handleChannelPost(AsyncWebServerRequest *request, JsonVariant &json);

    // Handler for GET requests to /api/channels (get all channel status)
    void handleChannelsGet(AsyncWebServerRequest *request);

    // Handler for POST requests to /api/sync (trigger manual sync)
    void handleSyncPost(AsyncWebServerRequest *request);

    // Static handler wrapper needed for AsyncWebServer library with JSON body
    static void handleTriggerPostWrapper(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
};

#endif // API_ROUTER_H 