// common/build_opts.h
#pragma once

// These flags are typically set via platformio.ini build_flags
// but having defaults here can be useful for standalone builds or clarity.

#ifndef BUILD_WEB
  #define BUILD_WEB 1 // Build web server components by default
#endif

#ifndef BUILD_SERIAL_CLI
  #define BUILD_SERIAL_CLI 1 // Build serial CLI by default
#endif

#ifndef BUILD_OTA
  #define BUILD_OTA 1 // Enable OTA by default unless specified
#endif

// --- Other Compile-Time Configurations ---

// Signal Engine Configuration
#define SIGNAL_ENGINE_CMD_QUEUE_LEN 10
#define DEFAULT_OUTPUT_PIN 18       // GPIO pin for the main signal output
#define DEFAULT_FREQUENCY_HZ 30.0 // Default signal frequency in Hz
#define DEFAULT_DUTY_CYCLE 0.25f     // Default signal duty cycle (0.0 to 1.0)
#define DEFAULT_LEDC_CHANNEL 0      // Default LEDC channel (0-15)
#define DEFAULT_LEDC_RESOLUTION 8   // Default LEDC resolution in bits (1-16, lower is often fine)

// Web Server Configuration
#define WEB_SERVER_PORT 80
#define WEB_HOSTNAME "trigger"

// Wifi Manager Configuration
#define WIFI_AP_SSID "TRIGGER-SETUP"
#define WIFI_CONNECT_TIMEOUT_S 10

// --- Feature Flags ---
#define ENABLE_SWEEP 0 // Set to 1 to enable compilation of sweep/modulation features (future)

// Add other project-wide #defines as needed 