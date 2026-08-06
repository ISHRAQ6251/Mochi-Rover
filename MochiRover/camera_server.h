#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// OV2640 MJPEG streaming + snapshot over ESPAsyncWebServer.
class CameraServer {
public:
    // Init the camera with the current saved settings.
    bool begin();

    // Re-apply resolution/quality/fps from settings (no re-init needed).
    void applySettings();

    // Live camera sensor handle (nullptr if camera failed).
    void* sensor() { return _sensor; }

    // Register handlers on the given web server instance.
    void setupHandlers(AsyncWebServer& server);

private:
    void* _sensor = nullptr;
};

extern CameraServer cameraServer;
