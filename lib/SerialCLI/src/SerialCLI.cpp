#include "SerialCLI.h"
#include "signal_iface.h" // For SignalCmd struct and enum

// Include CliHelpTable definitions if needed
// extern const CommandDef cliCommands[]; 

SerialCLI::SerialCLI(ISignalController& controller) :
    _controller(controller),
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

    SignalCmd cmd = {}; // Zero-initialize command structure
    bool commandSent = false;

    // Simple parsing logic (replace with more robust parser later)
    if (_inputBuffer == "help") {
        Serial.println("Available Commands:");
        Serial.println("Basic Control:");
        Serial.println("  start              - Start signal generation (uses last/default params)");
        Serial.println("  stop               - Stop signal generation");
        Serial.println("  update freq duty   - Set frequency (Hz) and duty cycle (0.0-1.0)");
        Serial.println("  freq <hz>          - Set frequency only");
        Serial.println("  duty <0.0-1.0>     - Set duty cycle only");
        Serial.println("Exact Parameters:");
        Serial.println("  period <us>        - Set period in microseconds");
        Serial.println("  pulsewidth <us>    - Set pulse width in microseconds");
        Serial.println("  duration <sec>     - Set duration in seconds (0=infinite)");
        Serial.println("  pulsecount <n>     - Set exact pulse count (0=infinite)");
        Serial.println("Multi-Channel:");
        Serial.println("  channel <id> <pin> <phase> <en>  - Configure slave channel (1-5)");
        Serial.println("                       id: 1-5, pin: GPIO, phase: 0-360, en: 0/1");
        Serial.println("  sync               - Trigger multi-channel synchronization");
        Serial.println("Advanced:");
        Serial.println("  polarity <h|l>     - Set polarity (h=high, l=low)");
        Serial.println("  setpin <gpio>      - Set master output pin (0-33)");
        Serial.println("  indicator <gpio>   - Set status indicator pin (0=disabled)");
        Serial.println("  status             - Show current status");
        Serial.println("Performance & Presets:");
        Serial.println("  metrics            - Show performance metrics");
        Serial.println("  savepreset <name>  - Save current config as preset");
        Serial.println("  loadpreset <name>  - Load and apply a preset");
        Serial.println("  delpreset <name>   - Delete a preset");
        Serial.println("  listpresets        - List all saved presets");

    } else if (_inputBuffer == "start") {
        cmd.type = SIG_CMD_START;
        cmd.paramMode = 0; // No params specified, will use last applied values
        commandSent = _controller.sendCommand(cmd);

    } else if (_inputBuffer == "stop") {
        cmd.type = SIG_CMD_STOP;
        cmd.paramMode = 0; // STOP doesn't need parameters
        commandSent = _controller.sendCommand(cmd);

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
            commandSent = _controller.sendCommand(cmd);
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
            commandSent = _controller.sendCommand(cmd);
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
            commandSent = _controller.sendCommand(cmd);
        } else {
            Serial.println("Error: Invalid format. Use: duty <0.0-1.0>");
        }
    } else if (_inputBuffer.startsWith("setpin ")) {
        // Example: setpin 18
        int pin = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "setpin %d", &pin);
        if (argsParsed == 1) {
            if (pin >= 0 && pin <= 33) {
                cmd.type = SIG_CMD_SET_PIN;
                cmd.pin = (uint8_t)pin;
                commandSent = _controller.sendCommand(cmd);
            } else {
                 Serial.println("Error: Invalid pin. Must be between 0 and 33.");
                 commandSent = false;
            }
        } else {
            Serial.println("Error: Invalid format. Use: setpin <gpio>");
        }
    } else if (_inputBuffer.startsWith("period ")) {
        // Example: period 10000
        uint32_t period = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "period %lu", &period);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_UPDATE_ALL;
            cmd.periodUs = period;
            cmd.paramMode = PARAM_USE_PERIOD;
            commandSent = _controller.sendCommand(cmd);
            Serial.printf("Setting period to %lu us\n", period);
        } else {
            Serial.println("Error: Invalid format. Use: period <microseconds>");
        }
    } else if (_inputBuffer.startsWith("pulsewidth ")) {
        // Example: pulsewidth 5000
        uint32_t width = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "pulsewidth %lu", &width);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_UPDATE_ALL;
            cmd.pulseWidthUs = width;
            cmd.paramMode = PARAM_USE_PULSE_WIDTH;
            commandSent = _controller.sendCommand(cmd);
            Serial.printf("Setting pulse width to %lu us\n", width);
        } else {
            Serial.println("Error: Invalid format. Use: pulsewidth <microseconds>");
        }
    } else if (_inputBuffer.startsWith("duration ")) {
        // Example: duration 10.5
        float duration = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "duration %f", &duration);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_START;
            cmd.durationSec = duration;
            cmd.paramMode = PARAM_USE_DURATION;
            commandSent = _controller.sendCommand(cmd);
            Serial.printf("Setting duration to %.2f seconds\n", duration);
        } else {
            Serial.println("Error: Invalid format. Use: duration <seconds>");
        }
    } else if (_inputBuffer.startsWith("pulsecount ")) {
        // Example: pulsecount 1000
        uint64_t count = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "pulsecount %llu", &count);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_START;
            cmd.pulseCount = count;
            cmd.paramMode = PARAM_USE_PULSE_COUNT;
            commandSent = _controller.sendCommand(cmd);
            Serial.printf("Setting pulse count to %llu\n", count);
        } else {
            Serial.println("Error: Invalid format. Use: pulsecount <count>");
        }
    } else if (_inputBuffer.startsWith("channel ")) {
        // Example: channel 1 19 90 1
        int channelId = 0, pin = 0, phase = 0, enabled = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "channel %d %d %d %d",
                                &channelId, &pin, &phase, &enabled);
        if (argsParsed == 4) {
            if (channelId >= 1 && channelId <= 5) {
                cmd.type = SIG_CMD_CONFIG_CHANNEL;
                cmd.channel = (uint8_t)channelId;
                cmd.pin = (uint8_t)pin;
                cmd.phaseOffset = (float)phase;
                cmd.enabled = (enabled != 0);
                cmd.paramMode = PARAM_USE_PHASE_DEGREES;
                commandSent = _controller.sendCommand(cmd);
                Serial.printf("Configuring channel %d: pin=%d, phase=%d deg, enabled=%s\n",
                             channelId, pin, phase, enabled ? "yes" : "no");
            } else {
                Serial.println("Error: Channel ID must be 1-5 (slave channels)");
                commandSent = false;
            }
        } else {
            Serial.println("Error: Invalid format. Use: channel <id> <pin> <phase> <enabled>");
        }
    } else if (_inputBuffer.startsWith("polarity ")) {
        // Example: polarity h  or  polarity l
        char polarityChar = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "polarity %c", &polarityChar);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_UPDATE_ALL;
            bool validPolarity = true;
            if (polarityChar == 'h' || polarityChar == 'H') {
                cmd.polarity = POLARITY_ACTIVE_HIGH;
                Serial.println("Setting polarity to ACTIVE HIGH");
            } else if (polarityChar == 'l' || polarityChar == 'L') {
                cmd.polarity = POLARITY_ACTIVE_LOW;
                Serial.println("Setting polarity to ACTIVE LOW");
            } else {
                Serial.println("Error: Polarity must be 'h' (high) or 'l' (low)");
                validPolarity = false;
            }
            if (validPolarity) {
                commandSent = _controller.sendCommand(cmd);
            }
        } else {
            Serial.println("Error: Invalid format. Use: polarity <h|l>");
        }
    } else if (_inputBuffer.startsWith("indicator ")) {
        // Example: indicator 27
        int pin = 0;
        int argsParsed = sscanf(_inputBuffer.c_str(), "indicator %d", &pin);
        if (argsParsed == 1) {
            cmd.type = SIG_CMD_SET_INDICATOR;
            cmd.pin = (uint8_t)pin;
            commandSent = _controller.sendCommand(cmd);
            if (pin == 0) {
                Serial.println("Indicator disabled");
            } else {
                Serial.printf("Setting indicator pin to GPIO %d\n", pin);
            }
        } else {
            Serial.println("Error: Invalid format. Use: indicator <gpio>");
        }
    } else if (_inputBuffer == "sync") {
        cmd.type = SIG_CMD_SYNC;
        cmd.paramMode = 0;
        commandSent = _controller.sendCommand(cmd);
        Serial.println("Triggering multi-channel synchronization");
    } else if (_inputBuffer == "status") {
        // Display current status
        SignalStatus_t status;
        SignalError err = _controller.getStatus(status);
        if (err == SIG_OK) {
            Serial.println("=== Current Status ===");
            Serial.printf("  Running: %s\n", status.isRunning ? "YES" : "NO");
            Serial.printf("  Frequency: %.2f Hz\n", status.frequency);
            Serial.printf("  Duty Cycle: %.2f%%\n", status.dutyCycle * 100.0);
            Serial.printf("  Period: %lu us\n", status.periodUs);
            Serial.printf("  Pulse Width: %lu us\n", status.pulseWidthUs);
            Serial.printf("  Duration: %.2f s\n", status.lastAppliedDurationSec);
            Serial.printf("  Output Pin: %d\n", _controller.getOutputPin());
            Serial.println("======================");
        } else {
            Serial.println("Error: Failed to retrieve status");
        }
    } else if (_inputBuffer == "metrics") {
        // Display performance metrics
        PerformanceMetrics metrics;
        _controller.getPerformanceMetrics(metrics);

        Serial.println("=== Performance Metrics ===");
        Serial.printf("  Total Commands: %llu\n", metrics.totalCommands);
        Serial.printf("  Avg Latency: %llu us\n", metrics.avgCommandLatencyUs);
        Serial.printf("  Max Latency: %llu us\n", metrics.maxCommandLatencyUs);
        Serial.printf("  Min Latency: %llu us\n",
            (metrics.minCommandLatencyUs == UINT64_MAX) ? 0 : metrics.minCommandLatencyUs);
        Serial.printf("  Free Heap: %lu bytes\n", metrics.freeHeap);
        Serial.printf("  Min Free Heap: %lu bytes\n",
            (metrics.minFreeHeap == UINT32_MAX) ? 0 : metrics.minFreeHeap);
        Serial.printf("  Uptime: %llu ms (%.2f hours)\n",
            metrics.uptimeMs, metrics.uptimeMs / 3600000.0);
        Serial.printf("  Total Events: %llu\n", metrics.totalEvents);
        Serial.printf("  Failed Events: %llu\n", metrics.failedEvents);
        if (metrics.totalEvents > 0) {
            float successRate = (metrics.totalEvents - metrics.failedEvents) * 100.0 / metrics.totalEvents;
            Serial.printf("  Event Success Rate: %.2f%%\n", successRate);
        }
        Serial.println("===========================");
    } else if (_inputBuffer.startsWith("savepreset ")) {
        // Example: savepreset myconfig
        char name[20];
        int argsParsed = sscanf(_inputBuffer.c_str(), "savepreset %19s", name);
        if (argsParsed == 1) {
            if (strlen(name) > 15) {
                Serial.println("Error: Preset name too long (max 15 chars)");
            } else if (_controller.savePreset(name)) {
                Serial.printf("Preset '%s' saved successfully\n", name);
            } else {
                Serial.printf("Error: Failed to save preset '%s'\n", name);
            }
        } else {
            Serial.println("Error: Invalid format. Use: savepreset <name>");
        }
    } else if (_inputBuffer.startsWith("loadpreset ")) {
        // Example: loadpreset myconfig
        char name[20];
        int argsParsed = sscanf(_inputBuffer.c_str(), "loadpreset %19s", name);
        if (argsParsed == 1) {
            if (_controller.loadPreset(name)) {
                Serial.printf("Preset '%s' loaded and applied successfully\n", name);
            } else {
                Serial.printf("Error: Preset '%s' not found or failed to load\n", name);
            }
        } else {
            Serial.println("Error: Invalid format. Use: loadpreset <name>");
        }
    } else if (_inputBuffer.startsWith("delpreset ")) {
        // Example: delpreset myconfig
        char name[20];
        int argsParsed = sscanf(_inputBuffer.c_str(), "delpreset %19s", name);
        if (argsParsed == 1) {
            if (_controller.deletePreset(name)) {
                Serial.printf("Preset '%s' deleted successfully\n", name);
            } else {
                Serial.printf("Error: Preset '%s' not found or failed to delete\n", name);
            }
        } else {
            Serial.println("Error: Invalid format. Use: delpreset <name>");
        }
    } else if (_inputBuffer == "listpresets") {
        // List all saved presets
        char buffer[512];
        int count = _controller.listPresets(buffer, sizeof(buffer));

        Serial.println("=== Saved Presets ===");
        if (count > 0) {
            Serial.printf("Found %d preset(s):\n", count);
            // Parse and display (format: "name1,name2,name3")
            char* token = strtok(buffer, ",");
            int idx = 1;
            while (token != NULL) {
                Serial.printf("  %d. %s\n", idx++, token);
                token = strtok(NULL, ",");
            }
        } else {
            Serial.println("  No presets saved");
        }
        Serial.println("====================");
    } else {
        Serial.printf("Error: Unknown command '%s'\n", _inputBuffer.c_str());
    }

    if (commandSent) {
        Serial.println("OK: Command sent to engine.");
    } else if (_inputBuffer != "help" && _inputBuffer != "status" && _inputBuffer != "metrics" &&
               _inputBuffer != "listpresets" && !_inputBuffer.startsWith("savepreset ") &&
               !_inputBuffer.startsWith("loadpreset ") && !_inputBuffer.startsWith("delpreset ")) {
        // Error message was printed by sendCommand or parsing logic
        // Consider adding more specific error feedback here if needed
         Serial.println("Error: Failed to send command (queue full? invalid?).");
    }
} 