#include "ApiRouter.h"
#include "signal_iface.h" // Ensure SigCmdType is visible
#include <vector> // Needed for temporary buffer

// Define the expected JSON buffer size
const size_t JSON_DOC_SIZE = 256; // Keep for buffer size check reference

// Structure to hold temporary body data during async reception
struct RequestBodyState {
    std::vector<uint8_t> buffer;
};

// Constructor
ApiRouter::ApiRouter(SignalEngine& engine, AsyncWebServer& server) :
    _engine(engine), _server(server) {}

// Method to register API routes
void ApiRouter::registerRoutes() {
    // Manual handling for POST /api/trigger
    _server.on("/api/trigger", HTTP_POST, [this](AsyncWebServerRequest *request){
        // This is the main handler when the request headers are received
        // Initialize state object to store body chunks
        if (!request->_tempObject) {
             request->_tempObject = new RequestBodyState();
        }
    },
    NULL, // No file upload handler
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // This is the 'onBody' handler
        RequestBodyState* state = reinterpret_cast<RequestBodyState*>(request->_tempObject);

        if (!state && index == 0) {
            // Should have been allocated in the main handler, but double-check
            state = new RequestBodyState();
            request->_tempObject = state;
        }

        if (state) {
            // Resize buffer on first chunk
            if (index == 0) {
                 if (total > JSON_DOC_SIZE * 4) { // Increased buffer check slightly
                     Serial.println("ERROR: JSON body too large");
                     request->send(413, "text/plain", "Payload Too Large");
                     // Clean up state
                     delete state;
                     request->_tempObject = nullptr;
                     // Need to ensure request processing stops; closing might be too abrupt
                     // request->client()->close();
                     return; // Stop processing body chunks
                 }
                 state->buffer.reserve(total + 1); // Reserve space including null terminator
            }
            // Append data
            state->buffer.insert(state->buffer.end(), data, data + len);
        }

        if (index + len == total) { // Last chunk received
             Serial.printf("Received POST /api/trigger, Body Size: %d\n", total);
            if (state) {
                 state->buffer.push_back(0); // Null-terminate for parsing

                // Use modern JsonDocument
                JsonDocument jsonDoc;
                DeserializationError error = deserializeJson(jsonDoc, state->buffer.data());

                if (error) {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
                } else {
                    JsonVariant jsonVariant = jsonDoc.as<JsonVariant>();
                    this->handleTriggerPost(request, jsonVariant); // Call the actual logic handler
                }

                // Clean up state object
                delete state;
                request->_tempObject = nullptr;
            } else {
                 request->send(500, "text/plain", "Internal Server Error: State Error");
            }
        }
    });

     _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
         // Get actual status from SignalEngine
         bool running = _engine.isRunning();
         double freq = _engine.getCurrentFrequencyHz();
         float duty = _engine.getCurrentDutyCycle();

         // Build JSON response
         JsonDocument doc; // Using modern JsonDocument
         doc["status"] = running ? "running" : "stopped";
         doc["frequency"] = freq;
         doc["duty_cycle"] = duty;

         String output;
         serializeJson(doc, output);
         request->send(200, "application/json", output);
         Serial.println("Sent GET /api/status response (actual data)");
     });
}

// Handler implementation for POST /api/trigger
void ApiRouter::handleTriggerPost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    // Use modern check for key presence and type
    if (!obj || !obj["command"].is<const char*>()) {
        request->send(400, "application/json", "{\"error\":\"Missing or invalid 'command' field\"}");
        return;
    }

    const char* commandStr = obj["command"];
    // uint8_t channel = obj["channel"] | 0; // Channel currently not in SignalCmd
    uint32_t freq = obj["frequency"] | 1000;
    float duty = obj["duty_cycle"] | 0.5f;

    SignalCmd cmd;
    bool commandValid = true;

    // Use correct enum type and values
    if (strcmp(commandStr, "start") == 0) {
        cmd.type = SIG_CMD_START;
        // Set params for start if needed, based on how SignalEngine interprets START
        // Assuming START uses the last known/default params, or we might need separate update
        cmd.frequencyHz = freq; // Let's assume START can set initial params
        cmd.dutyCycle = duty;
        Serial.printf("API: Parsed START (Freq: %lu, Duty: %.2f)\n", freq, duty);
    } else if (strcmp(commandStr, "stop") == 0) {
        cmd.type = SIG_CMD_STOP;
         Serial.printf("API: Parsed STOP\n");
    } else if (strcmp(commandStr, "update") == 0) {
        // Assuming "update" corresponds to UPDATE_ALL
        cmd.type = SIG_CMD_UPDATE_ALL;
        cmd.frequencyHz = freq; // Access members directly
        cmd.dutyCycle = duty;   // Access members directly
         Serial.printf("API: Parsed UPDATE (Freq: %lu, Duty: %.2f)\n", freq, duty);
    } else {
        commandValid = false;
         Serial.printf("API: Invalid command '%s'\n", commandStr);
        request->send(400, "application/json", "{\"error\":\"Invalid command value\"}");
    }

    if (commandValid) {
        // Use correct method name
        if (_engine.sendCommand(cmd)) {
            request->send(200, "application/json", "{\"status\":\"queued\"}");
            Serial.println("API: Command sent successfully.");
        } else {
            request->send(503, "application/json", "{\"error\":\"Command queue full\"}");
            Serial.println("API: Command queue full.");
        }
    }
} 