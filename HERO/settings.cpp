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
    if (data.authToken[0] == 0) {
        strncpy(data.authToken, AUTH_DEFAULT_TOKEN, sizeof(data.authToken) - 1);
        data.authToken[sizeof(data.authToken) - 1] = 0;
    }

    data.camResolution = constrain(prefs.getInt(NVS_KEY_RES, data.camResolution), 0, 13);
    data.camQuality   = constrain(prefs.getInt(NVS_KEY_QUALITY, data.camQuality), 10, 63);
    data.camFlip      = prefs.getBool(NVS_KEY_FLIP, data.camFlip);
    data.flashlightOn = prefs.getBool(NVS_KEY_FLASH, data.flashlightOn);
    data.oledAnim     = prefs.getBool(NVS_KEY_OLEDANIM, data.oledAnim);
    data.reverseLeft  = prefs.getBool(NVS_KEY_REV_L, data.reverseLeft);
    data.reverseRight = prefs.getBool(NVS_KEY_REV_R, data.reverseRight);
    data.trimLeft     = (uint8_t)constrain(prefs.getUChar(NVS_KEY_TRIM_L, data.trimLeft),
                                           MOTOR_TRIM_MIN, MOTOR_TRIM_MAX);
    data.trimRight    = (uint8_t)constrain(prefs.getUChar(NVS_KEY_TRIM_R, data.trimRight),
                                           MOTOR_TRIM_MIN, MOTOR_TRIM_MAX);
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
    prefs.putUChar(NVS_KEY_TRIM_L, data.trimLeft);
    prefs.putUChar(NVS_KEY_TRIM_R, data.trimRight);
    // Preferences writes commit immediately; the handle is opened once in
    // begin() and stays open. Closing/reopening here only added latency and
    // extra flash operations to every setting change.
}

void Settings::reset() {
    prefs.clear();
    data = RoverSettings();
    strncpy(data.authToken, AUTH_DEFAULT_TOKEN, sizeof(data.authToken) - 1);
    save();
}

void Settings::setMotorCal(bool reverseLeft, bool reverseRight, uint8_t trimLeft, uint8_t trimRight) {
    data.reverseLeft = reverseLeft;
    data.reverseRight = reverseRight;
    data.trimLeft = (uint8_t)constrain(trimLeft, MOTOR_TRIM_MIN, MOTOR_TRIM_MAX);
    data.trimRight = (uint8_t)constrain(trimRight, MOTOR_TRIM_MIN, MOTOR_TRIM_MAX);
    save();
}

void Settings::setToken(const char* token) {
    strncpy(data.authToken, token, sizeof(data.authToken) - 1);
    data.authToken[sizeof(data.authToken) - 1] = 0;
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
