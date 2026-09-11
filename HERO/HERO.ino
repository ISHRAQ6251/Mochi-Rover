// HERO - ESP32-S3-CAM rover with Dasai Mochi-style OLED eyes
//
// HERO (Hazardous Environment Reconnaissance & Observation robot) is a
// Wi-Fi-controlled rover streaming live video to a smartphone browser while
// an animated robot face runs on an OLED.
//
// Board: ESP32-S3-CAM "bare clone" N16R8 (16 MB flash, 8 MB PSRAM) with OV5640
// Motors: DRV8833 (IN1=GPIO47, IN2=GPIO14, IN3=GPIO21, IN4=GPIO42)
// OLED:   SH1106 128x64 over I2C Wire (SDA=GPIO40, SCL=GPIO41)
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

    settings.begin();        // NVS settings (token, camera, flash, motor reverse)
    Serial.println("settings ok");

    motors.begin();          // LEDC PWM on GPIO47/14/21/42 (safe with motors unplugged)
    Serial.println("motors ok");

    if (displayMgr.begin()) {
        Serial.printf("oled ok (SDA=%d SCL=%d addr=0x%02X)\n",
                      PIN_OLED_SDA, PIN_OLED_SCL, OLED_I2C_ADDR);
    } else {
        Serial.printf("oled missing (SDA=%d SCL=%d) - rest of rover still runs\n",
                      PIN_OLED_SDA, PIN_OLED_SCL);
    }

    wifiHelper.begin();      // SoftAP HERO / hero1234 at 192.168.4.1
    Serial.printf("wifi AP ip=%s\n", wifiHelper.ip().c_str());
    displayMgr.setConnected(true, true, wifiHelper.ip().c_str());

    if (cameraServer.begin()) {
        Serial.println("camera ok");
    } else {
        Serial.println("camera init failed - stream unavailable");
    }

    webServerMgr.begin();    // REST API + web UI + MJPEG stream
    Serial.println("web server ok");

    // Refresh OLED connection state now that networking is settled.
    displayMgr.setConnected(true, true, wifiHelper.ip().c_str());
    Serial.println("HERO ready");
}

// Bring-up test: press 1-4 on the serial monitor to drive one motor input
// (1=L_IN1/GPIO47, 2=L_IN2/GPIO14, 3=R_IN3/GPIO21, 4=R_IN4/GPIO42), q to stop.
// Bypasses trim/reverse/watchdog and auto-coasts after MOTOR_TEST_MS.
static void motorPinTest() {
    if (!Serial.available()) return;
    int c = Serial.read();
    switch (c) {
        case '1': case '2': case '3': case '4': {
            uint8_t index = (uint8_t)(c - '1');
            uint32_t duty = motors.testPin(index, 200);
            Serial.printf("test pin %u duty=%u\n", (unsigned)index, (unsigned)duty);
            break;
        }
        case 'q':
            motors.testPin(0, 0);
            Serial.println("test stop");
            break;
    }
}

void loop() {
    wifiHelper.update();
    displayMgr.update(millis());
    motors.update();
    motorPinTest();
    delay(10);
}
