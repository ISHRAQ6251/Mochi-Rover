#include "web_server.h"
#include "config.h"
#include "settings.h"
#include "motor_control.h"
#include "hero_eyes.h"
#include "display_manager.h"
#include "wifi_helper.h"
#include "camera_server.h"
#include "web_assets.h"

WebServerMgr webServerMgr;

static bool authorized(AsyncWebServerRequest* request) {
    const String& h = request->header("X-Auth-Token");
    if (h.length() > 0 && h == settings.data.authToken) return true;
    if (request->hasParam("token")) {
        return request->getParam("token")->value() == settings.data.authToken;
    }
    return false;
}

static void unauthorized(AsyncWebServerRequest* request) {
    request->send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
}

static int queryInt(AsyncWebServerRequest* request, const char* name, int fallback) {
    if (request->hasParam(name, true)) return request->getParam(name, true)->value().toInt();
    if (request->hasParam(name)) return request->getParam(name)->value().toInt();
    return fallback;
}

static HeroMood parseMood(const String& s) {
    if (s == "happy") return HeroMood::HAPPY;
    if (s == "angry") return HeroMood::ANGRY;
    if (s == "curious") return HeroMood::CURIOUS;
    if (s == "dead") return HeroMood::DEAD;
    if (s == "sleepy") return HeroMood::SLEEPY;
    if (s == "wink") return HeroMood::WINK;
    return HeroMood::IDLE;
}

// Escapes s into dst (quotes/backslashes preserved, control chars -> space).
// Bounded write, no heap: /api/state is polled by every client on a timer.
static void jsonEscape(char* dst, size_t len, const char* s) {
    if (!dst || len == 0) return;
    size_t o = 0;
    for (const char* p = s; p && *p; p++) {
        char c = *p;
        if (c == '"' || c == '\\') {
            if (o + 2 >= len) break;
            dst[o++] = '\\';
        } else if (c < 0x20) {
            c = ' ';
        }
        if (o + 1 >= len) break;
        dst[o++] = c;
    }
    dst[o] = 0;
}

static String stateJson() {
    char ip[16];
    wifiHelper.ip(ip, sizeof(ip));
    char message[140];
    jsonEscape(message, sizeof(message), displayMgr.message());
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"throttle\":%d,\"steering\":%d,\"mood\":\"%s\","
             "\"sleeping\":%s,\"flashlightOn\":%s,\"oledAnim\":%s,"
             "\"camResolution\":%d,\"camQuality\":%d,\"camFlip\":%s,\"camRunning\":%s,"
             "\"reverseLeft\":%s,\"reverseRight\":%s,\"trimLeft\":%u,\"trimRight\":%u,"
             "\"connected\":true,\"apMode\":true,\"ip\":\"%s\","
             "\"messageActive\":%s,\"message\":\"%s\"}",
             (int)motors.throttle(), (int)motors.steering(), eyes.moodName(),
             displayMgr.sleeping() ? "true" : "false",
             settings.data.flashlightOn ? "true" : "false",
             settings.data.oledAnim ? "true" : "false",
             settings.data.camResolution, settings.data.camQuality,
             settings.data.camFlip ? "true" : "false",
             cameraServer.wanted() ? "true" : "false",
             settings.data.reverseLeft ? "true" : "false",
             settings.data.reverseRight ? "true" : "false",
             (unsigned)settings.data.trimLeft, (unsigned)settings.data.trimRight,
             ip,
             displayMgr.messageActive() ? "true" : "false",
             message);
    return String(buf);
}

// Serial-free bring-up test: GET /api/pintest?token=hero&pin=1&speed=200
// drives one motor input (0=L_IN1/GPIO47, 1=L_IN2/GPIO14, 2=R_IN3/GPIO21,
// 3=R_IN4/GPIO42) for MOTOR_TEST_MS. `duty` in the reply is the PWM value read
// back from the pin: a non-zero value means the pin is attached and driven, so
// a wheel that still does not turn points at wiring/DRV8833 rather than code.
static void handlePinTest(AsyncWebServerRequest* request) {
    if (!authorized(request)) return unauthorized(request);
    int index = queryInt(request, "pin", -1);
    int value = queryInt(request, "speed", 200);
    if (index < 0 || index > 3) {
        request->send(400, "application/json", "{\"ok\":false,\"error\":\"pin must be 0..3\"}");
        return;
    }
    uint32_t duty = motors.testPin((uint8_t)index, (int16_t)value);
    String s = "{\"ok\":true,\"pin\":" + String(index) + ",\"duty\":" + String(duty) + "}";
    request->send(200, "application/json", s);
}

void WebServerMgr::begin() {
    pinMode(PIN_FLASH_LED, OUTPUT);
    digitalWrite(PIN_FLASH_LED, settings.data.flashlightOn ? HIGH : LOW);

    // ---------- static assets ----------
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });
    _server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/css; charset=utf-8", STYLE_CSS);
    });
    _server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "application/javascript; charset=utf-8", APP_JS);
    });

    // ---------- camera ----------
    cameraServer.setupHandlers(_server);

    // ---------- open endpoints ----------
    _server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        char ip[16];
        wifiHelper.ip(ip, sizeof(ip));
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":true,\"apMode\":true,\"connected\":true,"
                 "\"ip\":\"%s\",\"hostname\":\"" HOSTNAME "\"}",
                 ip);
        request->send(200, "application/json", buf);
    });

    _server.on("/api/auth", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("token", true) &&
            request->getParam("token", true)->value() == settings.data.authToken) {
            request->send(200, "application/json", "{\"ok\":true}");
        } else {
            request->send(401, "application/json", "{\"ok\":false}");
        }
    });

    // ---------- protected endpoints ----------
    _server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        request->send(200, "application/json", stateJson());
    });

    _server.on("/api/drive", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        int16_t throttle = request->hasParam("throttle", true)
                               ? constrain(request->getParam("throttle", true)->value().toInt(), -255, 255)
                               : 0;
        int16_t steering = request->hasParam("steering", true)
                               ? constrain(request->getParam("steering", true)->value().toInt(), -255, 255)
                               : 0;
        motors.drive(throttle, steering);
        eyes.setDriving(throttle, steering);
        displayMgr.notifyActivity();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        motors.stop();
        eyes.setDriving(0, 0);
        displayMgr.notifyActivity();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // Bring-up motor test, invokable from a phone browser (serial unusable on
    // battery): /api/pintest?token=hero&pin=1&speed=200
    _server.on("/api/pintest", HTTP_GET, handlePinTest);
    _server.on("/api/pintest", HTTP_POST, handlePinTest);

    _server.on("/api/mood", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        String m = request->hasParam("mood", true) ? request->getParam("mood", true)->value() : "idle";
        displayMgr.setMood(parseMood(m));
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/message", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        // empty text == clear the persistent message box
        if (request->hasParam("text", true) && request->getParam("text", true)->value().length() > 0) {
            String text = request->getParam("text", true)->value();
            if (text.length() > 63) text = text.substring(0, 63);
            displayMgr.showMessage(text.c_str());
        } else {
            displayMgr.clearMessage();
        }
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/flash", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        bool on = request->hasParam("on", true) && request->getParam("on", true)->value() == "1";
        settings.setFlashlight(on);
        digitalWrite(PIN_FLASH_LED, on ? HIGH : LOW);
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        if (request->hasParam("oledAnim", true)) {
            settings.setOledAnim(request->getParam("oledAnim", true)->value() == "1");
            eyes.setAnimEnabled(settings.data.oledAnim);
        }
        if (request->hasParam("reverseLeft", true) || request->hasParam("reverseRight", true) ||
            request->hasParam("trimLeft", true) || request->hasParam("trimRight", true)) {
            bool left = request->hasParam("reverseLeft", true)
                ? request->getParam("reverseLeft", true)->value() == "1"
                : settings.data.reverseLeft;
            bool right = request->hasParam("reverseRight", true)
                ? request->getParam("reverseRight", true)->value() == "1"
                : settings.data.reverseRight;
            uint8_t trimL = request->hasParam("trimLeft", true)
                ? (uint8_t)constrain(request->getParam("trimLeft", true)->value().toInt(),
                                     MOTOR_TRIM_MIN, MOTOR_TRIM_MAX)
                : settings.data.trimLeft;
            uint8_t trimR = request->hasParam("trimRight", true)
                ? (uint8_t)constrain(request->getParam("trimRight", true)->value().toInt(),
                                     MOTOR_TRIM_MIN, MOTOR_TRIM_MAX)
                : settings.data.trimRight;
            settings.setMotorCal(left, right, trimL, trimR);
            motors.reapply();
        }
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/camera", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        if (request->hasParam("on", true)) {
            bool on = request->getParam("on", true)->value() == "1";
            if (on) cameraServer.start();
            else cameraServer.stop();
            request->send(200, "application/json",
                          cameraServer.wanted()
                              ? "{\"ok\":true,\"camRunning\":true}"
                              : "{\"ok\":true,\"camRunning\":false}");
            return;
        }
        if (request->hasParam("flip", true)) {
            settings.setCamFlip(request->getParam("flip", true)->value() == "1");
            cameraServer.applySettings();
            request->send(200, "application/json", "{\"ok\":true}");
            return;
        }
        request->send(400, "application/json", "{\"ok\":false,\"error\":\"bad param\"}");
    });

    _server.on("/api/token", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
        String t = request->hasParam("token", true) ? request->getParam("token", true)->value() : "";
        if (t.length() >= 4) {
            settings.setToken(t.c_str());
            request->send(200, "application/json", "{\"ok\":true}");
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"token too short\"}");
        }
    });

    _server.begin();
}
