// apps/signal_cli/main.cpp
// Test application focusing on SignalEngine and SerialCLI

#include <Arduino.h>
#include "PulseGenerator.h" // Concrete implementation
#include "SignalEngine.h"
#include "signal_iface.h" // Include for SignalCmd
#include "SerialCLI.h"    // Include the new Serial CLI library
#include "SignalControllerAdapter.h" // Application layer adapter (Clean Architecture)

PulseGenerator pulseGen; // Hardware implementation
SignalEngine engine(pulseGen); // Inject dependency
SignalControllerAdapter adapter(engine); // Adapter bridges domain to application layer
SerialCLI* cli_ptr = nullptr; // Declare pointer globally

void setup() {
    Serial.begin(115200);
    while (!Serial); // Wait for serial connection
    delay(1000); // Small delay after serial connection

    Serial.println("\n\n--- ESP32 Signal CLI App (Phase 3) ---");

    engine.begin(); // Initialize the signal engine (starts dispatcher task)
    Serial.println("DEBUG: Engine begin() finished.");

    cli_ptr = new SerialCLI(adapter); // Instantiate CLI with adapter (Clean Architecture)
    Serial.println("DEBUG: CLI object created.");
    cli_ptr->begin();    // Initialize the Serial CLI (prints welcome message)

    Serial.println("DEBUG: Setup complete.");
    // Test commands previously here are now REMOVED.
}

void loop() {
    // Add a print at the beginning of loop
    // Serial.println("DEBUG: loop() start"); 
    // Note: Printing in every loop can flood the serial, use sparingly 
    //       or add a counter to print only occasionally.

    // Call engine's loop (currently empty)
    engine.loop();

    // Handle incoming serial commands
    if (cli_ptr) { // Check if pointer is valid before using
        cli_ptr->handleSerial();
    }

    // The main PWM signal and heartbeat task run independently.
    delay(10); // Small delay to yield time to other tasks
} 