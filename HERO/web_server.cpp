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

static HeroMood parseMood(const String& s) {
    if (s == "happy") return HeroMood::HAPPY;
    if (s == "angry") return HeroMood::ANGRY;
    if (s == "curious") return HeroMood::CURIOUS;
    if (s == "dead") return HeroMood::DEAD;
    if (s == "sleepy") return HeroMood::SLEEPY;
    if (s == "wink") return HeroMood::WINK;
    return HeroMood::IDLE;
}

static String jsonEscape(const char* s) {
    String out;
    for (const char* p = s; p && *p; p++) {
        if (*p == '"' || *p == '\\') {
            out += '\\';
            out += *p;
        } else if (*p < 0x20) {
            out += ' ';
        } else {
            out += *p;
        }
    }
    return out;
}

static String stateJson() {
    String s = "{";
    s += "\"ok\":true,";
    s += "\"throttle\":" + String(motors.throttle()) + ",";
    s += "\"steering\":" + String(motors.steering()) + ",";
    s += "\"mood\":\"" + String(eyes.moodName()) + "\",";
    s += "\"sleeping\":" + String(displayMgr.sleeping() ? "true" : "false") + ",";
    s += "\"flashlightOn\":" + String(settings.data.flashlightOn ? "true" : "false") + ",";
    s += "\"oledAnim\":" + String(settings.data.oledAnim ? "true" : "false") + ",";
    s += "\"camResolution\":" + String(settings.data.camResolution) + ",";
    s += "\"camQuality\":" + String(settings.data.camQuality) + ",";
    s += "\"camFlip\":" + String(settings.data.camFlip ? "true" : "false") + ",";
    s += "\"reverseLeft\":" + String(settings.data.reverseLeft ? "true" : "false") + ",";
    s += "\"reverseRight\":" + String(settings.data.reverseRight ? "true" : "false") + ",";
    s += "\"connected\":true,";
    s += "\"apMode\":true,";
    s += "\"ip\":\"" + wifiHelper.ip() + "\",";
    s += "\"messageActive\":" + String(displayMgr.messageActive() ? "true" : "false") + ",";
    s += "\"message\":\"" + jsonEscape(displayMgr.message()) + "\"";
    s += "}";
    return s;
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
        String s = "{";
        s += "\"ok\":true,";
        s += "\"apMode\":true,";
        s += "\"connected\":true,";
        s += "\"ip\":\"" + wifiHelper.ip() + "\",";
        s += "\"hostname\":\"" HOSTNAME "\"";
        s += "}";
        request->send(200, "application/json", s);
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
        if (request->hasParam("reverseLeft", true) || request->hasParam("reverseRight", true)) {
            bool left = request->hasParam("reverseLeft", true)
                ? request->getParam("reverseLeft", true)->value() == "1"
                : settings.data.reverseLeft;
            bool right = request->hasParam("reverseRight", true)
                ? request->getParam("reverseRight", true)->value() == "1"
                : settings.data.reverseRight;
            settings.setMotorReverse(left, right);
        }
        request->send(200, "application/json", "{\"ok\":true}");
    });

    _server.on("/api/camera", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!authorized(request)) return unauthorized(request);
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
