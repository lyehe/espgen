#ifndef SIGNAL_PERSISTENCE_H
#define SIGNAL_PERSISTENCE_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief Manages persistent storage for SignalEngine using ESP32 NVS.
 *
 * This class encapsulates all Non-Volatile Storage (NVS) operations,
 * providing a clean interface for loading and saving signal configuration.
 *
 * Responsibilities:
 * - Load signal parameters from NVS on startup
 * - Save signal parameters to NVS when changed
 * - Manage two NVS namespaces: SignalEngine settings and device config
 * - Validate loaded values and handle NVS errors gracefully
 *
 * Design Pattern: Single Responsibility Principle
 * Storage: ESP32 NVS (Non-Volatile Storage) - survives power cycles
 */

/**
 * @brief Structure holding all signal settings loaded from or saved to NVS
 */
struct SignalSettings {
    // Signal parameters (from "SignalEngine" namespace)
    double frequencyHz;
    float dutyCycle;
    float durationSec;

    // Device configuration (from "device_config" namespace)
    uint8_t outputPin;

    // Validation flag
    bool valid; // true if settings were successfully loaded and validated

    // Constructor with defaults
    SignalSettings() :
        frequencyHz(0.0),
        dutyCycle(0.0f),
        durationSec(0.0f),
        outputPin(0),
        valid(false)
    {}
};

class SignalPersistence {
public:
    SignalPersistence();
    ~SignalPersistence();

    /**
     * @brief Load all settings from NVS (both namespaces)
     *
     * Loads signal parameters from "SignalEngine" namespace and
     * device configuration from "device_config" namespace.
     * Validates all loaded values and returns defaults if invalid.
     *
     * @return SignalSettings structure with loaded/default values
     *         valid flag indicates if NVS was accessible
     */
    SignalSettings loadSettings();

    /**
     * @brief Save signal parameters to NVS
     *
     * Saves frequency, duty cycle, and duration to "SignalEngine" namespace.
     * These are the parameters that change during operation.
     *
     * @param freq Frequency in Hz
     * @param duty Duty cycle (0.0 to 1.0)
     * @param duration Duration in seconds
     * @return true if saved successfully, false on error
     */
    bool saveSignalParams(double freq, float duty, float duration);

    /**
     * @brief Save output pin configuration to NVS
     *
     * Saves pin to "device_config" namespace (shared with other modules).
     * This is device-level configuration that rarely changes.
     *
     * @param pin GPIO pin number
     * @return true if saved successfully, false on error
     */
    bool saveOutputPin(uint8_t pin);

    /**
     * @brief Clear all settings from NVS (factory reset)
     *
     * Removes all keys from both "SignalEngine" and "device_config" namespaces.
     * Useful for testing or factory reset functionality.
     *
     * @return true if cleared successfully, false on error
     */
    bool clearSettings();

    /**
     * @brief Check if NVS is accessible
     *
     * Attempts to open a namespace to verify NVS functionality.
     * Useful for diagnostics.
     *
     * @return true if NVS is working, false otherwise
     */
    bool isNvsAccessible();

private:
    // NVS namespace and keys for signal parameters
    static const char* NVS_NAMESPACE;        // "SignalEngine"
    static const char* NVS_KEY_FREQ;         // "lastFreq"
    static const char* NVS_KEY_DUTY;         // "lastDuty"
    static const char* NVS_KEY_DUR;          // "lastDur"

    // Device config namespace and key (shared with other modules)
    // Defined in preferences_keys.h: DEVICE_CFG_NAMESPACE, OUTPUT_PIN_KEY

    /**
     * @brief Helper to load signal parameters from SignalEngine namespace
     *
     * @param settings Reference to settings structure to populate
     * @return true if namespace opened successfully, false otherwise
     */
    bool loadSignalParams(SignalSettings& settings);

    /**
     * @brief Helper to load device config from device_config namespace
     *
     * @param settings Reference to settings structure to populate
     * @return true if namespace opened successfully, false otherwise
     */
    bool loadDeviceConfig(SignalSettings& settings);

    /**
     * @brief Validate frequency value
     *
     * @param freq Frequency to validate
     * @return true if valid (> 0 and <= 8 MHz), false otherwise
     */
    bool isValidFrequency(double freq) const;

    /**
     * @brief Validate duty cycle value
     *
     * @param duty Duty cycle to validate
     * @return true if valid (0.0 to 1.0), false otherwise
     */
    bool isValidDutyCycle(float duty) const;

    /**
     * @brief Validate duration value
     *
     * @param duration Duration to validate
     * @return true if valid (>= 0), false otherwise
     */
    bool isValidDuration(float duration) const;

    /**
     * @brief Validate GPIO pin number
     *
     * @param pin Pin number to validate
     * @return true if valid ESP32 GPIO pin, false otherwise
     */
    bool isValidOutputPin(uint8_t pin) const;
};

#endif // SIGNAL_PERSISTENCE_H
