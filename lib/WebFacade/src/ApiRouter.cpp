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

    // Add explicit OPTIONS handler for /api/trigger to handle CORS preflight
    _server.on("/api/trigger", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
        Serial.println("Received OPTIONS /api/trigger");
        AsyncWebServerResponse *response = request->beginResponse(204); // No Content
        // Add essential CORS headers matching the generic handler
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS"); // Specify allowed methods for this endpoint
        response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"); // Specify allowed headers
        request->send(response);
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

    // POST /api/setindicator - Set the status indicator pin
    _server.on("/api/setindicator", HTTP_POST, [this](AsyncWebServerRequest *request){
        // This is called when headers are received
        if (!request->_tempObject) {
            request->_tempObject = new RequestBodyState();
        }
    },
    NULL, // No file upload
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Body handler
        RequestBodyState* state = static_cast<RequestBodyState*>(request->_tempObject);
        if (state) {
            if (index == 0) { // First chunk
                state->buffer.clear();
                state->buffer.reserve(total + 1);
            }
            state->buffer.insert(state->buffer.end(), data, data + len);
        }

        if (index + len == total) { // Last chunk
            Serial.printf("Received POST /api/setindicator, Body Size: %d\n", total);
            if (state) {
                state->buffer.push_back(0); // Null-terminate

                JsonDocument jsonDoc;
                DeserializationError error = deserializeJson(jsonDoc, state->buffer.data());

                if (error) {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
                } else {
                    JsonVariant jsonVariant = jsonDoc.as<JsonVariant>();
                    this->handleSetIndicatorPost(request, jsonVariant);
                }

                delete state;
                request->_tempObject = nullptr;
            }
        }
    });

    // POST /api/channel - Configure slave channel
    _server.on("/api/channel", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (!request->_tempObject) {
            request->_tempObject = new RequestBodyState();
        }
    },
    NULL, // No file upload
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        RequestBodyState* state = static_cast<RequestBodyState*>(request->_tempObject);
        if (state) {
            if (index == 0) {
                state->buffer.clear();
                state->buffer.reserve(total + 1);
            }
            state->buffer.insert(state->buffer.end(), data, data + len);
        }

        if (index + len == total) {
            Serial.printf("Received POST /api/channel, Body Size: %d\n", total);
            if (state) {
                state->buffer.push_back(0);

                JsonDocument jsonDoc;
                DeserializationError error = deserializeJson(jsonDoc, state->buffer.data());

                if (error) {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
                } else {
                    JsonVariant jsonVariant = jsonDoc.as<JsonVariant>();
                    this->handleChannelPost(request, jsonVariant);
                }

                delete state;
                request->_tempObject = nullptr;
            }
        }
    });

    // GET /api/channels - Get all channel configurations
    _server.on("/api/channels", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleChannelsGet(request);
    });

    // POST /api/sync - Trigger manual sync
    _server.on("/api/sync", HTTP_POST, [this](AsyncWebServerRequest *request){
        this->handleSyncPost(request);
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
    SignalCmd cmd = {0}; // Zero-initialize entire structure
    bool commandValid = true;

    // Parse channel (default to 0)
    cmd.channel = obj["channel"] | 0;

    // Parse polarity (default to active high)
    if (obj.containsKey("polarity")) {
        const char* polarityStr = obj["polarity"];
        if (strcmp(polarityStr, "active_low") == 0) {
            cmd.polarity = POLARITY_ACTIVE_LOW;
        } else {
            cmd.polarity = POLARITY_ACTIVE_HIGH;
        }
    } else {
        cmd.polarity = POLARITY_ACTIVE_HIGH;
    }

    // Initialize paramMode
    cmd.paramMode = 0;

    // Use correct enum type and values
    if (strcmp(commandStr, "start") == 0) {
        cmd.type = SIG_CMD_START;

        // Frequency OR Period
        if (obj.containsKey("period_us")) {
            cmd.periodUs = obj["period_us"];
            cmd.paramMode |= PARAM_USE_PERIOD;
            Serial.printf("API: Using period: %lu us\n", cmd.periodUs);
        } else {
            cmd.frequencyHz = obj["frequency"] | 1000;
            cmd.paramMode |= PARAM_USE_FREQUENCY;
            Serial.printf("API: Using frequency: %.2f Hz\n", cmd.frequencyHz);
        }

        // Duty Cycle OR Pulse Width
        if (obj.containsKey("pulse_width_us")) {
            cmd.pulseWidthUs = obj["pulse_width_us"];
            cmd.paramMode |= PARAM_USE_PULSE_WIDTH;
            Serial.printf("API: Using pulse width: %lu us\n", cmd.pulseWidthUs);
        } else {
            cmd.dutyCycle = obj["duty_cycle"] | 0.5f;
            cmd.paramMode |= PARAM_USE_DUTY_CYCLE;
            Serial.printf("API: Using duty cycle: %.2f\n", cmd.dutyCycle);
        }

        // Duration OR Pulse Count
        if (obj.containsKey("pulse_count")) {
            cmd.pulseCount = obj["pulse_count"];
            cmd.paramMode |= PARAM_USE_PULSE_COUNT;
            Serial.printf("API: Using pulse count: %llu\n", cmd.pulseCount);
        } else {
            cmd.durationSec = obj["duration_sec"] | 0.0f;
            cmd.paramMode |= PARAM_USE_DURATION;
            Serial.printf("API: Using duration: %.2f s\n", cmd.durationSec);
        }

        Serial.printf("API: Parsed START (Channel: %d, Polarity: %s)\n",
                     cmd.channel,
                     cmd.polarity == POLARITY_ACTIVE_HIGH ? "HIGH" : "LOW");
    } else if (strcmp(commandStr, "stop") == 0) {
        cmd.type = SIG_CMD_STOP;
        cmd.paramMode = 0; // STOP doesn't need parameters
        Serial.printf("API: Parsed STOP\n");
    } else if (strcmp(commandStr, "update") == 0) {
        // Assuming "update" corresponds to UPDATE_ALL
        cmd.type = SIG_CMD_UPDATE_ALL;

        // Frequency OR Period
        if (obj.containsKey("period_us")) {
            cmd.periodUs = obj["period_us"];
            cmd.paramMode |= PARAM_USE_PERIOD;
        } else {
            cmd.frequencyHz = obj["frequency"] | 1000;
            cmd.paramMode |= PARAM_USE_FREQUENCY;
        }

        // Duty Cycle OR Pulse Width
        if (obj.containsKey("pulse_width_us")) {
            cmd.pulseWidthUs = obj["pulse_width_us"];
            cmd.paramMode |= PARAM_USE_PULSE_WIDTH;
        } else {
            cmd.dutyCycle = obj["duty_cycle"] | 0.5f;
            cmd.paramMode |= PARAM_USE_DUTY_CYCLE;
        }

        Serial.printf("API: Parsed UPDATE\n");
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

void ApiRouter::handleSetIndicatorPost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();

    if (!obj || !obj["pin"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"Missing or invalid 'pin' field (must be integer)\"}");
        return;
    }

    int pin = obj["pin"];
    Serial.printf("API: Received request to set indicator pin to %d\n", pin);

    // Indicator pin can be 0 (disabled) or any valid GPIO
    // Note: We don't restrict to 12-19 since indicator is just a simple GPIO output
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_SET_INDICATOR;
    cmd.pin = (uint8_t)pin;
    cmd.paramMode = 0; // No special parameters needed

    if (_engine.sendCommand(cmd)) {
        if (pin == 0) {
            request->send(200, "application/json", "{\"status\":\"indicator disabled\"}");
            Serial.println("API: Indicator disabled.");
        } else {
            request->send(200, "application/json", "{\"status\":\"indicator pin update queued\"}");
            Serial.printf("API: Set indicator pin to %d command sent successfully.\n", pin);
        }
    } else {
        request->send(503, "application/json", "{\"error\":\"Command queue full\"}");
        Serial.println("API: Command queue full for set indicator.");
    }
}

// Handler implementation for POST /api/channel
void ApiRouter::handleChannelPost(AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();

    // Validate required fields
    if (!obj || !obj["channel"].is<int>()) {
        request->send(400, "application/json", "{\"error\":\"Missing or invalid 'channel' field (must be integer)\"}");
        return;
    }

    int channel = obj["channel"];

    // Channel must be 1-5 (slave channels only)
    if (channel < 1 || channel > 5) {
        request->send(400, "application/json", "{\"error\":\"Channel must be 1-5 (slave channels only)\"}");
        return;
    }

    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_CONFIG_CHANNEL;
    cmd.channel = (uint8_t)channel;

    // Parse pin (optional)
    if (obj.containsKey("pin")) {
        cmd.pin = obj["pin"];
    }

    // Parse phase offset (optional) - can be degrees OR time delay
    if (obj.containsKey("phase_delay_us")) {
        cmd.phaseDelayUs = obj["phase_delay_us"];
        cmd.paramMode |= PARAM_USE_PHASE_TIME;
        Serial.printf("API: Channel %d phase delay: %lu us\n", channel, cmd.phaseDelayUs);
    } else if (obj.containsKey("phase_offset")) {
        cmd.phaseOffset = obj["phase_offset"];
        cmd.paramMode |= PARAM_USE_PHASE_DEGREES;
        Serial.printf("API: Channel %d phase offset: %.2f degrees\n", channel, cmd.phaseOffset);
    }

    // Parse enabled (optional)
    if (obj.containsKey("enabled")) {
        cmd.enabled = obj["enabled"];
    } else {
        cmd.enabled = true; // Default to enabled
    }

    Serial.printf("API: Configuring channel %d (pin: %d, enabled: %s)\n",
                 channel, cmd.pin, cmd.enabled ? "true" : "false");

    if (_engine.sendCommand(cmd)) {
        request->send(200, "application/json", "{\"status\":\"channel config queued\"}");
        Serial.println("API: Channel config command sent successfully.");
    } else {
        request->send(503, "application/json", "{\"error\":\"Command queue full\"}");
        Serial.println("API: Command queue full for channel config.");
    }
}

// Handler implementation for GET /api/channels
void ApiRouter::handleChannelsGet(AsyncWebServerRequest *request) {
    // Note: We need to add getters to SignalEngine to access PulseGenerator channel info
    // For now, we'll return a basic structure showing that the endpoint exists

    JsonDocument doc;
    JsonArray channels = doc["channels"].to<JsonArray>();

    // Channel 0 (master)
    JsonObject ch0 = channels.add<JsonObject>();
    ch0["id"] = 0;
    ch0["type"] = "master";
    ch0["pin"] = _engine.getOutputPin();
    ch0["enabled"] = true; // Master is always enabled
    ch0["phase_offset"] = 0; // Master has no phase offset

    // Channels 1-5 (slaves)
    // TODO: Add getters to SignalEngine/PulseGenerator to retrieve actual channel configs
    for (int i = 1; i <= 5; i++) {
        JsonObject ch = channels.add<JsonObject>();
        ch["id"] = i;
        ch["type"] = "slave";
        ch["pin"] = 0; // Unknown without getter
        ch["enabled"] = false; // Unknown without getter
        ch["phase_offset"] = 0; // Unknown without getter
    }

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
    Serial.println("Sent GET /api/channels response");
}

// Handler implementation for POST /api/sync
void ApiRouter::handleSyncPost(AsyncWebServerRequest *request) {
    SignalCmd cmd = {0};
    cmd.type = SIG_CMD_SYNC;
    cmd.paramMode = 0; // SYNC doesn't need parameters

    Serial.printf("API: Received sync trigger request\n");

    if (_engine.sendCommand(cmd)) {
        request->send(200, "application/json", "{\"status\":\"sync triggered\"}");
        Serial.println("API: Sync command sent successfully.");
    } else {
        request->send(503, "application/json", "{\"error\":\"Command queue full\"}");
        Serial.println("API: Command queue full for sync.");
    }
}
