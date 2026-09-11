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
  - Photo: `/capture` grabs a JPEG at the live stream resolution (SVGA default),
    downloaded as `hero_<timestamp>.jpg`. (UXGA mid-stream reused the SVGA
    buffers and produced a half-valid image.)
  - Clip: recorded client-side with MediaRecorder (VP9/VP8 WebM) by drawing the
    stream to a hidden canvas; downloaded as `hero_clip.webm`. No SD card used.
- Differential drive: cross d-pad (forward / left / right / back), speed slider
  (0-255), stop on release, arcade-style throttle + steering mixing on the DRV8833.
- Drive safety watchdog: the UI re-sends the drive command every 300 ms while a
  button is held; if no drive/stop command reaches the rover for ~1.5 s
  (`DRIVE_WATCHDOG_MS`) the motors coast automatically. A dropped connection,
  closed tab or killed app can therefore never leave the rover driving itself.
- Flashlight toggle driving the on-board LED on GPIO2.
- Mood system: six moods (Happy, Angry, Curious, Dead, Sleepy, Wink) shown both
  in a quick popup and a dropdown. A mood override lasts ~4 s (Wink 1.5 s)
  then returns to the idle face.
- Message-to-OLED: persistent rounded text box with the animated eyes kept in a
  compact mode above it; cleared with the ✕ button in the UI.
- Animated OLED face: blinking, saccades and an autonomous idle script
  (look-around, surprise, suspicious squint, doze-off, occasional wink), a
  wide-eyed "whoa" kick on drive start, driving-reaction eyes, and a Zzz sleep
  state after 60 s without input.
- Network: always SoftAP `HERO` / `hero1234` at `192.168.4.1` (captive DNS +
  mDNS hostname `hero`). No station-mode / lab-router provisioning.
- Auth: token-gated control API (default token `hero`, changeable in Settings).
  Streaming is open so the live view can start before unlock.

## Web UI

Smartphone-first dark UI with a light theme option (saved in the browser):

- Header: "HERO" wordmark, a status pill (CTRL / CAM dots), and flashlight /
  mood / settings icon buttons (SVG, no emoji).
- Video panel fills remaining portrait height and keeps the sensor aspect
  (`object-fit: contain`). Overlay: flip, photo, record.
- Cross d-pad (forward / left / right / back) plus mood + speed on the side.
- Bottom card: "Message to OLED..." input with inline Send (+ ✕ to clear), a
  Mood dropdown, and a Speed slider with a live value.
- Mood popup: a white rounded card under the header with the six moods.
- Settings modal: hotspot reminder, theme, reverse-left / reverse-right motor
  toggles, left/right speed trim (50-100%, persisted in NVS), optional new
  access token.

## API

All control endpoints (except `/api/info`, `/api/auth`, `/stream` and
`/capture`) require the token either as an `X-Auth-Token` header or a
`?token=` query parameter.

| Endpoint | Method | Auth | Body params | Description |
| -------- | ------ | ---- | ----------- | ----------- |
| `/` `/style.css` `/app.js` | GET | - | - | Static UI |
| `/stream` | GET | - | - | MJPEG multipart stream |
| `/capture` | GET | - | - | JPEG snapshot at stream resolution |
| `/api/info` | GET | - | - | `apMode`, `connected`, `ip`, `hostname` |
| `/api/auth` | POST | - | `token` | Verify token |
| `/api/state` | GET | yes | - | Full state JSON (below) |
| `/api/drive` | POST | yes | `throttle`, `steering` | Drive, -255..255. While a button is held the UI repeats this every 300 ms as a keep-alive; the firmware coasts the motors ~1.5 s after the last drive/stop command (`DRIVE_WATCHDOG_MS` in `config.h`) |
| `/api/stop` | POST | yes | - | Coast both motors |
| `/api/pintest` | GET/POST | yes | `pin` (0-3), `speed` (-255..255, default 200) | Bring-up test: drives a single motor input directly for ~0.6 s (`MOTOR_TEST_MS`), bypassing mixing/reverse/trim/watchdog. Reply `duty` is the PWM read back: non-zero means the pin is attached and driven. Pin map: 0=L_IN1/GPIO47, 1=L_IN2/GPIO14, 2=R_IN3/GPIO21, 3=R_IN4/GPIO42. Example: `/api/pintest?token=hero&pin=1&speed=200` |
| `/api/mood` | POST | yes | `mood` | `happy` `angry` `curious` `dead` `sleepy` `wink` `idle` |
| `/api/message` | POST | yes | `text` | Show message; empty `text` clears |
| `/api/flash` | POST | yes | `on` | 0/1 flashlight |
| `/api/settings` | POST | yes | `oledAnim`, `reverseLeft`, `reverseRight`, `trimLeft`, `trimRight` | eyes animation, per-motor reverse, and PWM trim (50-100) |
| `/api/camera` | POST | yes | `flip` | 0/1 mirror+vflip image |
| `/api/token` | POST | yes | `token` | Change access token (min 4 chars) |

`/api/state` returns: `throttle`, `steering`, `mood`, `sleeping`,
`flashlightOn`, `oledAnim`, `camResolution`, `camQuality`, `camFlip`,
`reverseLeft`, `reverseRight`, `trimLeft`, `trimRight`, `connected`,
`apMode`, `ip`, `messageActive`, `message`.

## OLED state machine

Screen priority (highest first):

```
BOOT (1.5 s) -> CONNECTION (~6.5 s AP details) -> TEXT (persistent message)
            -> sleep (Zzz) -> mood / driving / idle face
```

- **BOOT**: "HERO / Booting..." with an animated progress bar.
- **CONNECTION**: `Join HERO AP`, `AP: 192.168.4.1`, password, and
  `open 192.168.4.1`.
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
  wrapper (channels 0-3, timers 0-1) while the camera XCLK uses the native IDF
  LEDC driver on channel 5 / timer 2, so they can never collide. Channels 1-4
  would place the fourth motor on timer 2 (the camera timer) and break that
  motor's reverse.
- If the stream shows purple flicker or drops on a long ribbon, lower
  `cfg.xclk_freq_hz` from 20 MHz to 10 MHz in `camera_server.cpp`.

## Known limitations

- The MJPEG stream uses a single shared packetizer, so a second simultaneous
  viewer can cause glitches on the first; fine for a single controller phone.
- `/capture` grabs a JPEG at the current stream size. Taking a photo still
  briefly interrupts the live MJPEG for other clients.
- The rover is AP-only; the phone has no internet while joined to **HERO**.
