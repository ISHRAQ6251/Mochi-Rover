# MochiRover

ESP32-CAM smart rover with an animated **Dasai Mochi**-style face on a small
SH1106 OLED, MJPEG live video, a DRV8833 dual motor driver and a responsive
authenticated web control cockpit.

## Features

- **Live camera**: OV2640 MJPEG stream + single-frame snapshot.
- **Web cockpit** (mobile + desktop):
  - Auth gate with a token (default `mochi`, changeable in settings).
  - **STEERING** cross joystick and **THROTTLE** joystick with arcade-style
    differential mixing.
  - **Speed slider** 0-255 with integer snapping + haptic feedback
    (`navigator.vibrate`).
  - **Mood row** (Happy, Angry, Curious, Dead, Sleepy, Wink + Sad, Crying,
    Confused, Love, Blink, Idle) sent straight to the OLED face.
  - **Message to OLED**: persistent rounded-corner text box. When a message is
    shown, the eyes shrink and rise to the top of the display; the message stays
    until the dedicated **Clear** button on the cockpit is pressed.
  - **Flashlight** toggle (on-board LED) and **OLED animation** toggle.
  - **Emergency STOP** button.
  - Connection settings popup: rover IP/domain, target Wi-Fi SSID/password,
    interface theme, camera resolution/quality/frame-rate/format and access
    token.
  - Dark/light theme.
- **Dasai Mochi eyes** (procedural): authentic idle behavior script (autonomous
  gaze shifts, blinking, and spontaneous happy/curious/surprise/suspicious/
  doze/wink expressions, mirroring the feel of the upstream Rive-rendered idle
  loop), driving-reactive expressions, all moods, sleep mode with animated "Zzz"
  after prolonged inactivity, and a strict display priority:
  `boot -> connection -> text(persistent) -> sleep -> mood/driving -> idle`.
- **Wi-Fi config portal**: if no saved network is found the rover starts an
  access point (`MochiRover` / `mochi1234`, `192.168.4.1`) and the cockpit's
  settings panel is used to configure the target Wi-Fi. Credentials are stored
  in NVS.

## Hardware / Wiring

| Part                | Pin / note                                     |
| ------------------- | ---------------------------------------------- |
| ESP32-CAM           | AI Thinker board, SD card disabled             |
| DRV8833 IN1 (Motor A) | GPIO3                                        |
| DRV8833 IN2 (Motor A) | GPIO1                                        |
| DRV8833 IN3 (Motor B) | GPIO13                                       |
| DRV8833 IN4 (Motor B) | GPIO12                                       |
| SH1106 OLED SDA     | GPIO14                                         |
| SH1106 OLED SCL     | GPIO15                                         |
| Flash LED           | GPIO4                                          |
| Camera              | Standard AI Thinker OV2640 pinout              |

Motor driver and OLED logic power should be supplied as your wiring diagram
indicates (the OLED module note says *"make sure the jumper is set to 5V"*).
Motor supply comes from the DRV8833 VM pin.

> **UART0 caveat**: GPIO1 and GPIO3 are also U0TXD/U0RXD. This firmware
> re-purposes them as LEDC PWM outputs for the DRV8833, so `Serial`/UART0 must
> never be used for debug output. When flashing, the USB-TTL adapter drives
> these pins; keep the rover powered off or the wheels off the ground while
> uploading. **Do not drive the motors while flashing.**

> **Strapping pins**: GPIO12 (MTDI) and GPIO13 (MTCK) have boot-time pull
> behavior. They are configured as PWM outputs after boot, which is safe.

## Build & Upload

The sketch is a standard Arduino `.ino` project located in the `MochiRover/`
folder (the folder name must match the sketch name). Tested with the
`esp32:esp32` core (v3.x) using the `esp32:esp32:esp32cam` board profile with
**PSRAM enabled** (required for the camera) and SD card disabled.

Dependencies (install via the Arduino Library Manager):

- `Adafruit GFX Library`
- `Adafruit SH110X` (pulls in `Adafruit BusIO`)
- `ESP Async WebServer` (by ESP32Async)
- `Async TCP` (by ESP32Async)
- `esp32-camera` (bundled with the ESP32 core / espressif component)

With arduino-cli:

```bash
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SH110X" \
  "ESP Async WebServer" "Async TCP"

arduino-cli compile --fqbn esp32:esp32:esp32cam MochiRover
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam MochiRover
```

Board options used: **Flash Mode QIO**, **Flash Size 4MB**, **Partition Scheme
Huge APP** (default for this board profile). PSRAM is enabled by default on the
`esp32cam` board profile (required for the camera).

## First boot / configuration

1. Power the rover. The OLED shows a boot screen, then a connection screen.
2. The rover either joins the saved network or starts the `MochiRover` AP.
3. Open the cockpit in a browser:
   - STA mode: `http://mochirover.local` or the IP shown on the OLED.
   - AP mode: `http://192.168.4.1`.
4. Enter the access token (default `mochi`).
5. In **Gear -> Connection Settings** set your Wi-Fi SSID/password (AP mode),
   camera options and optionally a new token.

## Project layout

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
```

### Regenerating the embedded web UI

```bash
python3 MochiRover/tools/embed_web.py MochiRover/web_ui MochiRover/web_assets.h
```

## REST API

All endpoints except `/api/info`, `/api/auth`, `/stream` and `/capture` require
the token via the `X-Auth-Token` header (or a `token` query parameter).

| Endpoint            | Method | Body (form)                          | Purpose                          |
| ------------------- | ------ | ------------------------------------ | -------------------------------- |
| `/api/info`         | GET    | -                                    | connectivity info (no auth)      |
| `/api/auth`         | POST   | `token`                              | validate token (no auth)         |
| `/api/state`        | GET    | -                                    | full rover state                 |
| `/api/drive`        | POST   | `throttle`, `steering` (-255..255)   | drive motors                     |
| `/api/stop`         | POST   | -                                    | emergency stop                   |
| `/api/mood`         | POST   | `mood` (happy/angry/... )            | set OLED mood                    |
| `/api/message`      | POST   | `text` (empty = clear)               | show text on OLED (persistent until cleared) |
| `/api/flash`        | POST   | `on` (0/1)                           | flashlight                       |
| `/api/settings`     | POST   | `resolution,quality,fps,format,oledAnim` | camera + OLED options       |

> Note: `fps` is persisted and reported by the API/UI, but the OV2640 driver
> bundled with the ESP32 core does not expose a runtime `set_fps`, so the frame
> rate is governed by the configured XCLK. Resolution and quality apply live.

> Note: the MJPEG stream packetizer is a single shared instance, so it serves
> one active stream client at a time (the browser reconnects automatically).
| `/api/wifi`         | POST   | `ssid`, `pass`                       | save Wi-Fi + reconnect           |
| `/api/token`        | POST   | `token`                              | change access token              |
| `/stream`           | GET    | -                                    | MJPEG live stream (open)         |
| `/capture`          | GET    | -                                    | single JPEG snapshot (open)      |

## Credits

The animated eyes are inspired by the open-source **Dasai Mochi** / Datasai
OLED projects, in particular `upiir/esp32s3_oled_dasai_mochi` and
`NeoDoggy/BetterMochi`. Those projects are monolithic, bitmap-based sketches
without a reusable library API, so this firmware draws the eyes procedurally
with Adafruit_GFX primitives rather than embedding their bitmaps. Camera
streaming follows the classic Espressif ESP32-CAM `CameraWebServer` approach.
