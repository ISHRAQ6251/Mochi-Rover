# HERO - Functionality Reference

## Overview

HERO is a Wi-Fi controlled rover built on an ESP32-S3-CAM (OV5640, N16R8).
A phone browser provides an MJPEG live view plus a full remote-control cockpit;
the on-board SH1106 OLED runs an animated "HERO" face that reacts to state,
moods, driving and messages.

## Feature list

- Live MJPEG video stream (`/stream`), SVGA 800x600 default, quality configurable
  via NVS; XCLK 20 MHz, PSRAM frame buffers, `CAMERA_GRAB_LATEST` for low latency.
- Browser-side captures:
  - Photo: `/capture` temporarily bumps to UXGA (1600x1200) JPEG, downloaded as
    `hero_<timestamp>.jpg`.
  - Clip: recorded client-side with MediaRecorder (VP9/VP8 WebM) by drawing the
    stream to a hidden canvas; downloaded as `hero_clip.webm`. No SD card used.
- Differential drive: ◀ ▶ ▲ ▼ hold-to-move buttons, speed slider (0-255), stop
  on release, arcade-style throttle + steering mixing on the DRV8833.
- Flashlight toggle driving the on-board LED on GPIO2.
- Mood system: six moods (Happy, Angry, Curious, Dead, Sleepy, Wink) shown both
  in a quick popup (👀) and a dropdown. A mood override lasts ~4 s (Wink 1.5 s)
  then returns to the idle face.
- Message-to-OLED: persistent rounded text box with the animated eyes kept in a
  compact mode above it; cleared with the ✕ button in the UI.
- Animated OLED face: blinking, saccades and an autonomous idle script
  (look-around, surprise, suspicious squint, doze-off, occasional wink), a
  wide-eyed "whoa" kick on drive start, driving-reaction eyes, and a Zzz sleep
  state after 60 s without input.
- Network: STA-first; falls back to a SoftAP configuration portal
  (`HERO` / `hero1234`) with captive DNS; mDNS hostname `hero`.
- Auth: token-gated control API (default token `hero`, changeable in Settings).
  Streaming and the provisioning endpoint in AP mode are intentionally open.

## Web UI

Smartphone-first dark UI with a light theme option (saved in the browser):

- Header: robot wordmark "🤖 HERO", a status pill (red/green CTRL dot for
  connection, CAM dot for live feed), and 🔦 / 👀 / ⚙️ buttons.
- Rounded video panel with overlay buttons: 🔄 flip/refresh (mirror+vflip the
  image and reload the stream), 📷 capture photo, ● record clip (turns ⏹ while
  recording, REC badge shown).
- One row of four square control buttons: ◀ ▶ (STEERING) and ▲ ▼ (THROTTLE).
- Bottom card: "Message to OLED..." input with inline Send (+ ✕ to clear), a
  Mood dropdown, and a Speed slider with a live value.
- Mood popup: a white rounded card under the header with the six moods.
- Settings modal "Rover Connection & Settings": Rover IP / Domain field
  (default `192.168.4.1`), theme selector, and an optional new access token.
  It intentionally has no Wi-Fi SSID/password fields; first-time provisioning
  happens on the separate setup page shown while the rover is in AP mode.

## API

All control endpoints (except `/api/info`, `/api/auth`, `/api/wifi` in AP mode,
`/stream` and `/capture`) require the token either as an `X-Auth-Token` header
or a `?token=` query parameter.

| Endpoint | Method | Auth | Body params | Description |
| -------- | ------ | ---- | ----------- | ----------- |
| `/` `/style.css` `/app.js` | GET | - | - | Static UI |
| `/stream` | GET | - | - | MJPEG multipart stream |
| `/capture` | GET | - | - | High-res JPEG snapshot |
| `/api/info` | GET | - | - | `apMode`, `connected`, `haveSaved`, `ip`, `hostname` |
| `/api/auth` | POST | - | `token` | Verify token |
| `/api/wifi` | POST | AP only | `ssid`, `pass` | Provision Wi-Fi (open only in AP mode) |
| `/api/state` | GET | yes | - | Full state JSON (below) |
| `/api/drive` | POST | yes | `throttle`, `steering` | Drive, -255..255 |
| `/api/stop` | POST | yes | - | Coast both motors |
| `/api/mood` | POST | yes | `mood` | `happy` `angry` `curious` `dead` `sleepy` `wink` `idle` |
| `/api/message` | POST | yes | `text` | Show message; empty `text` clears |
| `/api/flash` | POST | yes | `on` | 0/1 flashlight |
| `/api/settings` | POST | yes | `oledAnim` | 0/1 eyes animation |
| `/api/camera` | POST | yes | `flip` | 0/1 mirror+vflip image |
| `/api/token` | POST | yes | `token` | Change access token (min 4 chars) |

`/api/state` returns: `throttle`, `steering`, `mood`, `sleeping`,
`flashlightOn`, `oledAnim`, `camResolution`, `camQuality`, `camFlip`,
`connected`, `apMode`, `ip`, `messageActive`, `message`.

## OLED state machine

Screen priority (highest first):

```
BOOT (1.5 s) -> CONNECTION (until Wi-Fi is up) -> TEXT (persistent message)
            -> sleep (Zzz) -> mood / driving / idle face
```

- **BOOT**: "HERO / Booting..." with an animated progress bar.
- **CONNECTION**: shows `AP: 192.168.4.1` in setup mode, or "Connecting...".
- **TEXT**: compact eyes on top + a rounded text box with the wrapped message
  (up to 3 lines, ellipsized). The message persists until cleared; the ✕
  restores the full-size face and resets the sleep timer.
- **FACE**: full animated eyes. After 60 s without drive/mood/message input the
  eyes close and Zzz float up; any input wakes them.

## Face engine

Procedural, drawn with Adafruit_GFX primitives (no bitmaps). Inspired by the
open-source "Dasai Mochi" OLED projects. Eases every parameter (lid, pupil,
brow) toward targets. Blinks every ~1.8-4.3 s; the idle script randomly cycles
look-around, happy squint, curious peek, surprise, suspicious, doze and wink.
Moods override the idle script; driving overrides moods with a wide "whoa"
start and gaze in the steering direction.

## Camera notes

- Default stream: SVGA (800x600), JPEG quality 12, 2 PSRAM frame buffers,
  `CAMERA_GRAB_LATEST`, XCLK 20 MHz.
- The LEDC channels are deliberately split: motor PWM uses the Arduino LEDC
  wrapper (channels 1-4, timer 0) while the camera XCLK uses the native IDF LEDC
  driver on channel 5 / timer 2, so they can never collide.
- If the stream shows purple flicker or drops on a long ribbon, lower
  `cfg.xclk_freq_hz` from 20 MHz to 10 MHz in `camera_server.cpp`.

## Known limitations

- The MJPEG stream uses a single shared packetizer, so a second simultaneous
  viewer can cause glitches on the first; fine for a single controller phone.
- `/capture` temporarily switches the sensor to UXGA, which briefly interrupts
  the live stream for other clients.
- If the saved Wi-Fi network is unreachable, power-cycle the rover to re-enter
  the configuration AP mode.
