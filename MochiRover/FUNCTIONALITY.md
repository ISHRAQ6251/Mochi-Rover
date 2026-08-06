# MochiRover — Full Functionality Report

ESP32-CAM smart rover with MJPEG live video, an authenticated responsive web
cockpit, DRV8833 dual-motor drive, and a Dasai-Mochi-style animated OLED face.

**Status: COMPLETE and FLASH-READY.** The firmware compiles cleanly
(`arduino-cli compile --fqbn esp32:esp32:esp32cam` — no errors, no warnings)
and ready-to-flash binaries are in `dist/`.

---

## 1. Architecture

| Module | File | Responsibility |
| ------ | ---- | -------------- |
| Entry point | `MochiRover.ino` | `setup()`/`loop()` orchestration |
| Config | `config.h` | pins, AP credentials, timing constants |
| Settings | `settings.h/.cpp` | NVS-backed `RoverSettings` struct |
| Motors | `motor_control.h/.cpp` | DRV8833 differential drive (LEDC PWM) |
| Eyes | `mochi_eyes.h/.cpp` | procedural Dasai-Mochi face engine |
| Display | `display_manager.h/.cpp` | screen scheduler + SH1106 render/push |
| Wi-Fi | `wifi_helper.h/.cpp` | STA / AP / captive-DNS / mDNS |
| Camera | `camera_server.h/.cpp` | OV2640 init, MJPEG stream + snapshot |
| Web | `web_server.h/.cpp` | ESPAsyncWebServer routes + REST API |
| UI | `web_ui/*` + `web_assets.h` | cockpit HTML/CSS/JS (PROGMEM embedded) |
| Tooling | `tools/embed_web.py` | regenerates `web_assets.h` from `web_ui/` |

Boot-time priority (in `setup()`): logging level → NVS settings → motors →
display/OLED → Wi-Fi → camera → web server. Everything is non-blocking;
`loop()` simply calls `wifiHelper.update()`, `displayMgr.update()`,
`motors.update()` every 10 ms while async web/camera work runs in the
background.

## 2. System State Machine

```
                 ┌────────────────────────────┐
                 │            BOOT            │  MochiRover splash + progress
                 └──────────────┬─────────────┘
                                │ 1.5 s
                                ▼
                 ┌────────────────────────────┐
                 │    NETWORK ACQUIRE (STA)   │─── saved SSID present ──► STA connect
                 └──────────────┬─────────────┘
                                │ no network saved / connect fails
                                ▼
                 ┌────────────────────────────┐
                 │   CONFIG PORTAL (AP mode)  │  MochiRover/mochi1234 @192.168.4.1
                 └──────────────┬─────────────┘  captive DNS → cockpit settings → NVS
                                │ STA connects
                                ▼
                 ┌────────────────────────────┐
                 │  RUN: CAMERA + WEB SERVER  │  MJPEG stream, REST API, mDNS
                 └──────────────┬─────────────┘
                                │ 600 s inactivity      │ new drive/mood/message
                                ▼                        ▼
                 ┌────────────────────────────┐  ┌────────────────────────────┐
                 │   SLEEP (Zzz) — wake on    │  │   ACTIVITY — resets sleep   │
                 │   any user input           │  └─────────────┬──────────────┘
                 └────────────────────────────┘                │
                                                                ▼
                                               ┌──────────────────────────────┐
                                               │   FACE / TEXT / DRIVING / ... │
                                               └──────────────────────────────┘
```

### 2a. Display screen scheduler (`DisplayManager::pickScreen`)

Screen selection is **strict priority, highest first**, evaluated every frame:

1. **BOOT** — splash + animated progress bar for 1.5 s after power-on.
2. **CONNECTION** — shown whenever the network is not connected; renders AP
   setup hint (`AP: 192.168.4.1`) in portal mode, "Connecting..." otherwise.
3. **TEXT** — shown while a message is active. The message **persists until the
   cockpit's dedicated Clear button is pressed** (it no longer auto-expires).
   While active, the eyes shrink and rise to the top and a rounded-corner text
   box holds the wrapped message below.
4. **FACE** — the normal face. Within it, the sub-priorities are:
   **sleep (Zzz) > driving reaction > explicit mood > autonomous idle**.
   - *Sleep*: after `SLEEP_TIMEOUT_MS` (600 000 ms) of no user input, eyes close
     and animated "Zzz" float up. Any activity wakes it instantly.
   - *Driving*: throttle/steering live-reacts (excitement kick on start, lids
     narrow with speed, pupils track steering).
   - *Mood*: explicit mood persists until another mood or idle is chosen.
   - *Idle*: the autonomous Dasai-Mochi idle script runs (see below).

Note: because TEXT outranks sleep, an active message never disappears on its
own; pressing Clear restores the full-size face and resets the sleep timer.

### 2b. Wi-Fi state machine (`WifiHelper`)

- **STA-first**: tries the NVS-saved SSID non-blocking; re-checks every 30 s.
- **AP fallback**: no saved network (or STA fails) → soft-AP `MochiRover`/
  `mochi1234` on `192.168.4.1` with captive DNS; the cockpit's settings panel
  writes target credentials to NVS and triggers a reconnect.
- **mDNS**: advertises `mochirover.local` and `_http` service.
- **Apply path**: `/api/wifi` saves and reconnects; `/api/token` rotates the
  auth token (also persisted in NVS).

## 3. The Dasai Mochi Face

The face is **fully procedural** (Adafruit_GFX primitives), inspired by the
open-source Dasai Mochi ecosystem (`upiir/esp32s3_oled_dasai_mochi`,
`NeoDoggy/BetterMochi`). The upstream projects ship monolithic pre-rendered
bitmaps, so authentic *behaviour* is recreated here as an animation engine.

**Autonomous idle script** (`MochiEyes::updateIdleScript`) — mirrors the feel of
the upstream Rive 90-frame idle loop: while sitting idle the character
periodically **looks around** (fast, wide saccades), **blinks**, and
spontaneously shifts through small expressions, each eased to/from with smooth
transitions:

| Idle pose | Behaviour |
| --------- | --------- |
| NEUTRAL   | gentle neutral look, occasional soft smile |
| LOOK      | active gaze wander (faster, wider saccades) |
| HAPPY     | content squint + smile |
| CURIOUS   | tilted peek + small mouth |
| SURPRISE  | wide eyes + open mouth |
| SUSPICIOUS| narrowed lids + slight frown |
| DOZE      | brief heavy-lid doze-off dip |
| WINK      | one-eye wink + smile |

Randomized on 0.9–2.7 s picks; blinks every 1.8–4.3 s; independent of the
explicit 12-mood API. All expressions are **driving-reactive**: pressing the
throttle instantly pops an "excitement kick" (wide eyes) that eases down as
speed builds, lids narrow with speed, and pupils steer into the turn.

**Text mode**: `setCompact(true)` drives a smooth 280 ms shrink-and-rise of the
eyes (interpolated geometry); the mouth/brows fold away to leave room for the
persistent rounded text box. The eyes stay fully animated in compact mode.

## 4. Feature Checklist

- [x] MJPEG live video (`/stream`, `multipart/x-mixed-replace`) + snapshot
      (`/capture`) — open endpoints, single shared stream client.
- [x] Authenticated REST API (`X-Auth-Token` header or `token` query param),
      default token `mochi`, rotatable via `/api/token`.
- [x] Arcade differential driving: two pointer pads → throttle + steering,
      mixed per-motor, STOP button, motor safety timeout.
- [x] Speed slider 0–255, integer snapping + `navigator.vibrate` haptics.
- [x] 12 moods (happy, angry, sad, crying, dead, confused, sleepy, wink, blink,
      curious, love, idle).
- [x] Persistent message-to-OLED with rounded text box + **Clear** button
      (`/api/message`, empty `text` clears).
- [x] Flashlight (GPIO4) and OLED-animation toggles.
- [x] Settings modal: rover IP, Wi-Fi SSID/password, theme, camera
      resolution/quality/fps/format, animation toggle.
- [x] Auth gate UI + token auto-unlock via localStorage.
- [x] Dark/light theme; responsive layout (`max-width: 640px`).
- [x] Sleep / wake with animated Zzz.
- [x] Config portal (AP + captive DNS) when no Wi-Fi is saved.
- [x] State polling (`/api/state`, 3 s) drives chips, mood highlight and the
      message-active indicator.

## 5. Task Priorities (runtime)

1. **Async web/camera** (ESP-IDF freeRTOS tasks under ESPAsyncWebServer): stream
   frames, serve API/UI — highest throughput, never blocks `loop()`.
2. **`loop()`** (10 ms): Wi-Fi reconnect timer → display scheduler
   (screen pick + eyes update + OLED push) → motor safety timeout.
3. **Display scheduler** decides *which* face work runs this frame; eyes are
   updated only when a face/text screen is active.
4. **Motor safety** (`motors.update()`): brakes after `MOTOR_STOP_DELAY_MS`
   with no new drive command — belt-and-suspenders on top of the web STOP.

Rationale: streaming is time-critical and runs in its own task; the OLED face
is a 10 Hz/100 Hz best-effort render; motor safety is cheap and runs every loop
so a lost connection can never leave the rover running.

## 6. Flash Readiness

Verified with Arduino CLI:

```
arduino-cli compile --fqbn esp32:esp32:esp32cam /workspace/MochiRover/MochiRover.ino
  Sketch uses 1133064 bytes (36%) of program storage space. Maximum is 3145728 bytes.
  Global variables use 62428 bytes (19%) of dynamic memory. Maximum is 327680 bytes.
```

- **No errors, no warnings.** Fits easily in the 3 MB "Huge APP" partition.
- Flashable binaries refreshed in `dist/`:
  - `MochiRover.merged.bin` (4 194 304 B) — single-file flash
  - `MochiRover.bin` (1 133 216 B), `MochiRover.bootloader.bin`,
    `MochiRover.partitions.bin` — component flash
- Libraries pinned: ESP32 core `esp32:esp32@3.3.11`, Adafruit GFX 1.12.6,
  Adafruit SH110X 2.1.14, Adafruit BusIO 1.17.4, ESP Async WebServer 3.12.0,
  Async TCP 3.5.0.
- Wiring per the confirmed diagram: DRV8833 GPIO3/1/13/12, OLED SDA/SCL
  GPIO14/15, flash GPIO4, OV2640 standard pinout, SD disabled.

**The project is complete and ready to flash to the ESP32-CAM.**
