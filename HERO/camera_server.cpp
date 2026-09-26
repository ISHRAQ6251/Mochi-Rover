#include "camera_server.h"
#include "config.h"
#include "settings.h"
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <memory>

CameraServer cameraServer;

static uint8_t* s_snapBuf = nullptr;
static size_t s_snapLen = 0;
static size_t s_snapCap = 0;
static bool s_snapBusy = false;

static void releaseSnapLocked() {
    s_snapLen = 0;
    s_snapBusy = false;
}

void CameraServer::ensureMux() {
    if (!_mux) _mux = xSemaphoreCreateRecursiveMutex();
}

void CameraServer::lock() {
    ensureMux();
    if (_mux) xSemaphoreTakeRecursive(_mux, portMAX_DELAY);
}

void CameraServer::unlock() {
    if (_mux) xSemaphoreGiveRecursive(_mux);
}

static const char STREAM_BOUNDARY[] = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";
static const size_t STREAM_HEADER_LEN = sizeof(STREAM_BOUNDARY) - 1;

static const char STREAM_TRAILER[] = "\r\n";
static const size_t STREAM_TRAILER_LEN = sizeof(STREAM_TRAILER) - 1;

// Incremental MJPEG packetizer. One instance per connected stream client.
// JPEG bytes are memcpy'd out of the camera frame buffer under the camera
// mutex, then the fb is returned immediately so deinit and other clients
// never race a live DMA buffer. fill() copies header/payload/trailer in
// memcpy batches, not one byte at a time.
class StreamPacketizer {
public:
    ~StreamPacketizer() { reset(true); }

    size_t fill(uint8_t* buf, size_t maxLen, size_t index) {
        if (!cameraServer.wanted()) {
            reset(false);
            return 0;
        }
        if (index == 0) {
            reset(false);
        } else if (index != _written) {
            reset(false);
            _written = index;
        }

        size_t written = 0;
        while (written < maxLen) {
            if (!cameraServer.wanted()) {
                reset(false);
                break;
            }
            if (!_jpegLen && !nextFrame()) {
                if (written == 0) buf[written++] = '\n';
                break;
            }
            const size_t trailerStart = STREAM_HEADER_LEN + _jpegLen;
            const size_t frameLen = trailerStart + STREAM_TRAILER_LEN;
            size_t room = maxLen - written;
            if (_framePos < STREAM_HEADER_LEN) {
                size_t n = min(room, STREAM_HEADER_LEN - _framePos);
                memcpy(buf + written, STREAM_BOUNDARY + _framePos, n);
                _framePos += n;
                written += n;
            } else if (_framePos < trailerStart) {
                size_t off = _framePos - STREAM_HEADER_LEN;
                size_t n = min(room, _jpegLen - off);
                memcpy(buf + written, _jpeg + off, n);
                _framePos += n;
                written += n;
            } else if (_framePos < frameLen) {
                size_t off = _framePos - trailerStart;
                size_t n = min(room, STREAM_TRAILER_LEN - off);
                memcpy(buf + written, STREAM_TRAILER + off, n);
                _framePos += n;
                written += n;
            } else {
                _jpegLen = 0;
                _framePos = 0;
            }
        }
        _written += written;
        return written;
    }

    void reset(bool freeBuf) {
        _written = 0;
        _framePos = 0;
        _jpegLen = 0;
        if (freeBuf && _jpeg) {
            heap_caps_free(_jpeg);
            _jpeg = nullptr;
            _jpegCap = 0;
        }
    }

private:
    bool nextFrame() {
        return cameraServer.copyJpeg(&_jpeg, &_jpegLen, &_jpegCap);
    }

    uint8_t* _jpeg = nullptr;
    size_t _jpegLen = 0;
    size_t _jpegCap = 0;
    size_t _written = 0;
    size_t _framePos = 0;
};

bool CameraServer::copyJpeg(uint8_t** buf, size_t* len, size_t* cap) {
    if (!buf || !len || !cap) return false;
    for (int attempt = 0; attempt < 3; attempt++) {
        lock();
        if (!_running.load() || !_inited) {
            unlock();
            return false;
        }
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb || !fb->buf || fb->len == 0) {
            if (fb) esp_camera_fb_return(fb);
            unlock();
            return false;
        }
        const size_t n = fb->len;
        if (*cap >= n) {
            memcpy(*buf, fb->buf, n);
            *len = n;
            esp_camera_fb_return(fb);
            unlock();
            return true;
        }
        esp_camera_fb_return(fb);
        unlock();

        size_t need = n + 4096;
        uint8_t* p = (uint8_t*)heap_caps_realloc(
            *buf, need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) {
            p = (uint8_t*)heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (p && *buf) heap_caps_free(*buf);
        }
        if (!p) return false;
        *buf = p;
        *cap = need;
    }
    return false;
}

bool CameraServer::begin() {
    camera_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    // The esp32-camera driver configures its XCLK clock through the native
    // IDF LEDC driver (invisible to the Arduino LEDC wrapper). Reserve a
    // dedicated channel/timer so it can never collide with the motor PWM,
    // which the Arduino API allocates on channels 0..3 (timers 0..1).
    cfg.ledc_channel = LEDC_CHANNEL_5;
    cfg.ledc_timer = LEDC_TIMER_2;
    cfg.pin_d0 = CAM_PIN_D0;
    cfg.pin_d1 = CAM_PIN_D1;
    cfg.pin_d2 = CAM_PIN_D2;
    cfg.pin_d3 = CAM_PIN_D3;
    cfg.pin_d4 = CAM_PIN_D4;
    cfg.pin_d5 = CAM_PIN_D5;
    cfg.pin_d6 = CAM_PIN_D6;
    cfg.pin_d7 = CAM_PIN_D7;
    cfg.pin_xclk = CAM_PIN_XCLK;
    cfg.pin_pclk = CAM_PIN_PCLK;
    cfg.pin_vsync = CAM_PIN_VSYNC;
    cfg.pin_href = CAM_PIN_HREF;
    cfg.pin_sccb_sda = CAM_PIN_SIOD;
    cfg.pin_sccb_scl = CAM_PIN_SIOC;
    cfg.sccb_i2c_port = 1;
    cfg.pin_pwdn = CAM_PIN_PWDN;
    cfg.pin_reset = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = (framesize_t)settings.data.camResolution;
    cfg.jpeg_quality = settings.data.camQuality;
    // Three PSRAM buffers: two streamers or stream+capture can hold a frame
    // while GRAB_LATEST still has a spare to overwrite.
    cfg.fb_count = 3;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode = CAMERA_GRAB_LATEST;

    lock();
    if (_inited) {
        bool on = _wantRunning.load();
        _running.store(on);
        unlock();
        if (on) applySettings();
        return true;
    }
    unlock();

    Serial.println("camera: init...");
    Serial.flush();
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("camera: init failed (0x%x)\n", (unsigned)err);
        lock();
        _sensor = nullptr;
        _running.store(false);
        _inited = false;
        unlock();
        return false;
    }
    lock();
    _sensor = esp_camera_sensor_get();
    sensor_t* s = (sensor_t*)_sensor;
    if (s) {
        Serial.printf("camera: PID=0x%04x\n", s->id.PID);
    }
    _inited = true;
    _running.store(_wantRunning.load());
    unlock();
    if (_running.load()) applySettings();
    return true;
}

void CameraServer::start() {
    lock();
    _wantRunning.store(true);
    if (_inited) _running.store(true);
    unlock();
}

void CameraServer::stop() {
    lock();
    _wantRunning.store(false);
    _running.store(false);
    unlock();
}

void CameraServer::update() {
    ensureMux();
    if (_wantRunning.load() && !_inited) {
        if (!begin()) _wantRunning.store(false);
    } else if (!_wantRunning.load()) {
        maybeDeinit();
    }
}

void CameraServer::maybeDeinit() {
    if (_wantRunning.load() || _running.load()) return;
    if (_streamClients.load() > 0) return;
    lock();
    if (_wantRunning.load() || _running.load() || _streamClients.load() > 0 || s_snapBusy) {
        unlock();
        return;
    }
    deinitNow();
    unlock();
}

void CameraServer::deinitNow() {
    if (!_inited) return;
    releaseSnapLocked();
    _sensor = nullptr;
    esp_camera_deinit();
    _inited = false;
    _running.store(false);
    Serial.println("camera: stopped");
}

void CameraServer::noteStreamOpen() {
    _streamClients.fetch_add(1);
}

void CameraServer::noteStreamClose() {
    int n = _streamClients.load();
    while (n > 0 && !_streamClients.compare_exchange_weak(n, n - 1)) {}
}

void CameraServer::applySettings() {
    lock();
    if (!_inited || !_running.load()) {
        unlock();
        return;
    }
    sensor_t* s = (sensor_t*)_sensor;
    if (!s) {
        unlock();
        return;
    }
    s->set_framesize(s, (framesize_t)settings.data.camResolution);
    s->set_quality(s, settings.data.camQuality);
    bool flip = settings.data.camFlip;
    s->set_hmirror(s, flip ? 1 : 0);
    s->set_vflip(s, flip ? 1 : 0);
    unlock();
}

static void releaseSnap() {
    cameraServer.lock();
    releaseSnapLocked();
    cameraServer.unlock();
}

static size_t snapshotFiller(uint8_t* buf, size_t maxLen, size_t index) {
    cameraServer.lock();
    if (!s_snapBusy || !s_snapBuf || s_snapLen == 0 || index >= s_snapLen) {
        releaseSnapLocked();
        cameraServer.unlock();
        return 0;
    }
    size_t chunk = min(maxLen, s_snapLen - index);
    memcpy(buf, s_snapBuf + index, chunk);
    cameraServer.unlock();
    return chunk;
}

static void setupCapture(AsyncWebServer& server) {
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!cameraServer.running()) {
            request->send(503, "text/plain", "camera stopped");
            return;
        }
        cameraServer.lock();
        if (!cameraServer.running()) {
            cameraServer.unlock();
            request->send(503, "text/plain", "camera stopped");
            return;
        }
        if (s_snapBusy) {
            cameraServer.unlock();
            request->send(503, "text/plain", "busy");
            return;
        }
        s_snapBusy = true;
        cameraServer.unlock();
        if (!cameraServer.copyJpeg(&s_snapBuf, &s_snapLen, &s_snapCap)) {
            cameraServer.lock();
            releaseSnapLocked();
            cameraServer.unlock();
            request->send(503, "text/plain", "camera busy");
            return;
        }
        request->onDisconnect(releaseSnap);
        AsyncWebServerResponse* resp =
            request->beginChunkedResponse("image/jpeg", snapshotFiller);
        resp->addHeader("Content-Disposition", "attachment; filename=\"hero.jpg\"");
        resp->addHeader("Cache-Control", "no-store");
        request->send(resp);
    });
}

void CameraServer::setupHandlers(AsyncWebServer& server) {
    ensureMux();
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!cameraServer.wanted()) {
            request->send(503, "text/plain", "camera stopped");
            return;
        }
        auto pkt = std::make_shared<StreamPacketizer>();
        cameraServer.noteStreamOpen();
        request->onDisconnect([pkt]() {
            pkt->reset(true);
            cameraServer.noteStreamClose();
        });
        AsyncWebServerResponse* resp = request->beginChunkedResponse(
            "multipart/x-mixed-replace; boundary=frame",
            [pkt](uint8_t* buf, size_t maxLen, size_t index) {
                return pkt->fill(buf, maxLen, index);
            });
        resp->addHeader("Cache-Control", "no-store");
        request->send(resp);
    });

    setupCapture(server);
}
