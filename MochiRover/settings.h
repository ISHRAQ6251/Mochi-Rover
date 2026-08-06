#pragma once

#include <Arduino.h>

struct RoverSettings {
    char wifiSSID[33] = {0};
    char wifiPass[65] = {0};
    char authToken[33] = {0};

    int camResolution = 8;    // framesize_t: 8 = FRAMESIZE_VGA
    int camQuality = 12;      // JPEG quality 10..63 (lower = better)
    int camFps = 25;
    int camFormat = 1;        // 1 = JPEG (MJPEG stream), 0 = BMP fallback

    bool flashlightOn = false;
    bool oledAnim = true;
};

class Settings {
public:
    void begin();
    void load();
    void save();
    void reset();

    RoverSettings data;

    // convenience accessors used by the rest of the firmware
    void setWifi(const char* ssid, const char* pass);
    bool hasWifi() const;
    void setToken(const char* token);
    void setCamera(int resolution, int quality, int fps, int format);
    void setFlashlight(bool on);
    void setOledAnim(bool on);

private:
    void writeString(const char* key, const char* value);
    void readString(const char* key, char* dst, size_t len, const char* def);
};

extern Settings settings;
