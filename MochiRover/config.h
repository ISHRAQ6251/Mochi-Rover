#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Board / wiring configuration (user-confirmed)
// ---------------------------------------------------------------------------

// DRV8833 dual motor driver
//   Motor A (left):  IN1 = GPIO3, IN2 = GPIO1
//   Motor B (right): IN3 = GPIO13, IN4 = GPIO12
#define PIN_MOTOR_L_IN1  3
#define PIN_MOTOR_L_IN2  1
#define PIN_MOTOR_R_IN3  13
#define PIN_MOTOR_R_IN4  12

// SH1106 OLED over I2C
#define PIN_OLED_SDA     14
#define PIN_OLED_SCL     15

// On-board flashlight LED (high = on)
#define PIN_FLASH_LED    4

// ---------------------------------------------------------------------------
// AI Thinker ESP32-CAM camera pin mapping (SD disabled)
// ---------------------------------------------------------------------------
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

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
#define HOSTNAME          "mochirover"
#define AP_SSID           "MochiRover"
#define AP_PASS           "mochi1234"
#define HTTP_PORT         80

// ---------------------------------------------------------------------------
// Auth
// ---------------------------------------------------------------------------
#define AUTH_DEFAULT_TOKEN "mochi"

// ---------------------------------------------------------------------------
// NVS
// ---------------------------------------------------------------------------
#define NVS_NAMESPACE     "mochirover"
#define NVS_KEY_SSID      "wifi_ssid"
#define NVS_KEY_PASS      "wifi_pass"
#define NVS_KEY_TOKEN     "auth_token"
#define NVS_KEY_RES       "cam_res"
#define NVS_KEY_QUALITY   "cam_quality"
#define NVS_KEY_FPS       "cam_fps"
#define NVS_KEY_FORMAT    "cam_format"
#define NVS_KEY_FLASH     "flash_on"
#define NVS_KEY_OLEDANIM  "oled_anim"

// ---------------------------------------------------------------------------
// Behavioural timings
// ---------------------------------------------------------------------------
#define SLEEP_TIMEOUT_MS  600000UL    // 10 min inactivity before eye sleep mode
#define STREAM_TIMEOUT_MS 10000UL     // max time to wait for a camera frame
#define MOTOR_STOP_DELAY_MS 300UL     // coast time after a stop command
