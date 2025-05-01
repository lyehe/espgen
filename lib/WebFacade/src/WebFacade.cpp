#include "WebFacade.h"
#include <Arduino.h> // For Serial prints
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h> // For mDNS
#include "OTAService.h" // Include OTAService for integration

// Constructor implementation - updated
WebFacade::WebFacade(SignalEngine& engine) :
    _engine(engine),          // Initialize the engine reference
    _server(80),              // Initialize the server
    _wifiMgr(),               // Initialize WifiMgr
    _apiRouter(_engine, _server), // Initialize ApiRouter, passing engine and server
    _wsHub(_server), // Initialize WebSocketHub, passing the server
    _otaService() // Initialize OTAService
{
    // Constructor body (if needed)
}

// Initialize LittleFS
bool WebFacade::initLittleFS()
{
    if (!LittleFS.begin())
    {
        Serial.println("ERROR: Failed to mount LittleFS");
        // TODO: Handle filesystem formatting or error indication
        return false;
    }
    Serial.println("LittleFS mounted successfully. Contents:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while(file){
        Serial.printf("  FILE: %s, SIZE: %d bytes\n", file.name(), file.size());
        // Serial.print("  FILE: ");
        // Serial.println(file.name());
        file = root.openNextFile();
    }
    Serial.println("-----------------------------");
    return true;
}

// Handle Root Request
void WebFacade::handleRoot(AsyncWebServerRequest *request)
{
    Serial.println("Serving /index.html");
    request->send(LittleFS, "/index.html", "text/html");
}

// Handle Not Found
void WebFacade::handleNotFound(AsyncWebServerRequest *request)
{
    Serial.printf("NOT FOUND: http://%s%s\n", request->host().c_str(), request->url().c_str());
    request->send(404, "text/plain", "Not found");
    }

// Begin WebFacade operation - updated
void WebFacade::begin()
{
    Serial.println("Initializing WebFacade...");

    if (!initLittleFS())
    {
        Serial.println("Halting WebFacade due to LittleFS failure.");
        return; // Don't proceed if FS fails
    }

    // Start WiFi Manager (handles connection/AP mode)
    _wifiMgr.begin(); // Assumes WifiMgr handles its own state reporting

    // --- Configure Web Server Routes & Handlers ---

    // Register API routes
    _apiRouter.registerRoutes();
    Serial.println("API routes registered.");

    // Initialize WebSocket Hub (adds /ws handler)
    _wsHub.begin(); // This call now exists

    // --- Register standard handlers ---

    // Root serves index.html
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        this->handleRoot(request); // handleRoot already sends index.html
    });

    // Serve static files from specific directories in LittleFS
    _server.serveStatic("/css", LittleFS, "/css");
    _server.serveStatic("/js", LittleFS, "/js");

    // Set up Not Found handler (LAST)
    _server.onNotFound([this](AsyncWebServerRequest *request){
        // Check if it looks like an API call that wasn't handled
        if (request->url().startsWith("/api/")) {
             request->send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
        } else {
           // Fallback to standard 404 or serve index.html for SPAs
           // this->handleNotFound(request);
           // For Single Page Apps, often good to serve index.html on 404s that aren't API/static files
            Serial.printf("Not Found (non-API), serving index.html: %s\n", request->url().c_str());
            request->send(LittleFS, "/index.html", "text/html");
        }
    });

    // Add CORS headers globally
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");

    // Handle OPTIONS requests for CORS preflight explicitly for API routes
    _server.on("^\\/api\\/.*$", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
        // Send necessary CORS headers for preflight
        AsyncWebServerResponse *response = request->beginResponse(204); // No Content
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        request->send(response);
    });

    // Start the server
    _server.begin();
    Serial.println("AsyncWebServer started.");

    // Start OTA Service (needs server to be running)
    _otaService.begin(&_server);

    // Initialize mDNS (optional but recommended)
    if (WiFi.getMode() == WIFI_STA)
    { // Only run mDNS in STA mode
        if (MDNS.begin("trigger"))
        { // Hostname 'trigger.local'
            MDNS.addService("http", "tcp", 80);
            Serial.println("MDNS responder started: http://trigger.local");
        }
        else
        {
            Serial.println("Error starting MDNS");
        }
    }
}
