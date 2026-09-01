# HERO

Wi-Fi controlled RC rover for a university microcontroller lab project.

An ESP32-S3-CAM module streams live video over Wi-Fi to a phone browser while a
little "HERO" robot face with animated eyes runs on an OLED. The browser is a
password-gated cockpit: differential drive pads, a speed slider, photo + video
clip capture, a flashlight toggle, six mood buttons and a message-to-OLED box.

![board: ESP32-S3-CAM clone + DRV8833 + 2x N20 + SH1106 OLED]

> **Building or using the rover?** Start with the
> [User Manual](USER_MANUAL.md) - it has the full parts list, step-by-step
> wiring, code upload instructions and the complete driving guide.
> The [Functionality Reference](HERO/FUNCTIONALITY.md) documents every
> API endpoint, the OLED state machine and the face engine.

## Features

- **Live MJPEG video** streamed to the phone (SVGA 800x600 default, PSRAM
  frame buffers, low-latency grab mode).
- **Photo & clip capture**: stills at the live stream size (SVGA default) and
  client-side WebM clips - no SD card required.
- **Differential drive**: hold-to-move steering/throttle pads with arcade
  throttle+steering mixing and a 0-255 speed slider.
- **Auto-stop safety watchdog**: the rover coasts the motors ~1.5 s after the
  last drive command, so a dropped connection or closed tab can never leave it
  driving itself.
- **"HERO" OLED face**: procedural animated eyes (blinks, saccades, idle
  script), six moods, driving reactions and a sleep state; plus a persistent
  message-to-OLED box.
- **Flashlight** toggle on the onboard LED.
- **Access-token auth** (default `hero`), changeable from the settings panel.
- **Own hotspot**: always SoftAP `HERO` / `hero1234`. Phone joins it and opens
  `http://192.168.4.1`. No lab router, no provisioning.

## Hardware

| Part | Notes |
| ---- | ----- |
| ESP32-S3-CAM (bare clone, N16R8) | 16 MB flash, 8 MB OPI PSRAM, **OV5640** camera on the FPC connector |
| DRV8833 dual H-bridge | 2.7-10.8 V, up to 50 kHz PWM |
| 2x N20 / TT DC gear motors | differential (skid) steering |
| SH1106 1.3" OLED 128x64 | I2C, address `0x3C` |

The camera pinout used is the well-known **Freenove / generic ESP32-S3-CAM
layout** (cross-confirmed by the `jzsalinas/exp-esp32s3` and
`sorn-AI/ESP32_WEB_Camera` projects). If your board's silkscreen differs, adjust
the `CAM_*` pins in `config.h`; the firmware prints the detected sensor PID on
boot to help you spot a mismatch.

## Wiring summary

| Signal | GPIO | Connects to |
| ------ | ---- | ----------- |
| OV5640 D0-D7 | 11, 9, 8, 10, 12, 18, 17, 16 | camera FPC (Y2..Y9) |
| OV5640 XCLK / PCLK | 15 / 13 | camera FPC |
| OV5640 VSYNC / HREF | 6 / 7 | camera FPC |
| OV5640 SIOD / SIOC (SCCB) | 4 / 5 | camera FPC |
| DRV8833 IN1 / IN2 (motor A) | 1 / 14 | DRV8833 AIN1/AIN2 |
| DRV8833 IN3 / IN4 (motor B) | 21 / 42 | DRV8833 BIN1/BIN2 |
| OLED SDA / SCL | 40 / 41 | SH1106 SDA/SCL (not 35/36 — those are PSRAM) |
| Flashlight LED | 2 | onboard LED (or external LED + resistor) |

The full wiring checklist, power layout (separate motor battery!) and the
pin-avoidance list are in the [User Manual](USER_MANUAL.md).

## Build & flash

Requirements: [arduino-cli](https://arduino.github.io/arduino-cli/) with the
`esp32:esp32` core (3.x) and these libraries:

- ESPAsyncWebServer (3.x) + AsyncTCP
- Adafruit GFX Library, Adafruit SH110X (+ Adafruit BusIO)
- The ESP32 core bundles the precompiled `esp32-camera` driver, so no separate
  camera library install is required.

```bash
# install core + libs (one time)
arduino-cli core install esp32:esp32
arduino-cli lib install "ESPAsyncWebServer" "AsyncTCP" \
  "Adafruit GFX Library" "Adafruit SH110X" "Adafruit BusIO"

# compile
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi HERO

# upload (board must be in download mode: hold BOOT, tap RST, release BOOT)
arduino-cli upload -p /dev/ttyACM0 \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi HERO
```

`FlashSize=16M` and `PSRAM=opi` are important: the N16R8 has 16 MB flash and
8 MB OPI PSRAM (required for camera buffers). Detailed upload steps, boot-mode
handling and troubleshooting are in the [User Manual](USER_MANUAL.md).

> **Known mbedTLS issue**: if compilation fails with
> `mbedtls_md5_update_ret ... not declared`, remove the `_ret` suffix from the
> MD5 calls in `~/Arduino/libraries/ESPAsyncWebServer/src/WebAuthentication.cpp`.

## Quick usage

1. Power the rover. It starts a Wi-Fi hotspot named **HERO** (password
   **`hero1234`**).
2. Join that network on your phone and open `http://192.168.4.1`.
3. Unlock with the access token (default **`hero`**).
4. Drive with the d-pad, adjust speed, reverse a motor in Settings if it
   spins the wrong way, capture from the video panel, send OLED messages.

See the [User Manual](USER_MANUAL.md) for the full cockpit tour.

## Project proposal

The group project proposal (`Project_Proposal_HERO.docx`) covers the required
course submission format: group member table, title + short description, key
features, equipment list, design considerations, workflow, expected outcome and
timeline, with placeholder boxes for the circuit diagram and 3D chassis render.

## Project layout

```
README.md                 this file
USER_MANUAL.md            parts list, wiring, upload and usage guide
HERO/               Arduino sketch
  HERO.ino          main sketch
  config.h                pins + constants
  settings.h/.cpp         NVS-persisted settings
  motor_control.h/.cpp    DRV8833 PWM driver
  hero_eyes.h/.cpp       procedural animated OLED face (6 moods)
  display_manager.h/.cpp  OLED state machine + screens
  wifi_helper.h/.cpp      SoftAP HERO hotspot + captive DNS + mDNS
  camera_server.h/.cpp    OV5640 MJPEG stream + snapshot
  web_server.h/.cpp       REST API + static asset server
  web_ui/                 browser UI (index.html, style.css, app.js)
  web_assets.h            generated PROGMEM copy of web_ui (tools/embed_web.py)
  tools/embed_web.py      embeds web_ui into web_assets.h
  FUNCTIONALITY.md        API reference + OLED/face behaviour
```

## Repository state

`PROJECT_STATE.md` records the pin/decision/checklist history. The old
OV2640-era build is recoverable from git commit `0cbd870`.
