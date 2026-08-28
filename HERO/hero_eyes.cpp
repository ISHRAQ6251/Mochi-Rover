#include "hero_eyes.h"
#include "config.h"
#include <math.h>

HeroEyes eyes;

// face geometry on the 128x64 OLED
static const int16_t EYE_W = 44;
static const int16_t EYE_H = 34;
static const int16_t EYE_Y = 6;
static const int16_t EYE_LX = 12;
static const int16_t EYE_RX = 72;
static const int16_t MOUTH_Y = 48;

// compact (text mode) eye geometry: shrunk and raised to leave room for the
// persistent rounded text box at the bottom of the display
static const int16_t C_EYE_W = 30;
static const int16_t C_EYE_H = 24;
static const int16_t C_EYE_Y = 3;
static const int16_t C_EYE_LX = 18;
static const int16_t C_EYE_RX = 80;

static void drawArc(Adafruit_GFX& c, int16_t cx, int16_t cy, int16_t r,
                    float a0, float a1, uint16_t color) {
    for (float a = a0; a <= a1; a += 0.06f) {
        c.drawPixel(cx + (int16_t)(r * cosf(a)),
                    cy + (int16_t)(r * sinf(a)), color);
    }
}

void HeroEyes::begin() {
    _lastUpdate = millis();
    _nextBlinkAt = millis() + 1800;
    _nextSaccadeAt = millis() + 1200;
}

const char* HeroEyes::moodName() const {
    switch (_mood) {
        case HeroMood::HAPPY:    return "happy";
        case HeroMood::ANGRY:    return "angry";
        case HeroMood::CURIOUS:  return "curious";
        case HeroMood::DEAD:     return "dead";
        case HeroMood::SLEEPY:   return "sleepy";
        case HeroMood::WINK:     return "wink";
        case HeroMood::IDLE:     return "idle";
    }
    return "idle";
}

void HeroEyes::setMood(HeroMood m) {
    _mood = m;
    switch (m) {
        case HeroMood::WINK:
            _moodUntil = millis() + 1500;
            break;
        default:
            _moodUntil = millis() + MOOD_OVERRIDE_MS;
            break;
    }
}

void HeroEyes::setDriving(int16_t throttle, int16_t steering) {
    _throttle = throttle;
    _steering = steering;
}

void HeroEyes::setSleeping(bool sleeping) {
    _sleeping = sleeping;
    if (sleeping) {
        _blinkUntil = 0;
    }
}

void HeroEyes::setAnimEnabled(bool enabled) {
    _animEnabled = enabled;
    if (!enabled) {
        _blinkUntil = 0;
    }
}

void HeroEyes::setCompact(bool compact) {
    _compact = compact;
}

// Autonomous idle behavior script: the character periodically looks around,
// blinks, and spontaneously shifts through small expressions while idle.
void HeroEyes::updateIdleScript(uint32_t now) {
    if (now >= _idleNextPickAt) {
        uint8_t r = (uint8_t)random(0, 100);
        if (r < 40) {
            _idlePose = IdlePose::NEUTRAL;
            _idleMouth = (random(0, 100) < 30) ? MOUTH_SMILE : MOUTH_NEUTRAL;
            _idlePoseUntil = 0;
            _idleNextPickAt = now + 900 + random(0, 1800);
        } else if (r < 52) {
            _idlePose = IdlePose::LOOK;
            _idleMouth = MOUTH_NEUTRAL;
            _idlePoseUntil = now + 700 + random(0, 700);
            _idleNextPickAt = _idlePoseUntil;
        } else if (r < 64) {
            _idlePose = IdlePose::HAPPY;
            _idleMouth = MOUTH_SMILE;
            _idlePoseUntil = now + 1200 + random(0, 900);
            _idleNextPickAt = _idlePoseUntil;
        } else if (r < 73) {
            _idlePose = IdlePose::CURIOUS;
            _idleMouth = MOUTH_O;
            _idlePoseUntil = now + 900 + random(0, 800);
            _idleNextPickAt = _idlePoseUntil;
        } else if (r < 82) {
            _idlePose = IdlePose::SURPRISE;
            _idleMouth = MOUTH_OPEN;
            _idlePoseUntil = now + 700 + random(0, 500);
            _idleNextPickAt = _idlePoseUntil;
        } else if (r < 90) {
            _idlePose = IdlePose::SUSPICIOUS;
            _idleMouth = MOUTH_FROWN;
            _idlePoseUntil = now + 900 + random(0, 800);
            _idleNextPickAt = _idlePoseUntil;
        } else if (r < 96) {
            _idlePose = IdlePose::DOZE;
            _idleMouth = MOUTH_NEUTRAL;
            _idlePoseUntil = now + 1000 + random(0, 700);
            _idleNextPickAt = _idlePoseUntil;
        } else {
            _idlePose = IdlePose::WINK;
            _idleMouth = MOUTH_SMILE;
            _idlePoseUntil = now + 900;
            _idleNextPickAt = _idlePoseUntil;
        }
    }

    if (_idlePoseUntil && now >= _idlePoseUntil) {
        _idlePose = IdlePose::NEUTRAL;
        _idleMouth = MOUTH_NEUTRAL;
        _idlePoseUntil = 0;
        _idleNextPickAt = now + 600 + random(0, 1200);
    }

    // active look-around drives faster, wider saccades
    if (_idlePose == IdlePose::LOOK) {
        if (now >= _nextSaccadeAt) {
            _targetPupilX = random(-6, 7);
            _targetPupilY = random(-5, 6);
            _nextSaccadeAt = now + 250 + random(0, 350);
        }
    }
}

void HeroEyes::update(uint32_t now) {
    uint32_t dt = (uint32_t)min((uint32_t)(now - _lastUpdate), 50UL);
    _lastUpdate = now;

    // transient moods expire back to idle
    if (_moodUntil && now >= _moodUntil) {
        _moodUntil = 0;
        _mood = HeroMood::IDLE;
    }

    // blinking
    if (_animEnabled && !_sleeping && now >= _nextBlinkAt) {
        _blinkUntil = now + 130;
        _nextBlinkAt = now + 1800 + random(0, 2500);
    }
    if (_blinkUntil && now >= _blinkUntil) {
        _blinkUntil = 0;
    }

    // saccades while idle
    if (_animEnabled && !_sleeping && _mood == HeroMood::IDLE && !(_throttle != 0 || _steering != 0)) {
        if (_idlePose != IdlePose::LOOK && now >= _nextSaccadeAt) {
            _targetPupilX = random(-4, 5);
            _targetPupilY = random(-4, 5);
            _nextSaccadeAt = now + 900 + random(0, 2200);
        }
        updateIdleScript(now);
    }

    // target selection
    bool driving = (_throttle != 0 || _steering != 0);
    if (_sleeping) {
        _targetLid = 0;
        _targetPupilX = 0;
        _targetPupilY = 0;
        _brow = 170;
    } else if (driving) {
        if (!_wasDriving) {
            _drivingKick = 255;   // excited "whoa" on drive start
        }
        _targetLid = 220 - (uint8_t)min((uint16_t)(abs(_throttle) / 3), (uint16_t)80);
        _targetPupilX = _steering / 42;
        _targetPupilY = 3;
        _brow = 90;
        _targetSaccadeX = 0;
        _targetSaccadeY = 0;
    } else {
        _drivingKick = 0;
        _targetSaccadeX = 0;
        _targetSaccadeY = 0;
        switch (_mood) {
            case HeroMood::HAPPY:    _targetLid = 255; _targetPupilX = 0; _targetPupilY = -6; _brow = 200; break;
            case HeroMood::ANGRY:    _targetLid = 230; _targetPupilX = 0; _targetPupilY = 2;  _brow = 60;  break;
            case HeroMood::CURIOUS:  _targetLid = 255; _targetPupilX = 5; _targetPupilY = -4; _brow = 190; break;
            case HeroMood::DEAD:     _targetLid = 255; _targetPupilX = 0; _targetPupilY = 0;  _brow = 128; break;
            case HeroMood::SLEEPY:   _targetLid = 60;  _targetPupilX = 0; _targetPupilY = 2;  _brow = 160; break;
            case HeroMood::IDLE:
            default:
                _targetLid = 255;
                _targetPupilX = _targetSaccadeX;
                _targetPupilY = _targetSaccadeY;
                switch (_idlePose) {
                    case IdlePose::NEUTRAL:
                        _brow = 140;
                        break;
                    case IdlePose::LOOK:
                        _targetLid = 255;
                        _targetPupilX = _targetSaccadeX;
                        _targetPupilY = _targetSaccadeY;
                        _brow = 155;
                        break;
                    case IdlePose::HAPPY:
                        _targetLid = 255;
                        _targetPupilX = 0;
                        _targetPupilY = -6;
                        _brow = 200;
                        break;
                    case IdlePose::CURIOUS:
                        _targetLid = 255;
                        _targetPupilX = 5;
                        _targetPupilY = -4;
                        _brow = 190;
                        break;
                    case IdlePose::SURPRISE:
                        _targetLid = 255;
                        _targetPupilX = 0;
                        _targetPupilY = 0;
                        _brow = 220;
                        break;
                    case IdlePose::SUSPICIOUS:
                        _targetLid = 185;
                        _targetPupilX = 3;
                        _targetPupilY = 2;
                        _brow = 70;
                        break;
                    case IdlePose::DOZE:
                        _targetLid = 60;
                        _targetPupilX = 0;
                        _targetPupilY = 2;
                        _brow = 160;
                        break;
                    case IdlePose::WINK:
                        _targetLid = 255;
                        _targetPupilX = 0;
                        _targetPupilY = -2;
                        _brow = 170;
                        break;
                }
                break;
        }
    }
    _wasDriving = driving;

    // excitement burst on drive start (briefly wider eyes)
    if (_drivingKick > 0) {
        _targetLid = 255;
        _targetPupilY = -2;
        _drivingKick = (_drivingKick > dt) ? (_drivingKick - dt) : 0;
    }

    // ease toward targets
    int32_t dlid = (int32_t)_targetLid - _lid;
    _lid = (uint8_t)constrain((int32_t)_lid + dlid * (int32_t)dt / 50, 0, 255);
    int32_t dpx = (int32_t)_targetPupilX - _pupilX;
    _pupilX = constrain((int32_t)_pupilX + dpx * (int32_t)dt / 60, -16, 16);
    int32_t dpy = (int32_t)_targetPupilY - _pupilY;
    _pupilY = constrain((int32_t)_pupilY + dpy * (int32_t)dt / 60, -16, 16);

    // compact-mode transition: smooth shrink + rise
    float target = _compact ? 1.0f : 0.0f;
    if (_compactT < target) {
        _compactT = min(1.0f, _compactT + (float)dt / 280.0f);
    } else if (_compactT > target) {
        _compactT = max(0.0f, _compactT - (float)dt / 280.0f);
    }
}

HeroEyes::EyePose HeroEyes::computePose(bool left, uint32_t now) {
    EyePose p;
    p.px = _pupilX;
    p.py = _pupilY;
    p.lid = _lid;
    p.brow = _brow;

    if (_sleeping) {
        p.lid = 0;
        return p;
    }

    if (_blinkUntil && now < _blinkUntil) {
        uint32_t el = now - (_blinkUntil - 130);
        if (el < 45) {
            p.lid = (uint8_t)(255 - (uint32_t)235 * el / 45);
        } else {
            p.lid = (uint8_t)(20 + (uint32_t)235 * (el - 45) / 85);
        }
    }

    if (_mood == HeroMood::WINK && !left) {
        p.lid = 0;
    }
    if (_mood == HeroMood::IDLE && _idlePose == IdlePose::WINK && !left) {
        p.lid = 0;
    }
    return p;
}

void HeroEyes::drawEye(Adafruit_GFX& c, int16_t x, int16_t y, int16_t w, int16_t h,
                        const EyePose& p, bool left, uint32_t now) {
    (void)left;
    (void)now;
    int16_t cx = x + w / 2;
    int16_t cy = y + h / 2;

    c.fillRect(x, y, w, h, 1);      // sclera
    c.drawRect(x, y, w, h, 0);

    if (_shownMood == HeroMood::DEAD) {
        int16_t r = w / 4;
        c.drawLine(cx - r, cy - r, cx + r, cy + r, 0);
        c.drawLine(cx + r, cy - r, cx - r, cy + r, 0);
        return;
    }

    int16_t r = w / 6;
    if (r < 4) r = 4;
    int16_t maxPx = w / 2 - r - 3;
    int16_t maxPy = h / 3 - r;
    int16_t px = constrain(p.px, -maxPx, maxPx);
    int16_t py = constrain(p.py, -maxPy, maxPy);
    c.fillCircle(cx + px, cy + py, r, 0);                       // pupil
    int16_t hl = r / 3;
    if (hl < 1) hl = 1;
    c.fillCircle(cx + px - r / 3, cy + py - r / 3, hl, 1);      // highlight

    int16_t lidH = (int16_t)(255 - p.lid) * h / 255;
    if (lidH > 0) {
        c.fillRect(x, y, w, lidH, 0);
    }
}

void HeroEyes::drawBrow(Adafruit_GFX& c, int16_t x, int16_t y, int16_t w, bool left,
                         const EyePose& p) {
    int16_t baseY = y - 2;
    int16_t lift = (200 - (int16_t)p.brow) / 40;
    int16_t yL, yR;

    switch (_shownMood) {
        case HeroMood::ANGRY:   // inner end lower
            yL = baseY + lift + (left ? -2 : 2);
            yR = baseY + lift + (left ? 2 : -2);
            break;
        case HeroMood::CURIOUS:
            yL = baseY + (left ? lift + 3 : lift);
            yR = baseY + (left ? lift + 3 : lift);
            break;
        default:
            yL = baseY + lift;
            yR = baseY + lift;
            break;
    }
    c.drawLine(x + 4, yL, x + w - 4, yR, 1);
}

void HeroEyes::render(Adafruit_GFX& c, uint32_t now) {
    _shownMood = _mood;

    // interpolate geometry between full size and compact (text) layout
    float t = _compactT;
    int16_t eyeW = EYE_W + (int16_t)((C_EYE_W - EYE_W) * t);
    int16_t eyeH = EYE_H + (int16_t)((C_EYE_H - EYE_H) * t);
    int16_t eyeY = EYE_Y + (int16_t)((C_EYE_Y - EYE_Y) * t);
    int16_t lx   = EYE_LX + (int16_t)((C_EYE_LX - EYE_LX) * t);
    int16_t rx   = EYE_RX + (int16_t)((C_EYE_RX - EYE_RX) * t);
    bool compact = (t > 0.5f);

    EyePose lp = computePose(true, now);
    EyePose rp = computePose(false, now);

    drawEye(c, lx, eyeY, eyeW, eyeH, lp, true, now);
    drawEye(c, rx, eyeY, eyeW, eyeH, rp, false, now);
    if (!compact) {
        drawBrow(c, EYE_LX, EYE_Y, EYE_W, true, lp);
        drawBrow(c, EYE_RX, EYE_Y, EYE_W, false, rp);
    }

    // mouth
    drawMouth(c, now);

    // sleeping Zzz
    if (_sleeping) {
        uint32_t tz = now % 6000;
        for (int i = 0; i < 3; i++) {
            int32_t ph = (int32_t)tz - i * 1800;
            if (ph < 0 || ph > 3000) continue;
            int16_t zy = 40 - ph / 150;
            int16_t zx = 92 + i * 4;
            int16_t sz = 3 + i;
            c.drawLine(zx, zy, zx + sz, zy, 1);
            c.drawLine(zx + sz, zy, zx, zy + sz, 1);
            c.drawLine(zx, zy + sz, zx + sz, zy + sz, 1);
        }
    }
}

void HeroEyes::drawMouth(Adafruit_GFX& c, uint32_t now) {
    (void)now;
    int16_t cx = 64;
    int16_t cy = MOUTH_Y;

    bool driving = (_throttle != 0 || _steering != 0);
    bool idleAuto = (_shownMood == HeroMood::IDLE && !_sleeping && !driving);

    if (_sleeping) {
        c.drawLine(cx - 8, cy, cx + 8, cy, 1);
    } else if (idleAuto) {
        switch (_idleMouth) {
            case MOUTH_SMILE:
                drawArc(c, cx, cy + 6, 9, (float)(PI / 6), (float)(5 * PI / 6), 1);
                break;
            case MOUTH_OPEN:
                c.fillCircle(cx, cy + 2, 4, 1);
                break;
            case MOUTH_FROWN:
                drawArc(c, cx, cy + 4, 8, (float)(7 * PI / 6), (float)(11 * PI / 6), 1);
                break;
            case MOUTH_O:
                c.fillCircle(cx, cy + 1, 2, 1);
                break;
            default:
                c.drawLine(cx - 6, cy, cx + 6, cy, 1);
                break;
        }
    } else {
        switch (_shownMood) {
            case HeroMood::HAPPY:
                drawArc(c, cx, cy + 6, 9, (float)(PI / 6), (float)(5 * PI / 6), 1);
                break;
            case HeroMood::ANGRY:
                drawArc(c, cx, cy + 4, 8, (float)(7 * PI / 6), (float)(11 * PI / 6), 1);
                break;
            case HeroMood::DEAD:
                c.drawLine(cx - 7, cy, cx + 7, cy, 1);
                c.drawLine(cx - 3, cy - 2, cx + 3, cy - 2, 1);
                break;
            case HeroMood::CURIOUS:
                c.fillCircle(cx, cy + 1, 2, 1);
                break;
            default:
                if (driving) {
                    drawArc(c, cx, cy + 5, 7, (float)(PI / 5), (float)(4 * PI / 5), 1);
                } else {
                    c.drawLine(cx - 6, cy, cx + 6, cy, 1);
                }
                break;
        }
    }
}
