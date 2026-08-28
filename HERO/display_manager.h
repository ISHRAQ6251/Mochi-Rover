#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"
#include "hero_eyes.h"

// Display priority (highest first):
//   boot -> connection -> text(persistent) -> sleep -> mood/driving -> idle
class DisplayManager {
public:
    void begin();

    void update(uint32_t now);   // decide screen, render, push to OLED

    // external inputs
    void setMood(HeroMood mood);
    void showMessage(const char* text);  // persists until clearMessage()
    void clearMessage();
    bool messageActive() const { return _message[0] != 0; }
    const char* message() const { return _message; }
    void setConnected(bool connected, bool apMode, const char* ip);
    void notifyActivity();       // any user input resets the sleep timer

    bool sleeping() const { return _sleeping; }

private:
    enum class Screen { BOOT, CONNECTION, TEXT, FACE };

    Screen pickScreen(uint32_t now);
    void renderBoot();
    void renderConnection();
    void renderText(uint32_t now);
    void renderFace(uint32_t now);
    void push();

    Adafruit_SH1106G _display{OLED_WIDTH, OLED_HEIGHT, &Wire1, -1};
    GFXcanvas1 _canvas{OLED_WIDTH, OLED_HEIGHT};

    Screen _screen = Screen::BOOT;
    uint32_t _bootStart = 0;
    uint32_t _lastActivity = 0;

    bool _connected = false;
    bool _apMode = false;
    char _ip[24] = "0.0.0.0";

    char _message[64] = {0};

    HeroMood _mood = HeroMood::IDLE;
    bool _sleeping = false;
};

extern DisplayManager displayMgr;
