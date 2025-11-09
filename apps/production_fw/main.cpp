// apps/production_fw/main.cpp
// Main application integrating all components for the final firmware.
// This is the Composition Root for the entire application.

#include <Arduino.h>
#include "build_opts.h"

#include "SignalEngine.h"
#include "signal_iface.h" // Needed for SignalCmd struct
#include <Button2.h>        // Include Button2 library

#if BUILD_SERIAL_CLI
#include "SerialCLI.h"
#include "SignalControllerAdapter.h" // Application layer adapter (Clean Architecture)
#endif

#if BUILD_WEB
#include "WebFacade.h" // WebFacade handles its own adapter internally
#endif

// Button Configuration
#define BUTTON_PIN 23

// Instantiate components following Clean Architecture
// Domain Layer
SignalEngine signalEngine;

// Infrastructure
Button2 button;

// Pointer to signal engine for use in button handler
SignalEngine* signalEnginePtr = nullptr;

// Application Layer (Adapters)
#if BUILD_SERIAL_CLI
SignalControllerAdapter cliAdapter(signalEngine); // Adapter for CLI
SerialCLI serialCLI(cliAdapter); // CLI depends on interface, not concrete class
#endif

#if BUILD_WEB
WebFacade webFacade(signalEngine); // WebFacade creates its own adapter internally
#endif

// Button Tap Handler Function
void handleButtonTap(Button2& btn) {
    if (signalEnginePtr == nullptr) {
        Serial.println("Button Error: SignalEngine pointer not set!");
        return;
    }

    if (signalEnginePtr->isRunning()) {
        // If running, send STOP command
        Serial.println("Button: Sending STOP command.");
        SignalCmd stopCmd = { .type = SIG_CMD_STOP };
        signalEnginePtr->sendCommand(stopCmd);
    } else {
        // If stopped, send START command with last applied parameters
        Serial.println("Button: Sending START command with last params.");
        SignalCmd startCmd;
        startCmd.type = SIG_CMD_START;
        startCmd.frequencyHz = signalEnginePtr->getLastAppliedFrequencyHz();
        startCmd.dutyCycle = signalEnginePtr->getLastAppliedDutyCycle();
        startCmd.durationSec = signalEnginePtr->getLastAppliedDurationSec();
        signalEnginePtr->sendCommand(startCmd);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);
    Serial.println("\n--- ESP32 Trigger Production Firmware ---");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    // Initialize core engine first
    signalEngine.begin();
    signalEnginePtr = &signalEngine; // Assign address to pointer

    // Initialize Button
    button.begin(BUTTON_PIN, INPUT_PULLUP); // Assuming internal pull-up
    button.setTapHandler(handleButtonTap); // Set the tap handler function
    Serial.printf("Button initialized on GPIO %d.\n", BUTTON_PIN);

    // Initialize optional components based on build flags
#if BUILD_SERIAL_CLI
    serialCLI.begin();
    Serial.println("Serial CLI Enabled.");
#endif

#if BUILD_WEB
    webFacade.begin();
    Serial.println("Web Interface Enabled.");
#endif

    Serial.println("Initialization Complete.");
    // Optionally start a default signal or restore last state
    // SignalCmd startCmd = {SIG_CMD_START, 0, DEFAULT_FREQUENCY_HZ, DEFAULT_DUTY_CYCLE};
    // signalEngine.sendCommand(startCmd);
}

void loop() {
    // Run loop functions for components
    signalEngine.loop();
    button.loop(); // IMPORTANT: Call button loop handler

#if BUILD_SERIAL_CLI
    // serialCLI.loop(); // SerialCLI handles input via task/event
#endif

#if BUILD_WEB
    // webFacade.loop(); // WebFacade relies on AsyncWebServer tasks
#endif

    // Keep the main loop relatively light, tasks handle heavy lifting.
    // Add delay or yield if necessary, e.g., if watchdog timer bites.
    // delay(1);
} 