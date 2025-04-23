#ifndef OTASERVICE_H
#define OTASERVICE_H

// Forward declaration
class AsyncWebServer;

class OTAService {
public:
    OTAService();
    void begin(AsyncWebServer *server);
};

#endif // OTASERVICE_H 