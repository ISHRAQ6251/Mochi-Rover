# Project State

## Summary

Wi-Fi controlled RC rover for a university microcontroller lab project. Hardware:
ESP32-S3-CAM module (**cheap bare clone**, N16R8 = 16 MB flash / 8 MB OPI PSRAM,
OV5640 camera on ribbon/FPC), DRV8833 dual H-bridge, 2x N20 DC gear motors
(differential/skid steering), 1.3" I2C SH1106 OLED (addr 0x3C). A smartphone-
oriented web UI (password-gated, default `mochi`) provides an MJPEG live stream,
forward/back/turn controls with a speed slider, still/clip capture (browser-side
MediaRecorder), a flashlight toggle (on-board LED, GPIO2), mood buttons (6 moods),
and a message-to-OLED text box. The OLED runs a state-driven animated "Mochi"
character face (boot/connect -> idle -> driving -> mood override -> sleep -> text).

Built from scratch per the project brief; the previous repo (AI Thinker ESP32-CAM
with OV2640) was deleted as instructed and remains in git commit `0cbd870`.

## Finalized Decisions

### Board identity (CONFIRMED by user)

Cheap bare "ESP32-S3 N16R8 CAM" clone with OV5640 — the AliExpress item
`1005007307965550` ("ESP32-S3 N16R8 CAM ... OV3660/5640 Camera"). NOT the
Waveshare AIoT board. Camera pinout cross-confirmed by two independent GitHub
projects that run this exact board: `jzsalinas/exp-esp32s3` and
`sorn-AI/ESP32_WEB_Camera` (both identify it as "Freenove / Generic
ESP32-S3-WROOM-1-N16R8"). Both agree on identical OV5640 wiring.

### Pinout (LOCKED)

| Part | Signal | GPIO |
| ---- | ------ | ---- |
| OV5640 | D0..D7 (Y2,Y3,Y4,Y5,Y6,Y7,Y8,Y9) | 11,9,8,10,12,18,17,16 |
| OV5640 | XCLK / PCLK | 15 / 13 |
| OV5640 | VSYNC / HREF | 6 / 7 |
| OV5640 | SIOD / SIOC (SCCB) | 4 / 5 |
| OV5640 | PWDN / RESET | -1 / -1 |
| On-board LED | flash / status | 2 |
| DRV8833 | IN1 / IN2 (motor A) | 1 / 14 |
| DRV8833 | IN3 / IN4 (motor B) | 21 / 42 |
| SH1106 OLED | SDA / SCL (Wire1, addr 0x3C) | 35 / 36 |
| Reserved | USB D-/D+ (flashing/console) | 19 / 20 |
| Reserved | UART0 TX/RX (header, debug) | 43 / 44 |
| Reserved | BOOT button / strapping | 0 / 3 / 45 / 46 (avoid) |

Camera occupies GPIO 4-18 (except 14). USB 19/20 kept for flashing. All other
module GPIOs free. OLED on Wire1 (35/36) to keep the camera SCCB bus (4/5) clean.

### Architecture

- Arduino framework, esp32 core v3.x, board profile `esp32:esp32:esp32s3`
  (flash 16 MB QIO 80 MHz, PSRAM OPI 80 MHz).
- Modular .ino/.h/.cpp layout, mirroring the old repo's proven split:
  `config.h`, `settings` (NVS), `motor_control`, `mochi_eyes`, `display_manager`,
  `wifi_helper`, `camera_server`, `web_server`, `web_ui/` embedded via generated
  PROGMEM header (`tools/embed_web.py`), `MochiRover.ino`.
- Web server = ESPAsyncWebServer + AsyncTCP (as in old repo) for chunked MJPEG.
- OV5640: PIXFORMAT_JPEG, XCLK 20 MHz, fb in PSRAM, fb_count 2, grab LATEST;
  default stream SVGA (800x600), quality from NVS (default 12), all adjustable.
- OLED face drawn procedurally with Adafruit_GFX + SH1106 I2C (no bitmaps).
- USER-CONFIRMED: captures go to the browser (stills via /capture, clips recorded
  client-side via MediaRecorder); no SD dependency.
- USER-CONFIRMED: AP config portal for Wi-Fi provisioning (no network saved ->
  rover becomes hotspot; UI enters SSID/pass stored in NVS), captive DNS, mDNS
  hostname `mochirover`.
- USER-CONFIRMED: default auth password `mochi`, changeable from settings modal.
- Settings modal has rover IP/domain + optional theme control ONLY; explicitly NO
  Wi-Fi SSID/password fields (provisioning happens only in the AP portal page).
- Drive safety: motors stop when controls are released / on disconnect.

Behavioral defaults chosen (recorded, not asked): auth password default `mochi`
(NVS, changeable from UI); sleep expression after 60 s of no driving input
(SLEEP_TIMEOUT_MS, was 600 s in the old repo — shortened for lab demos); mood
override lasts ~4 s (Wink 1.5 s) then returns to idle; idle blinks every
~1.8-4.3 s; message box persists until cleared; default stream resolution SVGA
800x600.

## Completed

- [x] Read project brief (full content available from git commit `0cbd870`).
- [x] Read every file of the old repository (OV2640-era build) for context.
- [x] Confirmed `PROJECT_STATE.md` did not exist -> treated as new project.
- [x] Deleted all old files per brief (recoverable in git commit `0cbd870`).
- [x] Created this PROJECT_STATE.md.
- [x] User confirmed: AP config portal, browser-download captures, password `mochi`.
- [x] User provided web-UI layout (pasted text from screenshots).
- [x] User confirmed board = cheap bare ESP32-S3-CAM clone (not Waveshare AIoT).
- [x] Camera OV5640 pinout locked via two independent GitHub projects
      (jzsalinas/exp-esp32s3 + sorn-AI/ESP32_WEB_Camera) — identical pinout.
- [x] Free-GPIO map derived; motors on 1/14/21/42, OLED I2C on 35/36, LED on 2.
- [x] Built all firmware modules (config, settings, motor_control, mochi_eyes
      6 moods, display_manager, wifi_helper, camera_server, web_server, web_ui,
      embed tool, main sketch).
- [x] Compiles clean with `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi`
      (1105 KB / 84% of 1.25 MB app partition).
- [x] LEDC conflict resolved: camera XCLK moved to native-IDF channel 5/timer 2
      (invisible to the Arduino LEDC wrapper); motors pinned to Arduino LEDC
      channels 1-4 on timer 0 via `ledcAttachChannel`.
- [x] README.md + FUNCTIONALITY.md written (wiring, build, API, face engine,
      power note about separate motor supply).
- [x] ESPAsyncWebServer 3.1.0 patched for mbedTLS (non-`_ret` MD5 calls);
      note recorded in README for reproducibility.

## In Progress

- Final review pass of web_ui + endpoints; optional runtime smoke checks.
- Docs/UI final tidy; user to verify wiring and board silkscreen at build time.

## Open Questions For The User

- None blocking. (Pin choices are the implementer's per the brief; documented in
  `config.h` and final docs. If the user's board differs from the Freenove-style
  clone pinout, camera pins 4/5/6/7/8/9/10/11/12/13/15/16/17/18 are the items to
  double-check against their board's silkscreen/listing.)

## Known Issues / TODO

- OV5640 ribbon wiring on the user's board must be verified at build time; the
  pinout above is the well-known Freenove/generic N16R8 layout, but cheap clones
  occasionally relabel. Firmware prints the detected sensor PID + a pin map on
  boot so mismatches are easy to diagnose.
- Flashlight toggles the on-board LED on GPIO2 (per sorn-AI repo); if the user's
  board lacks an LED there, docs say to wire an external LED to GPIO2.
- Separate motor supply (2x 18650 / 4x AA ~ 6 V) recommended; see FUNCTIONALITY
  docs for the power architecture.
