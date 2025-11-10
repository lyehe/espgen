#include "SignalPersistence.h"
#include "build_opts.h"
#include "param_helpers.h"
#include "preferences_keys.h"

// NVS Keys for storing settings in "SignalEngine" namespace
const char* SignalPersistence::NVS_NAMESPACE = "SignalEngine";
const char* SignalPersistence::NVS_KEY_FREQ = "lastFreq";
const char* SignalPersistence::NVS_KEY_DUTY = "lastDuty";
const char* SignalPersistence::NVS_KEY_DUR = "lastDur";

SignalPersistence::SignalPersistence() {
    // Constructor body (if needed)
}

SignalPersistence::~SignalPersistence() {
    // Destructor body (if needed)
}

// ========== Public Methods ==========

SignalSettings SignalPersistence::loadSettings() {
    SignalSettings settings;

    // Load signal parameters from "SignalEngine" namespace
    bool signalParamsLoaded = loadSignalParams(settings);

    // Load device configuration from "device_config" namespace
    bool deviceConfigLoaded = loadDeviceConfig(settings);

    // Mark as valid if at least one namespace was accessible
    settings.valid = (signalParamsLoaded || deviceConfigLoaded);

    return settings;
}

bool SignalPersistence::saveSignalParams(double freq, float duty, float duration) {
    // Validate parameters before saving
    if (!isValidFrequency(freq)) {
        Serial.printf("SignalPersistence: ERROR - Invalid frequency %.2f Hz, not saving\n", freq);
        return false;
    }

    if (!isValidDutyCycle(duty)) {
        Serial.printf("SignalPersistence: ERROR - Invalid duty cycle %.2f, not saving\n", duty);
        return false;
    }

    if (!isValidDuration(duration)) {
        Serial.printf("SignalPersistence: ERROR - Invalid duration %.2f s, not saving\n", duration);
        return false;
    }

    // Open preferences in read-write mode
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) {
        Serial.printf("SignalPersistence: ERROR - Failed to open NVS namespace '%s' for writing\n", NVS_NAMESPACE);
        return false;
    }

    // Save all parameters
    preferences.putDouble(NVS_KEY_FREQ, freq);
    preferences.putFloat(NVS_KEY_DUTY, duty);
    preferences.putFloat(NVS_KEY_DUR, duration);
    preferences.end();

    Serial.printf("SignalPersistence: Saved to NVS - F=%.2f Hz, D=%.2f%%, Dur=%.2f s\n",
                  freq, duty * 100.0, duration);
    return true;
}

bool SignalPersistence::saveOutputPin(uint8_t pin) {
    // Validate pin before saving
    if (!isValidOutputPin(pin)) {
        Serial.printf("SignalPersistence: ERROR - Invalid pin %d, not saving\n", pin);
        return false;
    }

    // Open preferences in read-write mode
    Preferences preferences;
    if (!preferences.begin(DEVICE_CFG_NAMESPACE, false)) {
        Serial.printf("SignalPersistence: ERROR - Failed to open NVS namespace '%s' for writing\n",
                     DEVICE_CFG_NAMESPACE);
        return false;
    }

    // Save pin
    preferences.putUChar(OUTPUT_PIN_KEY, pin);
    preferences.end();

    Serial.printf("SignalPersistence: Saved pin %d to NVS (Namespace: %s, Key: %s)\n",
                  pin, DEVICE_CFG_NAMESPACE, OUTPUT_PIN_KEY);
    return true;
}

bool SignalPersistence::clearSettings() {
    bool success = true;
    Preferences preferences;

    // Clear SignalEngine namespace
    if (preferences.begin(NVS_NAMESPACE, false)) {
        preferences.clear();
        preferences.end();
        Serial.printf("SignalPersistence: Cleared namespace '%s'\n", NVS_NAMESPACE);
    } else {
        Serial.printf("SignalPersistence: WARNING - Failed to open namespace '%s' for clearing\n", NVS_NAMESPACE);
        success = false;
    }

    // Clear device_config namespace (only the keys we manage)
    if (preferences.begin(DEVICE_CFG_NAMESPACE, false)) {
        preferences.remove(OUTPUT_PIN_KEY);
        preferences.end();
        Serial.printf("SignalPersistence: Cleared key '%s' from namespace '%s'\n",
                     OUTPUT_PIN_KEY, DEVICE_CFG_NAMESPACE);
    } else {
        Serial.printf("SignalPersistence: WARNING - Failed to open namespace '%s' for clearing\n",
                     DEVICE_CFG_NAMESPACE);
        success = false;
    }

    return success;
}

bool SignalPersistence::isNvsAccessible() {
    Preferences preferences;
    if (preferences.begin(NVS_NAMESPACE, true)) { // Read-only mode
        preferences.end();
        return true;
    }
    return false;
}

// ========== Private Helper Methods ==========

bool SignalPersistence::loadSignalParams(SignalSettings& settings) {
    Preferences preferences;

    // Try to open SignalEngine namespace (read-only)
    if (!preferences.begin(NVS_NAMESPACE, true)) {
        Serial.printf("SignalPersistence: ERROR - Failed to open NVS namespace '%s'\n", NVS_NAMESPACE);
        Serial.println("SignalPersistence: Using default values for signal parameters");

        // Set defaults from build_opts.h
        settings.frequencyHz = DEFAULT_FREQUENCY_HZ;
        settings.dutyCycle = DEFAULT_DUTY_CYCLE;
        settings.durationSec = 0.0f; // Default duration is 0 (infinite)

        return false;
    }

    // Load values, using compile-time defaults if not found
    double loadedFreq = preferences.getDouble(NVS_KEY_FREQ, DEFAULT_FREQUENCY_HZ);
    float loadedDuty = preferences.getFloat(NVS_KEY_DUTY, DEFAULT_DUTY_CYCLE);
    float loadedDur = preferences.getFloat(NVS_KEY_DUR, 0.0f);
    preferences.end();

    // Validate loaded frequency
    if (!isValidFrequency(loadedFreq)) {
        Serial.printf("SignalPersistence: WARNING - Invalid frequency %.2f Hz in NVS, using default %.2f Hz\n",
                     loadedFreq, DEFAULT_FREQUENCY_HZ);
        loadedFreq = DEFAULT_FREQUENCY_HZ;
    }

    // Validate loaded duty cycle
    if (!isValidDutyCycle(loadedDuty)) {
        Serial.printf("SignalPersistence: WARNING - Invalid duty cycle %.2f in NVS, using default %.2f\n",
                     loadedDuty, DEFAULT_DUTY_CYCLE);
        loadedDuty = DEFAULT_DUTY_CYCLE;
    }

    // Validate loaded duration
    if (!isValidDuration(loadedDur)) {
        Serial.printf("SignalPersistence: WARNING - Invalid duration %.2f s in NVS, using default 0.0s\n",
                     loadedDur);
        loadedDur = 0.0f;
    }

    // Store validated values
    settings.frequencyHz = loadedFreq;
    settings.dutyCycle = loadedDuty;
    settings.durationSec = loadedDur;

    Serial.printf("SignalPersistence: Loaded from NVS - F=%.2f Hz, D=%.2f%%, Dur=%.2f s\n",
                  loadedFreq, loadedDuty * 100.0, loadedDur);

    return true;
}

bool SignalPersistence::loadDeviceConfig(SignalSettings& settings) {
    Preferences preferences;

    // Try to open device_config namespace (read-only)
    if (!preferences.begin(DEVICE_CFG_NAMESPACE, true)) {
        Serial.printf("SignalPersistence: ERROR - Failed to open NVS namespace '%s'\n", DEVICE_CFG_NAMESPACE);
        Serial.printf("SignalPersistence: Using default pin %d\n", DEFAULT_OUTPUT_PIN);

        // Set default pin
        settings.outputPin = DEFAULT_OUTPUT_PIN;

        return false;
    }

    // Load pin value
    uint8_t loadedPin = preferences.getUChar(OUTPUT_PIN_KEY, DEFAULT_OUTPUT_PIN);
    preferences.end();

    // Validate loaded pin
    if (!isValidOutputPin(loadedPin)) {
        Serial.printf("SignalPersistence: ERROR - Invalid pin %d loaded from NVS. Using default %d.\n",
                     loadedPin, DEFAULT_OUTPUT_PIN);

        // Save corrected pin back to NVS
        saveOutputPin(DEFAULT_OUTPUT_PIN);

        loadedPin = DEFAULT_OUTPUT_PIN;
    }

    // Store validated value
    settings.outputPin = loadedPin;

    Serial.printf("SignalPersistence: Loaded Output Pin: %d (Namespace: %s, Key: %s)\n",
                  loadedPin, DEVICE_CFG_NAMESPACE, OUTPUT_PIN_KEY);

    return true;
}

// ========== Validation Methods ==========

bool SignalPersistence::isValidFrequency(double freq) const {
    return ::isValidFrequency(freq); // Use global helper from param_helpers.h
}

bool SignalPersistence::isValidDutyCycle(float duty) const {
    return ::isValidDutyCycle(duty); // Use global helper from param_helpers.h
}

bool SignalPersistence::isValidDuration(float duration) const {
    return ::isValidDuration(duration); // Use global helper from param_helpers.h
}

bool SignalPersistence::isValidOutputPin(uint8_t pin) const {
    return ::isValidOutputPin(pin); // Use global helper from param_helpers.h
}
