#ifndef WEBSOCKET_HUB_H
#define WEBSOCKET_HUB_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> // For creating JSON messages
#include "signal_iface.h" // For SignalEvt

// Forward declaration
class WebFacade;

class WebSocketHub {
public:
    WebSocketHub(AsyncWebServer& server);
    bool begin(); // Initialize WebSocket endpoint and event handling - returns false on failure
    void broadcastStatus(int32_t event_id, const SignalEvtData& eventData); // Use event_id for type

private:
    AsyncWebServer& _server; // Reference to the main web server
    AsyncWebSocket _ws;      // WebSocket server instance attached to path "/ws"

    // WebSocket Event Handlers
    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    // ESP-IDF Event Loop Handler (if we use this mechanism)
    static void espEventHandler(void* handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
};

#endif // WEBSOCKET_HUB_H 