// MochiRover - ESP32-CAM smart rover with Dasai Mochi-style OLED eyes
//
// Board: AI Thinker ESP32-CAM (SD disabled, PSRAM enabled)
// Motors: DRV8833 (IN1=GPIO3, IN2=GPIO1, IN3=GPIO13, IN4=GPIO12)
// OLED:   SH1106 128x64 over I2C (SDA=GPIO14, SCL=GPIO15)
// Camera: OV2640, standard AI Thinker pins
//
// NOTE: GPIO1/GPIO3 are U0TXD/U0RXD. They are re-purposed as LEDC PWM
// outputs here, so UART0 must never be used for Serial debug output.
//
// See README.md for wiring, build and configuration instructions.

#include "config.h"
#include "settings.h"
#include "motor_control.h"
#include "mochi_eyes.h"
#include "display_manager.h"
#include "wifi_helper.h"
#include "camera_server.h"
#include "web_server.h"

void setup() {
    // Reduce noisy boot logging to UART0 (which is wired to the motors).
    esp_log_level_set("*", ESP_LOG_ERROR);

    settings.begin();        // NVS settings (wifi, token, camera, flash)
    motors.begin();          // LEDC PWM on GPIO1/3/12/13 (takes over UART0)
    displayMgr.begin();      // SH1106 OLED + boot screen

    wifiHelper.begin();      // connect to saved network or start config AP
    displayMgr.setConnected(wifiHelper.isConnected(), wifiHelper.isApMode(),
                            wifiHelper.ip().c_str());

    cameraServer.begin();    // OV2640; harmless if it fails

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
