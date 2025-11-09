#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#include <Arduino.h>
#include "ISignalController.h" // Application layer interface (Clean Architecture)
#include "signal_iface.h"      // For SignalCmd struct

/**
 * @brief Serial CLI (Presentation Layer)
 *
 * Handles serial UART commands and routes them to application services.
 * Follows Clean Architecture by depending on interfaces, not concrete implementations.
 *
 * Responsibilities:
 * - Parse serial commands
 * - Validate input (presentation-level validation)
 * - Call application services
 * - Format serial responses
 */
class SerialCLI {
public:
    // Constructor - takes application service interface (Dependency Inversion)
    SerialCLI(ISignalController& controller);

    // Call this repeatedly from the main loop to process incoming serial data
    void handleSerial();

    // Initialize (e.g., print help message)
    void begin();

private:
    ISignalController& _controller; // Application service interface (not concrete class!)
    String _inputBuffer;            // Buffer to hold incoming serial characters
    bool _commandReady;             // Flag indicating a full line has been received

    // Parses the command stored in _inputBuffer and sends it to the engine
    void parseAndExecute();
};

#endif // SERIAL_CLI_H 