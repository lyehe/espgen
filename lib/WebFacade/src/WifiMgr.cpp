#include "WifiMgr.h"
#include <WiFi.h>
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
#include <Preferences.h>      // For NVS storage
#include "preferences_keys.h" // Shared NVS keys

// Define static members
bool WifiMgr::_shouldSaveConfig = false;

// Callback notifying us of the need to save config
void WifiMgr::saveConfigCallback()
{
    Serial.println("Should save config");
    WifiMgr::_shouldSaveConfig = true;
    // Note: WiFiManager library saves credentials automatically.
    // This callback is more for custom parameters if you add them.
}

WifiMgr::WifiMgr() : _currentState(WifiState::NO_WIFI_CONFIGURED),
                     _connectTimeoutMs(0),
                     _portalTimeoutMs(0),
                     _connectionStartTime(0),
                     _portalRunning(false)
{
    // Constructor can remain simple
}

bool WifiMgr::begin(const char *apName, const char *apPassword,
                    unsigned long connectionTimeout_s, unsigned long portalTimeout_s)
{

    _connectTimeoutMs = connectionTimeout_s * 1000;
    _portalTimeoutMs = portalTimeout_s * 1000;

    WiFiManager wifiManager;

    // Set timeouts
    wifiManager.setConnectTimeout(_connectTimeoutMs / 1000);     // WiFiManager uses seconds
    wifiManager.setConfigPortalTimeout(_portalTimeoutMs / 1000); // WiFiManager uses seconds

    // Set callback for saving config
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    // Set Dark Mode -> Uncomment if you have includes for it
    // wifiManager.setClass("invert");

    // Set static ip - Example, normally you'd load these from NVS if needed
    // IPAddress _ip, _gw, _sn;
    // _ip.fromString("192.168.1.150");
    // _gw.fromString("192.168.1.1");
    // _sn.fromString("255.255.255.0");
    // wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

    // Add custom parameters here if needed using WiFiManagerParameter
    // WiFiManagerParameter custom_field("custom", "Custom Field", "default value", 40);
    // wifiManager.addParameter(&custom_field);

    // Attempt to autoconnect. If it fails, it starts the config portal.
    // The portal name (SSID) is defined by 'apName', password by 'apPassword'
    Serial.println("Attempting auto-connect...");
    _currentState = WifiState::CONNECTING;
    _connectionStartTime = millis();

    bool connected = wifiManager.autoConnect(apName, apPassword);

    if (connected)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        _currentState = WifiState::STA_CONNECTED;
        _portalRunning = false;
        return true;
    }
    else
    {
        Serial.println("Config portal started or connection failed/timed out.");
        // Check if the portal is actually running or if it just timed out connecting
        // Note: WiFiManager's autoConnect handles the logic of starting the portal.
        // If autoConnect returns false, it means it either failed to connect
        // and the portal *was* started (and might still be running or timed out),
        // or it failed to connect within the connectTimeout and never started the portal.

        // We infer state based on whether connection was ever established
        if (WiFi.status() == WL_CONNECTED)
        {
            // Should not happen if autoConnect returned false, but defensive check
            _currentState = WifiState::STA_CONNECTED;
            _portalRunning = false;
            return true;
        }
        else if (wifiManager.getConfigPortalActive())
        {
            // If autoConnect failed AND the portal is active (didn't time out yet)
            _currentState = WifiState::AP_MODE;
            _portalRunning = true;
            Serial.println("Config Portal is Active on AP: ");
            Serial.println(apName);
        }
        else
        {
            // autoConnect failed, portal is not active (timed out or connection failed before portal)
            _currentState = WifiState::CONNECTION_FAILED;
            _portalRunning = false;
            Serial.println("Failed to connect and Config Portal timed out or did not start.");
        }
        return false;
    }
}

void WifiMgr::loop()
{
    // WiFiManager doesn't require a loop function for basic autoConnect operation.
    // If you use startConfigPortal non-blocking, you might need wifiManager.process().
    // However, autoConnect is blocking until done/timeout.

    // If the portal was started and is still running (e.g., non-blocking portal)
    // you would handle its processing here.
    // if (_portalRunning) {
    //    _wm.process(); // Assuming _wm is the WiFiManager instance member
    // }

    // Optional: Check connection status and handle reconnections if desired,
    // although WiFiManager's autoConnect handles initial connection.
    if (_currentState == WifiState::STA_CONNECTED && WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi connection lost. Attempting reconnect...");
        _currentState = WifiState::CONNECTING;
        // Consider a reconnect strategy here, maybe retry begin() or WiFi.reconnect()
        // For simplicity, we just mark state, actual reconnect might need more logic
    }
    else if (_currentState == WifiState::CONNECTING && WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi reconnected!");
        _currentState = WifiState::STA_CONNECTED;
    }
}

WifiState WifiMgr::getState() const
{
    return _currentState;
}

void WifiMgr::resetSettings()
{
    Serial.println("Resetting WiFi settings...");
    WiFiManager wifiManager;
    wifiManager.resetSettings(); // Clears saved credentials from WiFiManager's perspective

    // Also clear any related settings you might store in Preferences
    Preferences preferences;
    if (preferences.begin(WIFI_NVS_NAMESPACE, false))
    {                        // Use read/write mode
        preferences.clear(); // Clear all keys in this namespace
        preferences.end();
        Serial.println("NVS WiFi settings cleared.");
    }
    else
    {
        Serial.println("Failed to open NVS for clearing WiFi settings.");
    }

    // Disconnect and potentially restart
    WiFi.disconnect(true); // true = erase SDK credentials
    _currentState = WifiState::NO_WIFI_CONFIGURED;
    Serial.println("WiFi settings reset. Restarting device is recommended.");
    // ESP.restart(); // Optional: Force restart
}

// Removed getIpAddress, loadCredentials, startPortal, tryConnect methods 
// as they were not declared in WifiMgr.h and might be redundant 
// with WiFiManager's autoConnect functionality.

// Removed dangling #endif