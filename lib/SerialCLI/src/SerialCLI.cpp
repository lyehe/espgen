#include "SerialCLI.h"
#include "signal_iface.h" // For SignalCmd struct and enum

// Include CliHelpTable definitions if needed
// extern const CommandDef cliCommands[]; 

SerialCLI::SerialCLI(SignalEngine& engine) : 
    _engine(engine),
    _inputBuffer(""),
    _commandReady(false)
{
    _inputBuffer.reserve(128); // Pre-allocate buffer capacity
}

void SerialCLI::begin() {
    Serial.println("DEBUG: cli.begin() called.");
    Serial.println("--- Serial Command Line Interface --- V0.1 ---");
    Serial.println("Type 'help' for available commands.");
    Serial.print("> "); // Print initial prompt
    Serial.println("DEBUG: cli.begin() finished.");
}

void SerialCLI::handleSerial() {
    // Add print at the start of handleSerial (can be spammy)
    // Serial.println("DEBUG: handleSerial() called.");
    bool dataReceived = false;
    while (Serial.available() > 0) {
        dataReceived = true; // Mark that we received something
        char receivedChar = Serial.read();

        if (receivedChar == '\n' || receivedChar == '\r') { // End of line detected
            if (_inputBuffer.length() > 0) {
                Serial.println(); // Echo newline
                _commandReady = true;
            } else {
                // Empty line, just print a new prompt
                Serial.print("> "); 
            }
        } else if (receivedChar == '\b' || receivedChar == 127) { // Handle backspace
            if (_inputBuffer.length() > 0) {
                _inputBuffer.remove(_inputBuffer.length() - 1);
                Serial.print("\b \b"); // Erase character on terminal
            }
        } else if (isPrintable(receivedChar)) { // Accumulate printable characters
            if (_inputBuffer.length() < 127) { // Prevent buffer overflow
                _inputBuffer += receivedChar;
                Serial.print(receivedChar); // Echo character
            }
        }
    }

    // Optional: Print if any data was received in this call
    // if (dataReceived) { Serial.print("."); } 

    // If a command line is ready, process it
    if (_commandReady) {
        parseAndExecute();
        _inputBuffer = ""; // Clear buffer for next command
        _commandReady = false;
        Serial.print("> "); // Print prompt for next command
    }
}

void SerialCLI::parseAndExecute() {
    _inputBuffer.trim(); // Remove leading/trailing whitespace
    _inputBuffer.toLowerCase(); // Convert to lowercase for case-insensitive matching
    Serial.printf("Processing: [%s]\n", _inputBuffer.c_str());

    SignalCmd cmd = {0}; // Zero-initialize command structure
    bool commandSent = false;

    // Simple parsing logic (replace with more robust parser later)
    if (_inputBuffer == "help") {
        Serial.println("Available Commands:");
        Serial.println("  start              - Start signal generation (uses last/default params)");
        Serial.println("  stop               - Stop signal generation");
        Serial.println("  update freq duty   - Set frequency (Hz) and duty cycle (0.0-1.0)");
        Serial.println("  freq <hz>          - Set frequency only");
        Serial.println("  duty <0.0-1.0>     - Set duty cycle only");
        Serial.println("  setpin <gpio>      - Set output pin (12-19)");
        Serial.println("  status             - Show current status (Not Implemented)");
        // Add more help text

    } else if (_inputBuffer == "start") {
        cmd.type = SIG_CMD_START;
        cmd.paramMode = 0; // No params specified, will use last applied values
        commandSent = _engine.sendCommand(cmd);

    } else if (_inputBuffer == "stop") {
        cmd.type = SIG_CMD_STOP;
        cmd.paramMode = 0; // STOP doesn't need parameters
        commandSent = _engine.sendCommand(cmd);

    } else if (_inputBuffer.startsWith("update ")) {
        // Example: update 1000 0.5
        double freq = 0;
        float duty = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "update %lf %f", &freq, &duty);
        if (argsParsed == 2) {
            cmd.type = SIG_CMD_UPDATE_ALL;
            cmd.frequencyHz = freq;
            cmd.dutyCycle = duty;
            cmd.paramMode = PARAM_USE_FREQUENCY | PARAM_USE_DUTY_CYCLE;
            commandSent = _engine.sendCommand(cmd);
        } else {
            Serial.println("Error: Invalid format. Use: update <freq_hz> <duty_0.0-1.0>");
        }
    } else if (_inputBuffer.startsWith("freq ")) {
        // Example: freq 1500
        double freq = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "freq %lf", &freq);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_UPDATE_FREQ;
            cmd.frequencyHz = freq;
            cmd.paramMode = PARAM_USE_FREQUENCY;
            commandSent = _engine.sendCommand(cmd);
        } else {
            Serial.println("Error: Invalid format. Use: freq <hz>");
        }
    } else if (_inputBuffer.startsWith("duty ")) {
        // Example: duty 0.75
        float duty = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "duty %f", &duty);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_UPDATE_DUTY;
            cmd.dutyCycle = duty;
            cmd.paramMode = PARAM_USE_DUTY_CYCLE;
            commandSent = _engine.sendCommand(cmd);
        } else {
            Serial.println("Error: Invalid format. Use: duty <0.0-1.0>");
        }
    } else if (_inputBuffer.startsWith("setpin ")) {
        // Example: setpin 18
        int pin = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "setpin %d", &pin);
        if (argsParsed == 1) {
            if (pin >= 12 && pin <= 19) {
                cmd.type = SIG_CMD_SET_PIN;
                cmd.pin = (uint8_t)pin;
                commandSent = _engine.sendCommand(cmd);
            } else {
                 Serial.println("Error: Invalid pin. Must be between 12 and 19.");
                 commandSent = false; // Explicitly mark as not sent
            }
        } else {
            Serial.println("Error: Invalid format. Use: setpin <gpio_12_to_19>");
        }
    } else {
        Serial.printf("Error: Unknown command '%s'\n", _inputBuffer.c_str());
    }

    if (commandSent) {
        Serial.println("OK: Command sent to engine.");
    } else if (_inputBuffer != "help") { // Don't print error for help command
        // Error message was printed by sendCommand or parsing logic
        // Consider adding more specific error feedback here if needed
         Serial.println("Error: Failed to send command (queue full? invalid?).");
    }
} 