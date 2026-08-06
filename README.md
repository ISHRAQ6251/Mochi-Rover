# MochiRover

An **ESP32-CAM smart rover** with a living, animated **Dasai Mochi**-style face
on an SH1106 OLED, **live MJPEG video**, a DRV8833 dual-motor driver, and a
responsive, token-protected web cockpit that works on phones and desktops.

![Platform](https://img.shields.io/badge/platform-ESP32--CAM-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B11-green)
![IDE](https://img.shields.io/badge/IDE-Arduino%20CLI-orange)
![Video](https://img.shields.io/badge/video-MJPEG-red)
![Display](https://img.shields.io/badge/display-SH1106-8A2BE2)

Drive it from a browser, watch the rover's face blink and react while it moves,
send it messages it displays on its chest, and flip on its flashlight —
all over Wi-Fi, with no app required.

---

## Table of contents

- [Features](#features)
- [Hardware & wiring](#hardware--wiring)
- [How it works](#how-it-works)
- [Getting started](#getting-started)
- [First boot & Wi-Fi setup](#first-boot--wi-fi-setup)
- [Using the web cockpit](#using-the-web-cockpit)
- [REST API](#rest-api)
- [The animated face](#the-animated-face)
- [Project structure](#project-structure)
- [Customization & development](#customization--development)
- [Troubleshooting](#troubleshooting)
- [FAQ](#faq)
- [Security notes](#security-notes)
- [Known limitations](#known-limitations)
- [Credits](#credits)
- [License](#license)

---

## Features

### Driving
- **Arcade-style dual joysticks**: a STEERING cross and a THROTTLE pad with
  differential mixing on the DRV8833 dual motor driver.
- **Speed slider** (0-255) with integer snapping and haptic feedback on
  supported phones (`navigator.vibrate`).
- **Emergency STOP** button plus a firmware-side motor safety timeout that
  brakes the motors automatically if no drive command arrives.

### Video
- **Live MJPEG stream** from the OV2640 camera at up to 1280x960 (resolution,
  JPEG quality and frame rate configurable).
- **Single-frame snapshot** endpoint.

### The face
- Fully **procedural Dasai Mochi-style animated eyes** on the SH1106 OLED.
- **Autonomous idle personality**: while sitting still the rover blinks, looks
  around, and spontaneously shifts through happy, curious, surprised,
  suspicious, doze-off and wink expressions.
- **Driving-reactive**: eyes light up with an excitement kick when you hit the
  throttle, narrow as speed builds, and pupils steer into turns.
- **12 moods** you can trigger from the cockpit.
- **Sleep mode**: after 10 minutes of inactivity the eyes close and animated
  "Zzz" float up; any input wakes it instantly.
- **Persistent text box**: send a message and the eyes shrink and rise to the
  top of the display while a rounded-corner text box shows your message — it
  stays until you press the dedicated **Clear** button.

### Connectivity
- **Wi-Fi config portal**: no saved network? The rover becomes an access point
  (`MochiRover`, `192.168.4.1`) and the cockpit's settings panel lets you
  configure your Wi-Fi. Credentials are stored in NVS.
- **mDNS**: reachable at `http://mochirover.local`.
- **Token-based auth** on all control endpoints (default token: `mochi`).

### Web cockpit
- Auth gate, dark/light theme, responsive layout (mobile + desktop).
- Mood rows, message box with Clear button, flashlight toggle, OLED-animation
  toggle, and a settings popup for Wi-Fi, theme, camera and access token.
- Live state polling drives status chips, mood highlight and the
  message-active indicator.

---

## Hardware & wiring

### Required parts

| Part | Notes |
| ---- | ----- |
| ESP32-CAM | AI Thinker board (OV2640), SD card **disabled** in firmware |
| DRV8833 | Dual motor driver |
| 2x DC motor | Small rover drive motors |
| SH1106 OLED | 128x64, I2C, 0x3C address |
| USB-TTL adapter | 3.3V logic, for flashing only |
| Li-Po / battery pack | Rover + motor supply |

### Pinout

| Part | Signal | GPIO |
| ---- | ------ | ---- |
| DRV8833 | IN1 (Motor A) | GPIO3 |
| DRV8833 | IN2 (Motor A) | GPIO1 |
| DRV8833 | IN3 (Motor B) | GPIO13 |
| DRV8833 | IN4 (Motor B) | GPIO12 |
| SH1106 OLED | SDA | GPIO14 |
| SH1106 OLED | SCL | GPIO15 |
| Flash LED | (on-board) | GPIO4 |
| Camera | Standard AI Thinker OV2640 pinout | - |

Power notes:
- Motor power comes from the DRV8833 `VM` pin.
- The OLED module in this build carries a note *"make sure the jumper is set to
  5V"* — power the OLED logic per your module's instructions.
- Keep motor and logic grounds common.

> **UART0 caveat**: GPIO1 and GPIO3 are also U0TXD/U0RXD. This firmware
> re-purposes them as LEDC PWM outputs for the DRV8833, so **`Serial`/UART0
> must never be used for debug output**. The USB-TTL adapter drives these same
> pins while flashing, so keep the rover powered off — or at least the wheels
> off the ground — during upload. **Never drive the motors while flashing.**

> **Strapping pins**: GPIO12 (MTDI) and GPIO13 (MTCK) have boot-time pull
> behavior. They are configured as PWM outputs after boot, which is safe.

---

## How it works

A single firmware image runs five cooperating subsystems, orchestrated by
`setup()` / `loop()`:

1. **Web + camera** run in ESP-IDF background tasks (ESPAsyncWebServer): frames
   stream out of the OV2640 as MJPEG while the REST API serves the cockpit.
2. **The main loop** (every 10 ms) drives the Wi-Fi reconnect timer, the
   display scheduler and the motor safety timeout.
3. **The display scheduler** picks one screen per frame with strict priority:
   `boot -> connection -> text(persistent) -> sleep -> mood/driving -> idle`.
4. **The eyes engine** eases every pose (lids, pupils, brows, mouth) toward
   targets with smooth interpolation.
5. **The settings layer** persists everything (Wi-Fi, token, camera options,
   toggles) in ESP32 NVS.

A full description with state-machine diagrams and task-priority rationale is
in [`FUNCTIONALITY.md`](FUNCTIONALITY.md).

---

## Getting started

### 1. Install the toolchain

**Option A — Arduino IDE (recommended for beginners)**

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) 2.x.
2. Add the ESP32 board URL in *Preferences > Additional boards manager URLs*:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. In *Boards Manager* install **esp32 by Espressif Systems** (v3.x).
4. In *Library Manager* install:
   - **Adafruit GFX Library**
   - **Adafruit SH110X** (pulls in Adafruit BusIO)
   - **ESP Async WebServer** (by ESP32Async)
   - **Async TCP** (by ESP32Async)

**Option B — arduino-cli**

```bash
# Install Arduino CLI, then:
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SH110X" \
  "ESP Async WebServer" "Async TCP"
```

### 2. Board settings

Open the sketch folder `MochiRover/` (the folder name must match the sketch
name) and select the board **`ESP32-CAM`** (in Arduino IDE: `esp32:esp32cam`).

| Option | Value |
| ------ | ----- |
| Flash Mode | QIO |
| Flash Size | 4MB |
| Partition Scheme | Huge APP (3MB No OTA / 1MB SPIFFS) |
| PSRAM | Enabled (default on this profile, required for the camera) |
| SD Card | Disabled |

### 3. Compile & flash

Arduino IDE: click **Verify**, then **Upload** with the USB-TTL adapter wired to
the ESP32-CAM's U0TXD/U0RXD (press the boot button as needed).

arduino-cli:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32cam MochiRover
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam MochiRover
```

Pre-built binaries (`.bin`, `.merged.bin`, `.bootloader.bin`,
`.partitions.bin`) are also generated during compilation under
`dist/` for tools like esptool.

> Reminder: keep the rover powered off / wheels off the ground while flashing.

---

## First boot & Wi-Fi setup

1. Power the rover. The OLED shows a boot screen, then a connection screen.
2. The rover tries the saved network; if none is saved it starts the
   `MochiRover` access point (`mochi1234`, `192.168.4.1`).
3. Open the cockpit in a browser:
   - STA mode: `http://mochirover.local` or the IP shown on the OLED.
   - AP mode: `http://192.168.4.1`.
4. Enter the access token (default `mochi`).
5. Open **Settings** (gear icon) and configure:
   - Your Wi-Fi SSID/password (required in AP mode; saved to NVS and the rover
     reconnects).
   - Camera resolution, JPEG quality, FPS, format.
   - Interface theme (dark/light).
   - A new access token (optional).

---

## Using the web cockpit

| Control | What it does |
| ------- | ------------ |
| STEERING cross | Left pad: steer left/right (arcade mixing) |
| THROTTLE pad | Right pad: forward/backward |
| STOP | Emergency stop |
| Speed slider | 0-255 max throttle, snapping + haptics |
| Mood buttons | Sets the OLED face (12 moods) |
| Message + Send | Shows text in the persistent rounded text box |
| Clear | Removes the message and restores the full-size face |
| Flashlight | Toggles the on-board LED |
| Anim | Toggles OLED animation (idle script, blinking) |
| Settings (gear) | Wi-Fi, camera, theme, token |

---

## REST API

Endpoints are `application/json` / form-encoded POSTs. All endpoints except
`/api/info`, `/api/auth`, `/stream` and `/capture` require the token via the
`X-Auth-Token` header (or a `token` query parameter).

| Endpoint        | Method | Body (form)                          | Purpose                              |
| --------------- | ------ | ------------------------------------ | ------------------------------------ |
| `/api/info`     | GET    | -                                    | Connectivity info (no auth)          |
| `/api/auth`     | POST   | `token`                              | Validate token (no auth)             |
| `/api/state`    | GET    | -                                    | Full rover state (JSON)              |
| `/api/drive`    | POST   | `throttle`, `steering` (-255..255)   | Drive motors                         |
| `/api/stop`     | POST   | -                                    | Emergency stop                       |
| `/api/mood`     | POST   | `mood` (happy/angry/...)             | Set OLED mood                        |
| `/api/message`  | POST   | `text` (empty = clear)               | Show text on OLED (persistent)       |
| `/api/flash`    | POST   | `on` (0/1)                           | Flashlight                           |
| `/api/settings` | POST   | `resolution,quality,fps,format,oledAnim` | Camera + OLED options          |
| `/api/wifi`     | POST   | `ssid`, `pass`                       | Save Wi-Fi + reconnect               |
| `/api/token`    | POST   | `token`                              | Change access token                  |
| `/stream`       | GET    | -                                    | MJPEG live stream (open)             |
| `/capture`      | GET    | -                                    | Single JPEG snapshot (open)          |

Example (drive forward with a token):

```bash
curl -X POST "http://mochirover.local/api/drive" \
  -H "X-Auth-Token: mochi" \
  -d "throttle=200&steering=0"
```

Example (show a message, then clear it):

```bash
curl -X POST "http://mochirover.local/api/message" \
  -H "X-Auth-Token: mochi" -d "text=Hello world"

curl -X POST "http://mochirover.local/api/message" \
  -H "X-Auth-Token: mochi"
```

---

## The animated face

The eyes are drawn **procedurally** with Adafruit_GFX primitives — no bitmap
assets — which keeps the firmware tiny and the animation buttery. The idle
personality is a randomized behavior script: the rover spontaneously picks
look-around, happy, curious, surprise, suspicious, doze-off and wink poses
every 0.9-2.7 seconds, with blinks every 1.8-4.3 seconds, always easing
smoothly between poses. Explicit moods (via the API or cockpit) temporarily
override the script, and driving always wins while the throttle is held.

Display priority is strict:

```
boot -> connection -> text (persistent) -> sleep -> mood/driving -> idle
```

See [`FUNCTIONALITY.md`](FUNCTIONALITY.md) for the full state-machine detail.

---

## Project structure

```
MochiRover/
  MochiRover.ino          main sketch (setup/loop orchestration)
  config.h                pins, Wi-Fi, timings, NVS keys
  settings.h/.cpp         NVS-backed persistent settings
  motor_control.h/.cpp    DRV8833 + LEDC PWM, differential drive
  mochi_eyes.h/.cpp       procedural Dasai Mochi-style eye engine
  display_manager.h/.cpp  SH1106 + priority display scheduler
  wifi_helper.h/.cpp      station connect / AP config portal / mDNS
  camera_server.h/.cpp    OV2640 init, MJPEG stream, snapshot
  web_server.h/.cpp       REST API, auth, static assets
  web_assets.h            auto-generated embedded web UI (PROGMEM)
  web_ui/                 index.html, style.css, app.js
  tools/embed_web.py      regenerates web_assets.h
  FUNCTIONALITY.md        architecture, state machines, task priorities
```

---

## Customization & development

**Tweak the face**: all geometry and the idle behavior script live in
`mochi_eyes.cpp` (eye size, position, colors, blink/saccade timings, the list
of idle poses and their probability weights).

**Add a mood**: add a value to `MochiMood` in `mochi_eyes.h`, a target pose in
`update()`, a mouth in `render()`, and a mapping in `parseMood()`
(`web_server.cpp`) + a button in `web_ui/index.html`.

**Edit the web UI**: change files in `web_ui/`, then regenerate the embedded
assets header:

```bash
python3 MochiRover/tools/embed_web.py MochiRover/web_ui MochiRover/web_assets.h
```

**Change defaults**: `config.h` holds the AP credentials, default token,
hostname, sleep timeout and pin definitions. `settings.h` defines the NVS
namespace and keys.

---

## Troubleshooting

| Problem | Likely cause / fix |
| ------- | ------------------ |
| Won't flash | Wrong board profile, PSRAM off, or boot pin not held during reset. |
| Motors twitch during flash | USB-TTL shares the motor pins — power the rover off while uploading. |
| No camera image | SD card not disabled, PSRAM disabled, or low-quality WiFi link. |
| Can't reach the cockpit | Use AP mode (`192.168.4.1`) if no network is saved; check the OLED screen. |
| 401 on every request | Wrong token — it is `mochi` unless changed in Settings. |
| No OLED output | Check I2C address (0x3C) and SDA/SCL wiring (GPIO14/GPIO15). |

---

## FAQ

**Why no `Serial` debugging?** GPIO1/GPIO3 (U0TXD/U0RXD) are the DRV8833 inputs
in this design, so UART0 is unavailable. Logs are suppressed; state is
observable through `/api/state`.

**Why does the camera frame rate not change with the FPS setting?** The OV2640
driver bundled with the ESP32 core does not expose a runtime `set_fps`, so the
frame rate is governed by the camera XCLK. The FPS value is persisted and
reported for reference; resolution and quality apply live.

**Only one browser can watch the stream?** The MJPEG packetizer is a single
shared instance serving one stream client at a time; browsers reconnect
automatically when a second client disconnects.

**Do I need PSRAM?** Yes — the ESP32-CAM has no internal frame buffer big
enough for JPEG streaming; the `esp32cam` board profile enables PSRAM by
default.

**Can I run it on a plain ESP32 (no camera)?** The web cockpit and face run,
but video and streaming code require the camera hardware.

---

## Security notes

- The control API is protected by a token that is transmitted over the local
  network. The default token `mochi` should be changed in Settings.
- This is designed for **trusted local networks**. It is not hardened for
  internet exposure — do not port-forward it to the public internet.
- The MJPEG stream and snapshot endpoints are intentionally unauthenticated so
  the live view loads in `<img>` tags; consider this when choosing your network.

---

## Known limitations

- Single stream client at a time (shared MJPEG packetizer).
- No OTA update support (Huge APP partition scheme).
- No SD-card logging.
- FPS setting is advisory only (see FAQ).

---

## Credits

The animated eyes are inspired by the open-source **Dasai Mochi** / Datasai
OLED projects, in particular [`upiir/esp32s3_oled_dasai_mochi`](https://github.com/upiir/esp32s3_oled_dasai_mochi)
and [`NeoDoggy/BetterMochi`](https://github.com/NeoDoggy/BetterMochi). Those
projects ship monolithic, bitmap-based sketches without a reusable library API,
so this firmware draws the eyes procedurally with Adafruit_GFX primitives
instead of embedding their bitmaps. Camera streaming follows the classic
Espressif ESP32-CAM `CameraWebServer` approach. Built on the excellent
`esp32-camera`, ESPAsyncWebServer and Adafruit SH110X/GFX libraries.

---

## License

This project is released under the **MIT License**. See the
[`LICENSE`](LICENSE) file for details.
