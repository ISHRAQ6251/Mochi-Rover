#include "settings.h"
#include "config.h"
#include <Preferences.h>

Settings settings;

static Preferences prefs;

void Settings::begin() {
    prefs.begin(NVS_NAMESPACE, false);
    load();
}

void Settings::load() {
    readString(NVS_KEY_TOKEN, data.authToken, sizeof(data.authToken), AUTH_DEFAULT_TOKEN);

    data.camResolution = prefs.getInt(NVS_KEY_RES, data.camResolution);
    data.camQuality   = prefs.getInt(NVS_KEY_QUALITY, data.camQuality);
    data.camFlip      = prefs.getBool(NVS_KEY_FLIP, data.camFlip);
    data.flashlightOn = prefs.getBool(NVS_KEY_FLASH, data.flashlightOn);
    data.oledAnim     = prefs.getBool(NVS_KEY_OLEDANIM, data.oledAnim);
    data.reverseLeft  = prefs.getBool(NVS_KEY_REV_L, data.reverseLeft);
    data.reverseRight = prefs.getBool(NVS_KEY_REV_R, data.reverseRight);
}

void Settings::save() {
    writeString(NVS_KEY_TOKEN, data.authToken);
    prefs.putInt(NVS_KEY_RES, data.camResolution);
    prefs.putInt(NVS_KEY_QUALITY, data.camQuality);
    prefs.putBool(NVS_KEY_FLIP, data.camFlip);
    prefs.putBool(NVS_KEY_FLASH, data.flashlightOn);
    prefs.putBool(NVS_KEY_OLEDANIM, data.oledAnim);
    prefs.putBool(NVS_KEY_REV_L, data.reverseLeft);
    prefs.putBool(NVS_KEY_REV_R, data.reverseRight);
    prefs.end();
    prefs.begin(NVS_NAMESPACE, false);
}

void Settings::reset() {
    prefs.clear();
    prefs.end();
    prefs.begin(NVS_NAMESPACE, false);
    data = RoverSettings();
    strncpy(data.authToken, AUTH_DEFAULT_TOKEN, sizeof(data.authToken) - 1);
    save();
}

void Settings::setMotorReverse(bool left, bool right) {
    data.reverseLeft = left;
    data.reverseRight = right;
    save();
}

void Settings::setToken(const char* token) {
    strncpy(data.authToken, token, sizeof(data.authToken) - 1);
    data.authToken[sizeof(data.authToken) - 1] = 0;
    save();
}

void Settings::setCamera(int resolution, int quality) {
    data.camResolution = resolution;
    data.camQuality = quality;
    save();
}

void Settings::setCamFlip(bool flip) {
    data.camFlip = flip;
    save();
}

void Settings::setFlashlight(bool on) {
    data.flashlightOn = on;
    save();
}

void Settings::setOledAnim(bool on) {
    data.oledAnim = on;
    save();
}

void Settings::writeString(const char* key, const char* value) {
    prefs.putString(key, value);
}

void Settings::readString(const char* key, char* dst, size_t len, const char* def) {
    String s = prefs.getString(key, def);
    s.toCharArray(dst, len);
}
