#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

// Procedural "Dasai Mochi"-style animated eyes for the SH1106 OLED.
//
// The eyes are drawn with Adafruit_GFX primitives (no bitmaps) and follow a
// small state machine: idle script (blinks, saccades, micro-expressions),
// explicit mood overrides, driving reactions and sleep.

enum class HeroMood : uint8_t {
    HAPPY = 0,
    ANGRY,
    CURIOUS,
    DEAD,
    SLEEPY,
    WINK,
    IDLE
};

class HeroEyes {
public:
    void begin();

    void setMood(HeroMood mood);
    HeroMood mood() const { return _mood; }
    const char* moodName() const;

    // Driving reactions (called by WebServerMgr)
    void setDriving(int16_t throttle, int16_t steering);
    void setSleeping(bool sleeping);
    // When disabled: no auto-blink/saccades/idle script.
    void setAnimEnabled(bool enabled);
    // Compact mode: eyes shrink and rise to the top of the display to leave
    // room for the persistent text box. Smoothly animated in update().
    void setCompact(bool compact);
    bool compact() const { return _compact; }

    void update(uint32_t now);                 // advance animation state
    void render(Adafruit_GFX& canvas, uint32_t now);  // draw both eyes + mouth

private:
    enum class IdlePose : uint8_t {
        NEUTRAL,    // gentle neutral look with occasional blinks + saccades
        LOOK,       // active look-around (wider saccades)
        HAPPY,      // content squint + smile
        CURIOUS,    // tilted curious peek + small mouth
        SURPRISE,   // wide eyes + open mouth
        SUSPICIOUS, // narrowed eyes + slight frown
        DOZE,       // heavy lids (brief doze-off)
        WINK        // one-eye wink + smile
    };
    enum : uint8_t {
        MOUTH_NEUTRAL = 0,
        MOUTH_SMILE,
        MOUTH_OPEN,
        MOUTH_FROWN,
        MOUTH_O
    };

    struct EyePose {
        int16_t px;    // pupil offset px
        int16_t py;    // pupil offset py
        uint8_t lid;   // lid openness 0..255 (255 = fully open)
        uint8_t brow;  // brow raise 0..255
    };

    EyePose computePose(bool left, uint32_t now);
    void updateIdleScript(uint32_t now);
    void drawEye(Adafruit_GFX& canvas, int16_t x, int16_t y, int16_t w, int16_t h,
                 const EyePose& pose, bool left, uint32_t now);
    void drawBrow(Adafruit_GFX& canvas, int16_t x, int16_t y, int16_t w, bool left,
                  const EyePose& pose);
    void drawMouth(Adafruit_GFX& canvas, uint32_t now);

    HeroMood _mood = HeroMood::IDLE;
    HeroMood _shownMood = HeroMood::IDLE;

    int16_t _throttle = 0;
    int16_t _steering = 0;
    bool    _sleeping = false;
    bool    _animEnabled = true;

    int16_t _pupilX = 0;
    int16_t _pupilY = 0;
    int16_t _targetPupilX = 0;
    int16_t _targetPupilY = 0;
    uint8_t _lid = 255;
    uint8_t _targetLid = 255;
    uint8_t _brow = 128;

    uint32_t _nextBlinkAt = 0;
    uint32_t _blinkUntil = 0;
    uint32_t _nextSaccadeAt = 0;
    uint32_t _moodUntil = 0;
    uint32_t _lastUpdate = 0;
    int16_t _targetSaccadeX = 0;
    int16_t _targetSaccadeY = 0;

    bool     _wasDriving = false;
    uint8_t  _drivingKick = 0;   // excitement burst on drive start

    IdlePose _idlePose = IdlePose::NEUTRAL;
    uint8_t  _idleMouth = MOUTH_NEUTRAL;
    uint32_t _idlePoseUntil = 0;
    uint32_t _idleNextPickAt = 0;

    // compact-mode transition (0 = full size, 1 = shrunk + raised)
    bool     _compact = false;
    float    _compactT = 0.0f;
};

extern HeroEyes eyes;
