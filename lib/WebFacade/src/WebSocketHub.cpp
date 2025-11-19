#include "WebSocketHub.h"
#include <Arduino.h> // For Serial
#include "esp_event.h" // Include ESP event library

// Define the event base declared in signal_iface.h
// ESP_EVENT_DEFINE_BASE(SIGNAL_EVENTS); // REMOVED - Definition should only be in one .cpp file (e.g., SignalEngine.cpp)

// Define the JSON buffer size for outgoing messages
const size_t WS_JSON_DOC_SIZE = 128; // Smaller buffer for status updates

// Constructor: Initialize member variables
WebSocketHub::WebSocketHub(AsyncWebServer& server) :
    _server(server),
    _ws("/ws") // Attach WebSocket server to the "/ws" path
{
}

// Initialize WebSocket endpoint and event handling
bool WebSocketHub::begin() {
    // Register the WebSocket event handler
    _ws.onEvent(onWsEvent);

    // Add the WebSocket handler to the main web server
    _server.addHandler(&_ws);

    Serial.println("WebSocketHub: Initialized WebSocket endpoint at /ws");

    // Register the ESP event handler to listen for SignalEngine events
    esp_err_t reg_err = esp_event_handler_register(SIGNAL_EVENTS, ESP_EVENT_ANY_ID, &espEventHandler, this);
    if (reg_err != ESP_OK) {
        Serial.printf("WebSocketHub: CRITICAL - Event handler registration failed: %s\n",
                     esp_err_to_name(reg_err));

        if (reg_err == ESP_ERR_INVALID_STATE) {
            Serial.println("  Likely cause: Event loop not created!");
            Serial.println("  Call esp_event_loop_create_default() in setup()");
        } else if (reg_err == ESP_ERR_NO_MEM) {
            Serial.println("  Likely cause: Out of memory!");
        }

        Serial.println("WebSocketHub: Initialization FAILED - WebSocket updates will not work");
        return false;
    }

    Serial.println("WebSocketHub: Event handler registered successfully");
    return true;
}

// Static handler for WebSocket events
void WebSocketHub::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocketHub: Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // Optional: Send initial status upon connection?
            // Example: server->text(client->id(), "{\"message\":\"Welcome!\"}");
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocketHub: Client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // We don't expect data from clients in this phase, but log if received
            Serial.printf("WebSocketHub: Received data from client #%u\n", client->id());
            // Example: Process incoming data if needed later
            // AwsFrameInfo *info = (AwsFrameInfo*)arg;
            // if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            //    data[len] = 0; // Null-terminate
            //    Serial.printf("  Data: %s\n", (char*)data);
            //}
            break;
        case WS_EVT_PONG:
            Serial.printf("WebSocketHub: Received pong from client #%u\n", client->id());
            break;
        case WS_EVT_ERROR:
            Serial.printf("WebSocketHub: Error on client #%u. Code: %u, Message: %s\n", client->id(), *((uint16_t*)arg), (char*)data);
            break;
    }
}

// Static handler for ESP events from SignalEngine
void WebSocketHub::espEventHandler(void* handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    // Validate event base
    if (event_base != SIGNAL_EVENTS) {
        return; // Not our event
    }

    // Validate handler argument (instance pointer)
    if (!handler_arg) {
        Serial.println("ERROR: NULL handler_arg in WebSocketHub event handler!");
        return;
    }

    // Validate event data
    if (!event_data) {
        Serial.println("ERROR: NULL event_data in WebSocketHub event handler!");
        return;
    }

    // Cast and call handler (null checks already done above)
    WebSocketHub* hub = static_cast<WebSocketHub*>(handler_arg);
    SignalEvtData* data = static_cast<SignalEvtData*>(event_data);

    Serial.printf("WebSocketHub: Received event ID %ld from SIGNAL_EVENTS\n", event_id);

    // Call the broadcast method on the instance, passing event data directly
    // Note: C++ exceptions may be disabled in ESP32 builds, so we rely on null checks above
    hub->broadcastStatus(event_id, *data);
}

// Send event data (formatted as JSON) to all connected clients
void WebSocketHub::broadcastStatus(int32_t event_id, const SignalEvtData& eventData) {
    JsonDocument doc; // Use modern JsonDocument
    // Populate JSON based on event type (passed via event_id in handler, not data struct)
    // bool isRunning = (eventData.current_duty > 0.0f); // Infer running state - REMOVED incorrect logic

    // Determine message type based on event ID
    const char* msgType;
    switch(event_id) {
        case SIG_EVT_STARTED:
            msgType = "update"; // Treat started same as update for UI state
            break;
        case SIG_EVT_STOPPED:
            msgType = "stopped";
            break;
        case SIG_EVT_PARAMS_CHANGED:
            msgType = "update";
            break;
        default:
            msgType = "unknown"; // Should not happen
            break;
    }

    doc["type"] = msgType;
    doc["frequency"] = eventData.current_freq;
    doc["duty_cycle"] = eventData.current_duty;
    doc["duration_sec"] = eventData.duration_sec;
    char ticksStr[21]; // Max length for uint64_t string + null terminator
    snprintf(ticksStr, sizeof(ticksStr), "%llu", eventData.current_ticks);
    doc["ticks"] = ticksStr;

    String output;
    serializeJson(doc, output);
    _ws.textAll(output);
    Serial.printf("WebSocketHub: Broadcasted status - %s\n", output.c_str());
} 