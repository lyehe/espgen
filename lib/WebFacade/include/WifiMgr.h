#ifndef WIFIMGR_H
#define WIFIMGR_H

#include <Arduino.h>

// Forward declaration if needed, or include headers
// #include <WiFiManager.h>
// #include <Preferences.h>

enum WifiState {
    NO_WIFI_CONFIGURED, // No credentials saved
    CONNECTING,
    STA_CONNECTED,      // Connected to router
    AP_MODE,            // Running as Access Point (Captive Portal)
    CONNECTION_FAILED   // Failed to connect after trying
};

class WifiMgr {
public:
    WifiMgr();

    /**
     * @brief Initializes the WiFi Manager.
     * Attempts to connect using saved credentials.
     * If connection fails or no credentials exist, starts the captive portal.
     * 
     * @param apName The name of the Access Point to create for the portal.
     * @param apPassword Optional password for the Access Point.
     * @param connectionTimeout_s Timeout in seconds to wait for STA connection before starting AP.
     * @param portalTimeout_s Timeout in seconds for the captive portal configuration page.
     * @return true if connected to WiFi (STA mode), false otherwise (AP mode or failed).
     */
    bool begin(const char *apName = "TRIGGER-SETUP", const char *apPassword = nullptr, 
               unsigned long connectionTimeout_s = 15, unsigned long portalTimeout_s = 180);

    /**
     * @brief Call this periodically in the main loop to handle WiFiManager tasks.
     */
    void loop();

    /**
     * @brief Gets the current WiFi state.
     * @return The current WifiState enum value.
     */
    WifiState getState() const;

    /**
     * @brief Resets WiFi settings stored in NVS and disconnects.
     */
    void resetSettings();

private:
    // Callback when WiFiManager saves configuration
    static void saveConfigCallback();

    // Flag to indicate if config save is needed
    static bool _shouldSaveConfig;

    // Instance of WiFiManager
    // WiFiManager _wm;

    // Instance of Preferences for NVS
    // Preferences _preferences;

    WifiState _currentState;
    unsigned long _connectTimeoutMs;
    unsigned long _portalTimeoutMs;
    unsigned long _connectionStartTime;
    bool _portalRunning;
};

#endif // WIFIMGR_H 