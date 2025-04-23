// apps/web_mock/main.cpp
// Test application focusing on WebFacade (Phase 6: REST API)

#include <Arduino.h>
#include "WebFacade.h" // Include the WebFacade library
#include "SignalEngine.h" // Include the SignalEngine library

// Global instances
SignalEngine signalEngine; // Create the signal engine object
WebFacade webFacade(signalEngine); // Create WebFacade, passing the engine

void setup() {
    Serial.begin(115200);
    while (!Serial); // Wait for Serial port to connect (optional)
    delay(1000); // Short delay
    Serial.println("\n--- Web Mock Test App (Phase 6 Test) ---");

    // Initialize SignalEngine first
    signalEngine.begin();
    Serial.println("SignalEngine initialized.");

    // Initialize WebFacade (which handles WiFi, starts server, registers API routes)
    webFacade.begin();

    Serial.println("WebFacade initialized. Setup complete.");
    Serial.println("Check Serial Monitor for IP address or connect to TRIGGER-SETUP AP.");
}

void loop() {
    // AsyncWebServer handles requests in the background.
    // SignalEngine runs its tasks in the background.
    // No specific code needed in loop() for Phase 6 testing of WebFacade/ApiRouter.

    // We might need signalEngine.loop() if it has main-loop tasks,
    // but the plan suggests it uses FreeRTOS tasks. Let's assume no loop call needed for now.
    // signalEngine.loop();

    delay(100); // Small delay to yield
} 