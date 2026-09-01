#pragma once

#include <Arduino.h>

struct RoverSettings {
    char authToken[33] = {0};

    int camResolution = 9;    // framesize_t: 9 = FRAMESIZE_SVGA (800x600)
    int camQuality = 12;      // JPEG quality 10..63 (lower = better)
    bool camFlip = false;     // mirror + vflip the image

    bool flashlightOn = false;
    bool oledAnim = true;
    bool reverseLeft = false;
    bool reverseRight = false;
};

class Settings {
public:
    void begin();
    void load();
    void save();
    void reset();

    RoverSettings data;

    // convenience accessors used by the rest of the firmware
    void setToken(const char* token);
    void setCamera(int resolution, int quality);
    void setCamFlip(bool flip);
    void setFlashlight(bool on);
    void setOledAnim(bool on);
    void setMotorReverse(bool left, bool right);

private:
    void writeString(const char* key, const char* value);
    void readString(const char* key, char* dst, size_t len, const char* def);
};

extern Settings settings;
