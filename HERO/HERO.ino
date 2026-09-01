// HERO - ESP32-S3-CAM rover with Dasai Mochi-style OLED eyes
//
// HERO (Hazardous Environment Reconnaissance & Observation robot) is a
// Wi-Fi-controlled rover streaming live video to a smartphone browser while
// an animated robot face runs on an OLED.
//
// Board: ESP32-S3-CAM "bare clone" N16R8 (16 MB flash, 8 MB PSRAM) with OV5640
// Motors: DRV8833 (IN1=GPIO1, IN2=GPIO14, IN3=GPIO21, IN4=GPIO42)
// OLED:   SH1106 128x64 over I2C Wire1 (SDA=GPIO40, SCL=GPIO41)
// Camera: OV5640, Freenove/generic clone pinout (GPIO 4-18, minus 14)
//
// See ../USER_MANUAL.md for wiring, build and configuration instructions.


#include "config.h"
#include "settings.h"
#include "motor_control.h"
#include "hero_eyes.h"
#include "display_manager.h"
#include "wifi_helper.h"
#include "camera_server.h"
#include "web_server.h"

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(200);
    Serial.println();
    Serial.println("HERO boot");
    Serial.flush();

    // Reduce noisy IDF logging on the USB serial console.
    esp_log_level_set("*", ESP_LOG_ERROR);

    settings.begin();        // NVS settings (wifi, token, camera, flash)
    Serial.println("settings ok");

    motors.begin();          // LEDC PWM on GPIO1/14/21/42 (safe with motors unplugged)
    Serial.println("motors ok");

    if (displayMgr.begin()) {
        Serial.printf("oled ok (SDA=%d SCL=%d addr=0x%02X)\n",
                      PIN_OLED_SDA, PIN_OLED_SCL, OLED_I2C_ADDR);
    } else {
        Serial.printf("oled missing (SDA=%d SCL=%d) - rest of rover still runs\n",
                      PIN_OLED_SDA, PIN_OLED_SCL);
    }

    wifiHelper.begin();      // connect to saved network or start config AP
    Serial.printf("wifi %s ip=%s\n",
                  wifiHelper.isApMode() ? "AP" : "STA",
                  wifiHelper.ip().c_str());
    displayMgr.setConnected(wifiHelper.isConnected(), wifiHelper.isApMode(),
                            wifiHelper.ip().c_str());

    if (cameraServer.begin()) {
        Serial.println("camera ok");
    } else {
        Serial.println("camera init failed - stream unavailable");
    }

    webServerMgr.begin();    // REST API + web UI + MJPEG stream
    Serial.println("web server ok");

    // Refresh OLED connection state now that networking is settled.
    displayMgr.setConnected(wifiHelper.isConnected(), wifiHelper.isApMode(),
                            wifiHelper.ip().c_str());
    Serial.println("HERO ready");
}

void loop() {
    wifiHelper.update();
    displayMgr.update(millis());
    motors.update();
    delay(10);
}
