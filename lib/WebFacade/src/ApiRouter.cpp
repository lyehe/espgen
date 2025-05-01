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
         // Call the dedicated handler method
         this->handleStatusGet(request);
     });

    // Register GET handler for /api/discovery
    _server.on("/api/discovery", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleDiscoveryGet(request);
    });

    // Register POST handler for /api/v1/config/output_pin
    // Using v1 namespace for config endpoint
    _server.on("/api/v1/config/output_pin", HTTP_POST, 
        [](AsyncWebServerRequest *request){ /* No file upload */ }, 
        NULL, 
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Re-use or adapt the RequestBodyState logic if needed for larger bodies,
            // but for a simple {"pin": 18}, direct handling might be okay if limits are enforced.
            
            // Basic implementation assuming single chunk or small body
            if (index == 0) { // First chunk (or only chunk)
                if (total > 64) { // Check for excessively large body
                    Serial.println("ERROR: /api/v1/config/output_pin body too large");
                    request->send(413, "text/plain", "Payload Too Large");
                    return;
                }
                
                // Process the data directly
                JsonDocument jsonDoc;
                DeserializationError error = deserializeJson(jsonDoc, data, len);

                if (error) {
                    Serial.print("deserializeJson() failed for setpin: ");
                    Serial.println(error.c_str());
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
                } else {
                    JsonVariant jsonVariant = jsonDoc.as<JsonVariant>();
                    this->handleSetOutputPinPost(request, jsonVariant); // Call specific handler
                }
            } else {
                // Handle potential multi-chunk bodies if necessary (complex)
                Serial.println("Warning: Multi-chunk body not fully supported for setpin yet.");
                // Ideally, accumulate using RequestBodyState like /api/trigger
            }
    });
}

// Handler implementation for GET /api/discovery
void ApiRouter::handleDiscoveryGet(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["status"] = "ok";
    doc["device_type"] = "ESP32_Signal_Generator"; // Identify the device type
    doc["hostname"] = "trigger"; // Reflecting the mDNS name

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
    Serial.println("Sent GET /api/discovery response");
}

// Handler implementation for GET /api/status
void ApiRouter::handleStatusGet(AsyncWebServerRequest *request) {
    SignalStatus_t currentStatus;
    SignalError err = _engine.getCurrentStatus(currentStatus);

    if (err != SIG_OK) {
        // Handle potential errors from getCurrentStatus if any are added later
        Serial.printf("Error getting signal status: %d\n", err);
        request->send(500, "application/json", "{\"error\":\"Failed to get engine status\"}");
        return;
    }

    // Get the current output pin
    int outputPin = _engine.getOutputPin();

    // Build JSON response using the status struct
    JsonDocument doc; // Using modern JsonDocument
    doc["channel"] = currentStatus.channel; 
    doc["frequency"] = currentStatus.frequency;
    doc["duty_cycle"] = currentStatus.dutyCycle;
    doc["is_running"] = currentStatus.isRunning; // Use a different key to avoid confusion with generic "status"
    doc["duration_sec"] = currentStatus.lastAppliedDurationSec; // Add the duration
    doc["output_pin"] = outputPin; // Add the current output pin
    doc["status_text"] = currentStatus.isRunning ? "running" : "stopped"; // Optional descriptive text

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
    Serial.println("Sent GET /api/status response (using status struct)");
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
    float duration = obj["duration_sec"] | 0.0f; // Parse duration_sec, default 0

    SignalCmd cmd;
    bool commandValid = true;
    cmd.durationSec = 0; // Ensure duration is 0 for non-start commands by default

    // Use correct enum type and values
    if (strcmp(commandStr, "start") == 0) {
        cmd.type = SIG_CMD_START;
        cmd.frequencyHz = freq;
        cmd.dutyCycle = duty;
        cmd.durationSec = duration; // Set duration for start command
        Serial.printf("API: Parsed START (Freq: %lu, Duty: %.2f, Duration: %.2f s)\n", freq, duty, duration);
    } else if (strcmp(commandStr, "stop") == 0) {
        cmd.type = SIG_CMD_STOP;
         Serial.printf("API: Parsed STOP\n");
    } else if (strcmp(commandStr, "update") == 0) {
        // Assuming "update" corresponds to UPDATE_ALL
        cmd.type = SIG_CMD_UPDATE_ALL;
        cmd.frequencyHz = freq;
        cmd.dutyCycle = duty;
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

// Handler implementation for POST /api/v1/config/output_pin
void ApiRouter::handleSetOutputPinPost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();

    if (!obj || !obj["pin"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"Missing or invalid 'pin' field (must be integer)\"}");
        return;
    }

    int pin = obj["pin"];
    Serial.printf("API: Received request to set output pin to %d\n", pin);

    // Validate the pin number
    if (pin >= 12 && pin <= 19) {
        SignalCmd cmd;
        cmd.type = SIG_CMD_SET_PIN;
        cmd.pin = (uint8_t)pin;

        if (_engine.sendCommand(cmd)) {
            request->send(200, "application/json", "{\"status\":\"output pin update queued\"}");
            Serial.println("API: Set Output Pin command sent successfully.");
        } else {
            request->send(503, "application/json", "{\"error\":\"Command queue full\"}");
            Serial.println("API: Command queue full for Set Output Pin.");
        }
    } else {
        Serial.printf("API: Invalid pin %d requested. Must be 12-19.\n", pin);
        request->send(400, "application/json", "{\"error\":\"Invalid pin number (must be 12-19)\"}");
    }
} 