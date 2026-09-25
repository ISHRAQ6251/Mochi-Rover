#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// OV5640 MJPEG streaming + snapshot over ESPAsyncWebServer.
class CameraServer {
public:
    // Init the camera with the current saved settings.
    bool begin();

    // Request start after stop(). Heavy init runs in update().
    void start();

    // Request stop; de-init runs in update() once buffers are free.
    void stop();

    // Pump start/stop so init/deinit never run on the async web task.
    void update();

    bool running() const { return _running; }
    bool wanted() const { return _wantRunning; }

    // Re-apply resolution/quality from settings (no re-init needed).
    void applySettings();

    // Register handlers on the given web server instance.
    void setupHandlers(AsyncWebServer& server);

    void noteStreamOpen();
    void noteStreamClose();

private:
    void maybeDeinit();
    void deinitNow();

    void* _sensor = nullptr;
    bool _running = false;
    bool _inited = false;
    bool _wantRunning = false;
    int _streamClients = 0;
};

extern CameraServer cameraServer;
