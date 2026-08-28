// MochiRover - ESP32-S3-CAM rover with Dasai Mochi-style OLED eyes
//
// Board: ESP32-S3-CAM "bare clone" N16R8 (16 MB flash, 8 MB PSRAM) with OV5640
// Motors: DRV8833 (IN1=GPIO1, IN2=GPIO14, IN3=GPIO21, IN4=GPIO42)
// OLED:   SH1106 128x64 over I2C Wire1 (SDA=GPIO35, SCL=GPIO36)
// Camera: OV5640, Freenove/generic clone pinout (GPIO 4-18, minus 14)
//
// See ../USER_MANUAL.md for wiring, build and configuration instructions.

#include "config.h"
#include "settings.h"
#include "motor_control.h"
#include "mochi_eyes.h"
#include "display_manager.h"
#include "wifi_helper.h"
#include "camera_server.h"
#include "web_server.h"

void setup() {
    // Reduce noisy boot logging on the USB serial console.
    esp_log_level_set("*", ESP_LOG_ERROR);

    settings.begin();        // NVS settings (wifi, token, camera, flash)
    motors.begin();          // LEDC PWM on GPIO1/14/21/42
    displayMgr.begin();      // SH1106 OLED (Wire1) + boot screen

    wifiHelper.begin();      // connect to saved network or start config AP
    displayMgr.setConnected(wifiHelper.isConnected(), wifiHelper.isApMode(),
                            wifiHelper.ip().c_str());

    cameraServer.begin();    // OV5640; harmless if it fails

    webServerMgr.begin();    // REST API + web UI + MJPEG stream

    // Refresh OLED connection state now that networking is settled.
    displayMgr.setConnected(wifiHelper.isConnected(), wifiHelper.isApMode(),
                            wifiHelper.ip().c_str());
}

void loop() {
    wifiHelper.update();
    displayMgr.update(millis());
    motors.update();
    delay(10);
}
