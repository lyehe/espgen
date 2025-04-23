// CliHelpTable.cpp
// Stores the definition of CLI commands, arguments, and help text.
// This makes the parser in SerialCLI.cpp cleaner.

#include <Arduino.h>

// Example structure - refine as needed for your parser library or custom parser
typedef struct {
    const char* commandName;
    const char* helpText;
    // Add function pointer or other info for dispatching
    // void (*handler)(int argc, char** argv);
} CommandDef;

// Example command definitions
const CommandDef cliCommands[] = {
    {"help", "Shows this help message"},
    {"start", "start <ch> <freq> <duty> - Start PWM signal"},
    {"stop", "stop <ch> - Stop PWM signal"},
    {"update", "update <ch> <duty> - Update PWM duty cycle"},
    // Add more commands here
    {NULL, NULL} // End marker
};

// Function to print help (used by the 'help' command handler in SerialCLI.cpp)
void printCliHelp(Stream* port) {
    port->println("\nAvailable Commands:");
    for (int i = 0; cliCommands[i].commandName != NULL; i++) {
        port->printf("  %-10s - %s\n", cliCommands[i].commandName, cliCommands[i].helpText);
    }
} 