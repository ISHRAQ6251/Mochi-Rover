#include "camera_server.h"
#include "config.h"
#include "settings.h"
#include <esp_camera.h>

CameraServer cameraServer;

static const char STREAM_BOUNDARY[] = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";
static const size_t STREAM_HEADER_LEN = sizeof(STREAM_BOUNDARY) - 1;

// Incremental MJPEG packetizer used by the async chunked stream filler.
static class StreamPacketizer {
public:
    size_t fill(uint8_t* buf, size_t maxLen, size_t index) {
        if (index == 0) {
            reset();
        } else if (index != _written) {
            // server asked for a non-contiguous offset; resync
            reset();
            _written = index;
        }

        size_t written = 0;
        while (written < maxLen) {
            if (!_fb && !nextFrame()) break;
            if (_framePos < STREAM_HEADER_LEN) {
                buf[written++] = (uint8_t)STREAM_BOUNDARY[_framePos++];
            } else if (_framePos - STREAM_HEADER_LEN < _fb->len) {
                buf[written++] = _fb->buf[_framePos - STREAM_HEADER_LEN];
                _framePos++;
            } else {
                esp_camera_fb_return(_fb);
                _fb = nullptr;
            }
        }
        _written += written;
        return written;
    }

private:
    // Begin serving a new frame. May be invoked mid-fill when a frame ends
    // before the buffer is full; frame bytes are appended contiguously.
    bool nextFrame() {
        _fb = esp_camera_fb_get();
        if (!_fb) return false;
        _framePos = 0;
        return true;
    }

    void reset() {
        if (_fb) {
            esp_camera_fb_return(_fb);
            _fb = nullptr;
        }
        _written = 0;
        _framePos = 0;
    }

    camera_fb_t* _fb = nullptr;
    size_t _written = 0;
    size_t _framePos = 0;
} streamPacketizer;

static size_t streamFiller(uint8_t* buf, size_t maxLen, size_t index) {
    return streamPacketizer.fill(buf, maxLen, index);
}

bool CameraServer::begin() {
    camera_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    // The esp32-camera driver configures its XCLK clock through the native
    // IDF LEDC driver (invisible to the Arduino LEDC wrapper). Reserve a
    // dedicated channel/timer so it can never collide with the motor PWM,
    // which the Arduino API allocates starting at timer 0 / channels 1..4.
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
    cfg.fb_count = 2;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode = CAMERA_GRAB_LATEST;

    Serial.println("camera: init...");
    Serial.flush();
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("camera: init failed (0x%x)\n", (unsigned)err);
        return false;
    }
    _sensor = esp_camera_sensor_get();
    sensor_t* s = (sensor_t*)_sensor;
    if (s) {
        Serial.printf("camera: PID=0x%04x\n", s->id.PID);
    }
    applySettings();
    return true;
}

void CameraServer::applySettings() {
    sensor_t* s = (sensor_t*)_sensor;
    if (!s) return;
    s->set_framesize(s, (framesize_t)settings.data.camResolution);
    s->set_quality(s, settings.data.camQuality);
    bool flip = settings.data.camFlip;
    s->set_hmirror(s, flip ? 1 : 0);
    s->set_vflip(s, flip ? 1 : 0);
}

// Single JPEG snapshot (briefly bumps to UXGA for a higher-res photo).
// Served through a chunked filler that reads directly from the PSRAM frame
// buffer so we never copy a large JPEG onto the internal heap.
static camera_fb_t* s_snapFb = nullptr;

static size_t snapshotFiller(uint8_t* buf, size_t maxLen, size_t index) {
    if (!s_snapFb) return 0;
    if (index >= s_snapFb->len) {
        esp_camera_fb_return(s_snapFb);
        s_snapFb = nullptr;
        return 0;   // end of response
    }
    size_t chunk = min(maxLen, s_snapFb->len - index);
    memcpy(buf, s_snapFb->buf + index, chunk);
    return chunk;
}

static void setupCapture(AsyncWebServer& server) {
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest* request) {
        sensor_t* s = esp_camera_sensor_get();
        if (!s) {
            request->send(503, "text/plain", "camera unavailable");
            return;
        }
        if (s_snapFb) {
            request->send(503, "text/plain", "busy");
            return;
        }
        framesize_t prevSize = s->status.framesize;
        int prevQuality = s->status.quality;
        s->set_framesize(s, FRAMESIZE_UXGA);
        s->set_quality(s, 10);

        camera_fb_t* fb = nullptr;
        uint32_t start = millis();
        while (!fb && millis() - start < STREAM_TIMEOUT_MS) {
            fb = esp_camera_fb_get();
        }

        s->set_quality(s, prevQuality);
        s->set_framesize(s, prevSize);

        if (!fb) {
            request->send(503, "text/plain", "camera busy");
            return;
        }
        s_snapFb = fb;
        AsyncWebServerResponse* resp =
            request->beginChunkedResponse("image/jpeg", snapshotFiller);
        request->send(resp);
    });
}

void CameraServer::setupHandlers(AsyncWebServer& server) {
    // MJPEG live stream
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp =
            request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame",
                                          streamFiller);
        request->send(resp);
    });

    setupCapture(server);
}
