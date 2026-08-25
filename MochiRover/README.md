# MochiRover

> **New to the project? Start with the [User Manual](USER_MANUAL.md)** - it
> covers the parts list, wiring, code upload and how to drive the rover.

Wi-Fi controlled RC rover for a university microcontroller lab project.

An ESP32-S3-CAM module streams live video over Wi-Fi to a phone browser while a
little "Mochi" robot face with animated eyes runs on an OLED. The browser is a
password-gated cockpit: differential drive pads, a speed slider, photo + video
clip capture, a flashlight toggle, six mood buttons and a message-to-OLED box.

![board: ESP32-S3-CAM clone + DRV8833 + 2x N20 + SH1106 OLED]

## Hardware

| Part | Notes |
| ---- | ----- |
| ESP32-S3-CAM (bare clone, N16R8) | 16 MB flash, 8 MB OPI PSRAM, **OV5640** camera on the FPC/ribbon connector |
| DRV8833 dual H-bridge | 2.7-10.8 V, up to 50 kHz PWM |
| 2x N20 / TT DC gear motors | differential (skid) steering |
| SH1106 1.3" OLED 128x64 | I2C, address `0x3C` |
| Power | see the power section below |

The camera pinout used is the well-known **Freenove / generic ESP32-S3-CAM
layout** (cross-confirmed by the `jzsalinas/exp-esp32s3` and
`sorn-AI/ESP32_WEB_Camera` projects). If your board's silkscreen differs, adjust
the `CAM_*` pins in `config.h`; the firmware prints the detected sensor PID on
boot to help you spot a mismatch.

## Wiring

| Signal | GPIO | Connects to |
| ------ | ---- | ----------- |
| OV5640 D0-D7 | 11, 9, 8, 10, 12, 18, 17, 16 | camera FPC (Y2..Y9) |
| OV5640 XCLK / PCLK | 15 / 13 | camera FPC |
| OV5640 VSYNC / HREF | 6 / 7 | camera FPC |
| OV5640 SIOD / SIOC (SCCB) | 4 / 5 | camera FPC |
| DRV8833 IN1 / IN2 (motor A) | 1 / 14 | DRV8833 AIN1/AIN2 |
| DRV8833 IN3 / IN4 (motor B) | 21 / 42 | DRV8833 BIN1/BIN2 |
| OLED SDA / SCL | 35 / 36 | SH1106 SDA/SCL |
| Flashlight LED | 2 | on-board LED (or external LED via resistor) |

Notes:

- The camera uses GPIO 4-18 (except 14) on the ribbon connector.
- The OLED uses `Wire1` (pins 35/36) so it stays off the camera SCCB bus (4/5).
- GPIO 19/20 are the USB pins, GPIO 0 is the BOOT button, GPIO 43/44 are UART0
  (left free for a serial debug header). GPIO 3/45/46 are strapping pins - avoid.
- DRV8833 `nSLEEP` and `VMODE` are tied to `3V3` (or the motor supply for
  VMODE); the four `IN` pins carry PWM directly. If you prefer a hardware
  disable, wire DRV8833 `nSLEEP` to a spare GPIO and add it to `motor_control`.

### Power

- The ESP32-S3-CAM runs from USB (5 V) or from a regulated 5 V / 3.3 V supply.
- The DRV8833 and motors should use a **separate battery pack**
  (2x 18650 or 4x AA gives about 6 V). The DRV8833 accepts 2.7-10.8 V.
- Do not draw motor current from the ESP32's regulator; keep the logic ground
  common. Two N20s stall around 2-3 A combined, which a 5 V/3.3 V regulator on
  the module cannot deliver.

## Build & Flash

Requirements: [arduino-cli](https://arduino.github.io/arduino-cli/) with the
`esp32:esp32` core (3.x) and these libraries:

- ESPAsyncWebServer (3.x) + AsyncTCP
- Adafruit GFX Library, Adafruit SH110X (+ Adafruit BusIO)
- The ESP32 core already bundles the precompiled `esp32-camera` driver, so no
  separate camera library install is required.

If your ESPAsyncWebServer version fails to build against the newer mbedTLS
(`mbedtls_md5_update_ret ... not declared`), replace those calls in
`WebAuthentication.cpp` with the non-`_ret` variants (`mbedtls_md5_starts`,
`mbedtls_md5_update`, `mbedtls_md5_finish`).

Compile:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi MochiRover
```

The FQBN options are important: `FlashSize=16M` (the N16R8 has 16 MB flash) and
`PSRAM=opi` (8 MB OPI PSRAM, required for camera frame buffers).

Upload over USB:

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi MochiRover
```

Put the board in download mode if needed (hold BOOT, tap RST, release BOOT).

## Usage

1. Power the rover. If it has no saved network it starts a SoftAP named
   **MochiRover** (password `mochi1234`).
2. Connect your phone to that network, open `http://192.168.4.1` and enter your
   Wi-Fi SSID/password. The rover reboots into station mode on your network.
3. Open `http://mochirover.local` (or the rover IP) and unlock with the access
   token (default **`mochi`**).
4. Drive with the ◀ ▶ ▲ ▼ buttons (hold to move, release to stop), set speed
   with the slider, change the mood with 👀, flip/refresh or capture from the
   video panel, and send a message to the OLED.

Power-cycling the rover returns it to setup mode if it cannot reach the saved
network. Change the access token in the gear settings panel.

## Project layout

```
MochiRover.ino         main sketch
config.h               pins + constants
settings.h/.cpp        NVS-persisted settings
motor_control.h/.cpp   DRV8833 PWM driver
mochi_eyes.h/.cpp      procedural animated OLED face (6 moods)
display_manager.h/.cpp OLED state machine + screens
wifi_helper.h/.cpp     STA-first, AP config portal, mDNS
camera_server.h/.cpp   OV5640 MJPEG stream + snapshot
web_server.h/.cpp      REST API + static asset server
web_ui/                browser UI (index.html, style.css, app.js)
web_assets.h           generated PROGMEM copy of web_ui (run tools/embed_web.py)
tools/embed_web.py     embeds web_ui into web_assets.h
```

See `FUNCTIONALITY.md` for the full feature and API reference.
