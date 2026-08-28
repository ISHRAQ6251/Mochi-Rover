#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// OV5640 MJPEG streaming + snapshot over ESPAsyncWebServer.
class CameraServer {
public:
    // Init the camera with the current saved settings.
    bool begin();

    // Re-apply resolution/quality from settings (no re-init needed).
    void applySettings();

    // Register handlers on the given web server instance.
    void setupHandlers(AsyncWebServer& server);

private:
    void* _sensor = nullptr;
};

extern CameraServer cameraServer;
