// apps/production_fw/main.cpp
// Main application integrating all components for the final firmware.

#include <Arduino.h>
#include "build_opts.h"

#include "SignalEngine.h"

#if BUILD_SERIAL_CLI
#include "SerialCLI.h"
#endif

#if BUILD_WEB
#include "WebFacade.h"
#endif

// Instantiate components
SignalEngine signalEngine;

#if BUILD_SERIAL_CLI
SerialCLI serialCLI(signalEngine);
#endif

#if BUILD_WEB
WebFacade webFacade(signalEngine);
#endif

void setup() {
    Serial.begin(115200);
    while (!Serial);
    delay(1000);
    Serial.println("\n--- ESP32 Trigger Production Firmware ---");
    Serial.printf("Build: %s %s\n", __DATE__, __TIME__);

    // Initialize core engine first
    signalEngine.begin();

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