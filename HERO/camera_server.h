#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <atomic>

// OV5640 MJPEG streaming + snapshot over ESPAsyncWebServer.
class CameraServer {
public:
    // Init the camera with the current saved settings. Does not change wanted().
    bool begin();

    // Request start after stop(). Heavy init runs in update().
    void start();

    // Request stop; de-init runs in update() once buffers are free.
    void stop();

    // Pump start/stop so init/deinit never run on the async web task.
    void update();

    bool running() const { return _running.load(); }
    bool wanted() const { return _wantRunning.load(); }

    // Re-apply resolution/quality from settings (no re-init needed).
    void applySettings();

    // Register handlers on the given web server instance.
    void setupHandlers(AsyncWebServer& server);

    void noteStreamOpen();
    void noteStreamClose();

    // Copy one JPEG into *buf (PSRAM, grown as needed) and return the camera
    // frame buffer immediately. Serialized with init/deinit/snapshot.
    bool copyJpeg(uint8_t** buf, size_t* len, size_t* cap);

    void lock();
    void unlock();

private:
    void ensureMux();
    void maybeDeinit();
    void deinitNow();

    void* _sensor = nullptr;
    std::atomic<bool> _running{false};
    bool _inited = false;
    std::atomic<bool> _wantRunning{false};
    std::atomic<int> _streamClients{0};
    SemaphoreHandle_t _mux = nullptr;
};

extern CameraServer cameraServer;
