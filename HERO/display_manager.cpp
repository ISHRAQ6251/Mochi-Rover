#include "display_manager.h"
#include "config.h"
#include "settings.h"

DisplayManager displayMgr;

void DisplayManager::begin() {
    _bootStart = millis();
    _lastActivity = millis();

    Wire1.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    _display.begin(OLED_I2C_ADDR, false);
    _display.clearDisplay();
    _display.display();

    eyes.begin();
    eyes.setAnimEnabled(settings.data.oledAnim);
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
    if (!_connected) {
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
    _canvas.fillScreen(0);
    Screen next = pickScreen(now);
    if (next != _screen) {
        _screen = next;
    }
    switch (_screen) {
        case Screen::BOOT:       renderBoot(); break;
        case Screen::CONNECTION: renderConnection(); break;
        case Screen::TEXT:       renderText(now); break;
        case Screen::FACE:       renderFace(now); break;
    }
    push();
}

void DisplayManager::renderBoot() {
    _canvas.setTextColor(1);
    _canvas.setTextSize(1);
    _canvas.setCursor(22, 20);
    _canvas.print("HERO");
    _canvas.setCursor(30, 34);
    _canvas.print("Booting...");

    // little animated progress bar
    int w = 80;
    int p = ((millis() - _bootStart) % 1200) * w / 1200;
    _canvas.drawRect(24, 46, w + 2, 6, 1);
    if (p > 0) _canvas.fillRect(25, 47, p, 4, 1);
}

void DisplayManager::renderConnection() {
    _canvas.setTextColor(1);
    _canvas.setTextSize(1);
    _canvas.setCursor(22, 8);
    _canvas.print("Wi-Fi Setup");

    _canvas.setCursor(8, 22);
    if (_apMode) {
        _canvas.print("AP: 192.168.4.1");
    } else {
        _canvas.print("Connecting...");
    }

    _canvas.setCursor(8, 36);
    _canvas.print("Open browser to");
    _canvas.setCursor(8, 48);
    _canvas.print("set network + mood.");

    // blinking dot
    if ((millis() / 500) % 2) {
        _canvas.fillCircle(116, 8, 3, 1);
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
    eyes.render(_canvas, now);

    // persistent rounded-corner text box
    const int16_t bx = 6;
    const int16_t by = 30;
    const int16_t bw = OLED_WIDTH - 12;
    const int16_t bh = 33;
    _canvas.fillRoundRect(bx, by, bw, bh, 6, 1);
    _canvas.drawRoundRect(bx, by, bw, bh, 6, 0);

    char lines[3][20];
    int n = wrapLines(_message, lines);
    _canvas.setTextColor(0);
    _canvas.setTextSize(1);
    int ty = by + (bh - n * 8) / 2 + 1;
    for (int i = 0; i < n; i++) {
        int x = (OLED_WIDTH - (int)strlen(lines[i]) * 6) / 2;
        _canvas.setCursor(x, ty);
        _canvas.print(lines[i]);
        ty += 8;
    }
}

void DisplayManager::renderFace(uint32_t now) {
    eyes.update(now);
    eyes.render(_canvas, now);
}

void DisplayManager::push() {
    _display.drawBitmap(0, 0, _canvas.getBuffer(), OLED_WIDTH, OLED_HEIGHT, 1);
    _display.display();
}
