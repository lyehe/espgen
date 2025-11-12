#include "WebFacade.h"
#include <Arduino.h> // For Serial prints
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h> // For mDNS
#include "OTAService.h" // Include OTAService for integration

// Constructor implementation - Clean Architecture wiring
// This is the Composition Root where dependency injection happens
WebFacade::WebFacade(SignalEngine& engine) :
    _engine(engine),               // Domain layer reference
    _adapter(engine),              // Create adapter (Application layer -> Domain layer)
    _server(80),                   // Initialize the server
    _wifiMgr(),                    // Initialize WifiMgr
    _apiRouter(_adapter, _server), // Inject adapter into ApiRouter (Dependency Inversion!)
    _wsHub(_server),               // Initialize WebSocketHub, passing the server
    _otaService()                  // Initialize OTAService
{
    // Constructor body (if needed)
    // Note: ApiRouter now depends on ISignalController interface, not concrete SignalEngine
    // This enables testing with mock controllers and follows Clean Architecture
}

// Initialize LittleFS
bool WebFacade::initLittleFS()
{
    if (!LittleFS.begin())
    {
        Serial.println("ERROR: Failed to mount LittleFS");
        Serial.println("Attempting to format LittleFS...");

        // Attempt to format the filesystem
        if (!LittleFS.format()) {
            Serial.println("ERROR: LittleFS format failed!");
            return false;
        }

        Serial.println("LittleFS formatted successfully");

        // Try mounting again after format
        if (!LittleFS.begin()) {
            Serial.println("ERROR: Failed to mount LittleFS after format");
            return false;
        }

        Serial.println("LittleFS mounted successfully after format");
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

// Begin WebFacade operation - returns false on critical failure
bool WebFacade::begin()
{
    Serial.println("Initializing WebFacade...");

    if (!initLittleFS())
    {
        Serial.println("CRITICAL: WebFacade initialization FAILED - LittleFS mount failed");
        return false; // Don't proceed if FS fails
    }

    // Start WiFi Manager (handles connection/AP mode)
    _wifiMgr.begin(); // Assumes WifiMgr handles its own state reporting

    // --- Configure Web Server Routes & Handlers ---

    // !! IMPORTANT: Register generic OPTIONS handler for API routes BEFORE specific API routes !!
    // This ensures CORS preflight requests are handled correctly for all API endpoints.
    _server.on("^\\/api\\/.*$", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
        Serial.printf("Received OPTIONS %s\n", request->url().c_str());
        // Send necessary CORS headers for preflight
        AsyncWebServerResponse *response = request->beginResponse(204); // No Content
        response->addHeader("Access-Control-Allow-Origin", "*"); // Allow all origins
        response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"); // Allow common methods
        response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With"); // Allow typical headers
        request->send(response);
    });
    Serial.println("Generic API OPTIONS handler registered.");

    // Register API routes (POST, GET etc. for specific endpoints)
    _apiRouter.registerRoutes();
    Serial.println("API routes registered.");

    // Initialize WebSocket Hub (adds /ws handler)
    if (!_wsHub.begin()) {
        Serial.println("WebFacade: ERROR - WebSocket hub initialization failed!");
        Serial.println("WebFacade: WebSocket updates will not work, but continuing...");
        // Continue anyway - web interface will work, just no real-time updates
    }

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

    // Add CORS headers globally (these apply to non-OPTIONS responses, e.g., the actual GET/POST response)
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    // Note: DefaultHeaders for Allow-Methods/Headers might be less critical now that OPTIONS is handled explicitly,
    // but they don't hurt and can help if a 'simple' cross-origin request occurs (rare for APIs).
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");

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

    Serial.println("WebFacade initialization complete");
    return true; // Successful initialization
}
