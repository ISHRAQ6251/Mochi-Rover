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

        if (!_fb) {
            if (!nextFrame()) return 0;
        }

        size_t written = 0;
        while (written < maxLen && _fb) {
            if (_framePos < STREAM_HEADER_LEN) {
                buf[written++] = (uint8_t)STREAM_BOUNDARY[_framePos++];
            } else if (_framePos - STREAM_HEADER_LEN < _fb->len) {
                buf[written++] = _fb->buf[_framePos - STREAM_HEADER_LEN];
                _framePos++;
            } else {
                esp_camera_fb_return(_fb);
                _fb = nullptr;
                if (written < maxLen) {
                    if (!nextFrame()) break;
                }
            }
        }
        _written += written;
        return written;
    }

private:
    bool nextFrame() {
        _fb = esp_camera_fb_get();
        if (!_fb) return false;
        _frameStart = _written;
        _frameLen = STREAM_HEADER_LEN + _fb->len;
        _framePos = 0;
        return true;
    }

    void reset() {
        if (_fb) {
            esp_camera_fb_return(_fb);
            _fb = nullptr;
        }
        _written = 0;
        _frameStart = 0;
        _frameLen = 0;
        _framePos = 0;
    }

    camera_fb_t* _fb = nullptr;
    size_t _written = 0;
    size_t _frameStart = 0;
    size_t _frameLen = 0;
    size_t _framePos = 0;
} streamPacketizer;

static size_t streamFiller(uint8_t* buf, size_t maxLen, size_t index) {
    return streamPacketizer.fill(buf, maxLen, index);
}

bool CameraServer::begin() {
    camera_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer = LEDC_TIMER_0;
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
    cfg.pin_pwdn = CAM_PIN_PWDN;
    cfg.pin_reset = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = (framesize_t)settings.data.camResolution;
    cfg.jpeg_quality = settings.data.camQuality;
    cfg.fb_count = 1;
    cfg.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        return false;
    }
    _sensor = esp_camera_sensor_get();
    applySettings();
    return true;
}

void CameraServer::applySettings() {
    sensor_t* s = (sensor_t*)_sensor;
    if (!s) return;
    s->set_framesize(s, (framesize_t)settings.data.camResolution);
    s->set_quality(s, settings.data.camQuality);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
}

void CameraServer::setupHandlers(AsyncWebServer& server) {
    // MJPEG live stream
    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp =
            request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame",
                                          streamFiller);
        request->send(resp);
    });

    // Single JPEG snapshot
    server.on("/capture", HTTP_GET, [](AsyncWebServerRequest* request) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            request->send(503, "text/plain", "camera busy");
            return;
        }
        AsyncWebServerResponse* resp =
            request->beginResponse(200, "image/jpeg",
                                   String((const char*)fb->buf, fb->len));
        esp_camera_fb_return(fb);
        request->send(resp);
    });
}
