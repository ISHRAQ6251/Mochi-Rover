#include "display_manager.h"
#include "config.h"
#include "settings.h"

DisplayManager displayMgr;

bool DisplayManager::begin() {
    _bootStart = millis();
    _lastActivity = millis();
    _present = false;

    // OLED uses Arduino Wire = I2C port 0. The OV5640 SCCB driver (sccb-ng)
    // installs its own master on I2C port 1. Sharing Wire1 (port 1) with the
    // camera produced "I2C bus id(1) has already been acquired" / camera
    // probe 0x103 and a blank display.
    Wire.setPins(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.begin();
    Wire.setTimeOut(50);

    uint8_t addr = OLED_I2C_ADDR;
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) {
        addr = 0x3D;
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) {
            Wire.end();
            return false;
        }
        Serial.printf("oled: using alt I2C addr 0x%02X\n", addr);
    }

    // Release I2C0 so Adafruit_SH1106G::begin() can acquire it once.
    Wire.end();
    Wire.setPins(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!_display.begin(addr, false)) {
        return false;
    }
    Wire.setClock(400000);

    _present = true;
    _display.setRotation(0);
    _display.setContrast(0x7F);
    _display.clearDisplay();
    _display.display();

    eyes.begin();
    eyes.setAnimEnabled(settings.data.oledAnim);
    return true;
}

void DisplayManager::setMood(HeroMood mood) {
    _mood = mood;
    eyes.setMood(mood);
    notifyActivity();
}

void DisplayManager::showMessage(const char* text) {
    strncpy(_message, text, sizeof(_message) - 1);
    _message[sizeof(_message) - 1] = 0;
    eyes.setCompact(true);          // shrink + raise eyes to make room
    notifyActivity();
}

void DisplayManager::clearMessage() {
    if (_message[0] != 0) {
        _message[0] = 0;
        eyes.setCompact(false);     // restore full-size face
        notifyActivity();
    }
}

void DisplayManager::setConnected(bool connected, bool apMode, const char* ip) {
    _connected = connected;
    _apMode = apMode;
    if (ip) {
        strncpy(_ip, ip, sizeof(_ip) - 1);
        _ip[sizeof(_ip) - 1] = 0;
    }
}

void DisplayManager::notifyActivity() {
    _lastActivity = millis();
    if (_sleeping) {
        _sleeping = false;
        eyes.setSleeping(false);
    }
}

DisplayManager::Screen DisplayManager::pickScreen(uint32_t now) {
    if (now - _bootStart < 1500) {
        return Screen::BOOT;
    }
    if (now - _bootStart < 8000) {
        return Screen::CONNECTION;
    }
    if (_message[0] != 0) {
        return Screen::TEXT;        // persistent until explicitly cleared
    }
    if (now - _lastActivity > (uint32_t)SLEEP_TIMEOUT_MS) {
        if (!_sleeping) {
            _sleeping = true;
            eyes.setSleeping(true);
        }
        return Screen::FACE;
    }
    return Screen::FACE;
}

void DisplayManager::update(uint32_t now) {
    if (!_present) return;
    if (now - _lastPush < 40) return;
    _lastPush = now;

    Screen next = pickScreen(now);
    if (next != _screen) {
        _screen = next;
    }
    _display.clearDisplay();
    switch (_screen) {
        case Screen::BOOT:       renderBoot(); break;
        case Screen::CONNECTION: renderConnection(); break;
        case Screen::TEXT:       renderText(now); break;
        case Screen::FACE:       renderFace(now); break;
    }
    _display.display();
}

void DisplayManager::renderBoot() {
    _display.setTextColor(SH110X_WHITE);
    _display.setTextSize(1);
    _display.setCursor(22, 20);
    _display.print("HERO");
    _display.setCursor(30, 34);
    _display.print("Booting...");

    int w = 80;
    int p = ((millis() - _bootStart) % 1200) * w / 1200;
    _display.drawRect(24, 46, w + 2, 6, SH110X_WHITE);
    if (p > 0) _display.fillRect(25, 47, p, 4, SH110X_WHITE);
}

void DisplayManager::renderConnection() {
    _display.setTextColor(SH110X_WHITE);
    _display.setTextSize(1);
    _display.setCursor(22, 8);
    _display.print("Join HERO AP");

    _display.setCursor(8, 22);
    _display.print("AP: 192.168.4.1");

    _display.setCursor(8, 36);
    _display.print("Pass: hero1234");
    _display.setCursor(8, 48);
    _display.print("token: hero");

    if ((millis() / 500) % 2) {
        _display.fillCircle(116, 8, 3, SH110X_WHITE);
    }
}

// Wrap the message into up to 3 lines (19 chars max per line at font size 1),
// preferring to break at spaces. Appends "..." when truncated.
static int wrapLines(const char* msg, char lines[3][20]) {
    const int MAXW = 19;
    int n = 0;
    const char* p = msg;
    while (*p && n < 3) {
        int take = 0;
        int lastSpace = -1;
        while (take < MAXW && p[take] != '\0') {
            if (p[take] == ' ') lastSpace = take;
            take++;
        }
        if (p[take] != '\0' && lastSpace > 0) take = lastSpace;
        memcpy(lines[n], p, take);
        lines[n][take] = 0;
        if (p[take] == ' ') take++;
        p += take;
        n++;
    }
    if (*p && n > 0) {
        int l = strlen(lines[n - 1]);
        if (l > 3) {
            lines[n - 1][l - 3] = '.';
            lines[n - 1][l - 2] = '.';
            lines[n - 1][l - 1] = '.';
            lines[n - 1][l] = 0;
        }
    }
    return n;
}

void DisplayManager::renderText(uint32_t now) {
    // compact eyes stay animated (idle script, moods, driving reactions)
    eyes.update(now);
    eyes.render(_display, now);

    // persistent rounded-corner text box
    const int16_t bx = 6;
    const int16_t by = 30;
    const int16_t bw = OLED_WIDTH - 12;
    const int16_t bh = 33;
    _display.fillRoundRect(bx, by, bw, bh, 6, SH110X_WHITE);
    _display.drawRoundRect(bx, by, bw, bh, 6, SH110X_BLACK);

    char lines[3][20];
    int n = wrapLines(_message, lines);
    _display.setTextColor(SH110X_BLACK);
    _display.setTextSize(1);
    int ty = by + (bh - n * 8) / 2 + 1;
    for (int i = 0; i < n; i++) {
        int x = (OLED_WIDTH - (int)strlen(lines[i]) * 6) / 2;
        _display.setCursor(x, ty);
        _display.print(lines[i]);
        ty += 8;
    }
}

void DisplayManager::renderFace(uint32_t now) {
    eyes.update(now);
    eyes.render(_display, now);
}
