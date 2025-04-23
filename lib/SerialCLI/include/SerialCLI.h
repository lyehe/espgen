#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#include <Arduino.h>
#include "SignalEngine.h" // To access SignalEngine::sendCommand and SignalCmd

class SerialCLI {
public:
    // Constructor - takes a reference to the SignalEngine instance
    SerialCLI(SignalEngine& engine);

    // Call this repeatedly from the main loop to process incoming serial data
    void handleSerial();

    // Initialize (e.g., print help message)
    void begin();

private:
    SignalEngine& _engine; // Reference to the signal engine
    String _inputBuffer;   // Buffer to hold incoming serial characters
    bool _commandReady;    // Flag indicating a full line has been received

    // Parses the command stored in _inputBuffer and sends it to the engine
    void parseAndExecute(); 
};

#endif // SERIAL_CLI_H 