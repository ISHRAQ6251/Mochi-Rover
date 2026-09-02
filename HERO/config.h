#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Board / wiring configuration (user-confirmed)
// ---------------------------------------------------------------------------
// ESP32-S3-CAM "bare clone" (N16R8 = 16 MB flash, 8 MB OPI PSRAM) with an
// OV5640 camera on the ribbon/FPC connector. Camera pinout is the well-known
// Freenove / generic layout (cross-confirmed by jzsalinas/exp-esp32s3 and
// sorn-AI/ESP32_WEB_Camera). Camera occupies GPIO 4-18 (except 14).

// OV5640 parallel camera
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D7      16   // Y9
#define CAM_PIN_D6      17   // Y8
#define CAM_PIN_D5      18   // Y7
#define CAM_PIN_D4      12   // Y6
#define CAM_PIN_D3      10   // Y5
#define CAM_PIN_D2      8    // Y4
#define CAM_PIN_D1      9    // Y3
#define CAM_PIN_D0      11   // Y2
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

// DRV8833 dual motor driver
//   Motor A (left):  IN1 = GPIO1,  IN2 = GPIO14
//   Motor B (right): IN3 = GPIO21, IN4 = GPIO42
#define PIN_MOTOR_L_IN1  1
#define PIN_MOTOR_L_IN2  14
#define PIN_MOTOR_R_IN3  21
#define PIN_MOTOR_R_IN4  42

// SH1106 OLED over I2C Wire (I2C port 0). Camera SCCB uses I2C port 1 on
// GPIO 4/5, so the two buses must not share a port. Do NOT use GPIO 33-37:
// on the N16R8 those are OPI PSRAM (SPIIO4-7 / DQS) and watchdog-reset the chip.
#define PIN_OLED_SDA     40
#define PIN_OLED_SCL     41

// On-board flashlight LED (high = on). On this board an LED is wired to GPIO2;
// if your board lacks it, wire an external LED to GPIO2 through a resistor.
#define PIN_FLASH_LED    2

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_I2C_ADDR    0x3C

// ---------------------------------------------------------------------------
// Motor PWM (LEDC)
// ---------------------------------------------------------------------------
#define MOTOR_PWM_FREQ    20000   // DRV8833 supports up to 50 kHz
#define MOTOR_PWM_RES     8       // 8-bit -> 0..255 matches UI speed slider

// ---------------------------------------------------------------------------
// Wi-Fi / networking
// ---------------------------------------------------------------------------
#define HOSTNAME          "hero"
#define AP_SSID           "HERO"
#define AP_PASS           "hero1234"
#define HTTP_PORT         80

// ---------------------------------------------------------------------------
// Auth
// ---------------------------------------------------------------------------
#define AUTH_DEFAULT_TOKEN "hero"

// ---------------------------------------------------------------------------
// NVS
// ---------------------------------------------------------------------------
#define NVS_NAMESPACE     "hero"
#define NVS_KEY_TOKEN     "auth_token"
#define NVS_KEY_RES       "cam_res"
#define NVS_KEY_QUALITY   "cam_quality"
#define NVS_KEY_FLIP      "cam_flip"
#define NVS_KEY_FLASH     "flash_on"
#define NVS_KEY_OLEDANIM  "oled_anim"
#define NVS_KEY_REV_L     "rev_left"
#define NVS_KEY_REV_R     "rev_right"
#define NVS_KEY_TRIM_L    "trim_l"
#define NVS_KEY_TRIM_R    "trim_r"
#define MOTOR_TRIM_MIN    50
#define MOTOR_TRIM_MAX    100

// ---------------------------------------------------------------------------
// Behavioural timings
// ---------------------------------------------------------------------------
#define SLEEP_TIMEOUT_MS  60000UL     // 60 s inactivity before eye sleep mode
#define DRIVE_WATCHDOG_MS 1500UL      // stop motors if no drive/stop cmd in this window
#define MOOD_OVERRIDE_MS  4000UL      // mood override lasts ~4 s, then idle
