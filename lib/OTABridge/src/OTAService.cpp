#include "OTAService.h"
#include <ElegantOTA.h>
#include <ESPAsyncWebServer.h> // Required for AsyncWebServer parameter

OTAService::OTAService() {
    // Constructor, if needed for initialization
}

void OTAService::begin(AsyncWebServer *server) {
    if (server) {
        ElegantOTA.begin(server); // Use ElegantOTA's begin method
        // Note: You can add username/password arguments here if desired:
        // ElegantOTA.begin(server, "username", "password");
        Serial.println("OTA Update Server started at /update");
    } else {
        Serial.println("Error: Web server pointer is null, cannot start OTA.");
    }
} 