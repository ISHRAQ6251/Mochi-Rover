# HERO - User Manual

A step-by-step guide for building, uploading and driving your HERO.

---

## 1. What you need

### Electronics

| Item | Notes |
| ---- | ----- |
| ESP32-S3-CAM board (N16R8 clone) | 16 MB flash, 8 MB PSRAM, **OV5640** camera |
| DRV8833 motor driver module | dual H-bridge, 2.7-10.8 V |
| 2x DC gear motors (N20 or TT) | one per side |
| 2x wheels + castor/ball wheel | differential (skid) steering |
| SH1106 1.3" OLED 128x64 | I2C, address 0x3C |
| Battery pack for motors | e.g. 2x 18650 or 4x AA (about 6 V) |
| Power for the ESP32 | USB phone-charging bank / 5 V regulator |
| Jumper wires | for motor driver + OLED |
| Optional: external LED + resistor | only if your board has no onboard LED on GPIO2 |

### Software

- [arduino-cli](https://arduino.github.io/arduino-cli/) (recommended) **or** the
  Arduino IDE 2.x with the ESP32 board package.
- USB data cable for the ESP32-S3-CAM.

---

## 2. Software setup (one time)

### 2.1 Install arduino-cli (Linux/macOS/Windows)

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"    # add to your shell profile
```

or install the latest release from the [releases page](https://github.com/arduino/arduino-cli/releases).

### 2.2 Add the ESP32 core

```bash
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

This installs the Arduino ESP32 core (v3.x) which already bundles the
precompiled `esp32-camera` driver, so you do not need to install the camera
library separately.

### 2.3 Install the libraries

```bash
arduino-cli lib install "ESPAsyncWebServer" "AsyncTCP" \
  "Adafruit GFX Library" "Adafruit SH110X" "Adafruit BusIO"
```

> **If you get an mbedTLS error** while compiling
> (`mbedtls_md5_update_ret ... not declared`), edit
> `~/Arduino/libraries/ESPAsyncWebServer/src/WebAuthentication.cpp` and remove
> the `_ret` suffix from the `mbedtls_md5_starts/update/finish` calls, then save.
> This is a known incompatibility with newer ESP32 core mbedTLS versions.

### 2.4 Compile the sketch

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi HERO
```

`FlashSize=16M` and `PSRAM=opi` are important - the N16R8 board needs both.
You should see:

```
Sketch uses 1106618 bytes (84%) of program storage space.
Global variables use 60812 bytes (18%) of dynamic memory.
```

### 2.5 Alternative: build with the Arduino IDE

If you prefer the Arduino IDE over the command line:

1. Install the **Arduino IDE 2.x**.
2. In **File > Preferences**, set the "Additional boards manager URLs" to
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json` (add it
   if the field is empty).
3. In **Boards Manager** (Tools > Board > Boards Manager), search for
   **esp32** by Espressif and install it.
4. In **Library Manager**, install: **ESPAsyncWebServer** and **AsyncTCP**
   (pick the versions that support the ESP32 core 3.x - the actively maintained
   forks), plus **Adafruit GFX Library**, **Adafruit SH110X**, **Adafruit
   BusIO**.
5. Open `HERO/HERO.ino`. It auto-loads the other files in the
   folder.
6. **Tools > Board > esp32 > ESP32S3 Dev Module**, then set:
   - **Flash Size**: `16MB (128Mb)`
   - **PSRAM**: `OPI PSRAM`
   - **USB CDC On Boot**: `Enabled` (optional; lets you see serial logs)
7. **Sketch > Export Compiled Binary**, or select your port and click
   **Upload** (see section 4 for boot mode).

---

## 3. Wiring

Work from this table. GPIO numbers refer to the silkscreen/labels on the
ESP32-S3-CAM module header pins.

| From (ESP32-S3-CAM) | GPIO | To |
| ------------------- | ---- | -- |
| Camera ribbon | - | Already attached via FPC socket (no user wiring) |
| IN1 | 1 | DRV8833 **AIN1** |
| IN2 | 14 | DRV8833 **AIN2** |
| IN3 | 21 | DRV8833 **BIN1** |
| IN4 | 42 | DRV8833 **BIN2** |
| DRV8833 **AO1/AO2** | - | Left motor terminals |
| DRV8833 **BO1/BO2** | - | Right motor terminals |
| DRV8833 **VM** | - | Motor battery **+** |
| DRV8833 **VIN** | - | 3.3 V (logic supply) |
| GND | - | Common ground (all boards) |
| OLED **SDA** | 40 | SH1106 SDA |
| OLED **SCL** | 41 | SH1106 SCL |
| OLED **VCC** | - | 3.3 V |
| OLED **GND** | - | GND |
| Flash LED (onboard) | 2 | already wired; otherwise an external LED + resistor to GND |

### Wiring checklist

1. **Camera** - seat the OV5640 FPC ribbon into the socket on the ESP32-S3-CAM
   board, gold contacts facing the board. No header pins are needed for it.
2. **DRV8833 inputs** - four jumper wires from the ESP32 GPIOs to the driver's
   four IN pins.
3. **Motors** - connect each motor's two terminals to one driver channel
   (AO1/AO2 for the left motor, BO1/BO2 for the right). Swapping a motor's two
   wires reverses its direction - fix steering direction by swapping if needed.
4. **Motor power** - battery pack to **VM** and **GND**. Connect the same GND
   rail to the ESP32 GND (a common ground is required).
5. **OLED** - 4 wires (VCC, GND, SDA, SCL) from GPIO 40/41. Motors and the
   DRV8833 are not required for the face or the web UI to come up.
6. **Driver logic supply** - DRV8833 `VIN` to 3.3 V; tie `nSLEEP` and `VMODE`
   to 3.3 V as well (enables the driver, full motor voltage).

### Power

- The ESP32 is powered over **USB** (flash it, then run it from a USB power
  bank) or from a regulated 5 V.
- The motors run from a **separate battery pack** (2x 18650 or 4x AA = ~6 V)
  into the DRV8833 `VM` pin.
- Never draw motor current from the ESP32's regulator, and always keep the
  grounds common.

### Avoid these pins

GPIO 19/20 (USB), GPIO 43/44 (UART serial debug), GPIO 0 (BOOT button),
strapping pins GPIO 3/45/46, and **GPIO 33-37** (OPI PSRAM on the N16R8 —
using 35/36 for the OLED watchdog-resets the chip). The camera already
occupies GPIO 4-18 (except 14).

---

## 4. Uploading the code

### 4.1 Put the board in download mode

1. Connect the board to your computer with a **USB data** cable (many cheap
   cables are charge-only - if upload hangs, try another cable).
2. Hold the **BOOT** button on the board.
3. While holding BOOT, tap **RST** and release BOOT.
4. The board stays in download mode until it is reset or powered off.

The board exposes a native USB-Serial/JTAG interface, so it usually appears as
`/dev/ttyACM0` or `/dev/ttyUSB0` (Linux/macOS) or a COM port (Windows).

Find it with:

```bash
arduino-cli board list
```

### 4.2 Upload

```bash
arduino-cli upload -p /dev/ttyACM0 \
  --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi HERO
```

Replace `/dev/ttyACM0` with the port from the previous step.

You should see the progress bar run and finish with "Hard resetting...". The
rover then boots. **First boot after flashing is slow** (Wi-Fi tries to connect,
then falls back to the setup network) - give it ~30 seconds.

**If upload hangs at "Connecting...":** the board is not in download mode (BOOT
was not held). Try again, or check the cable. Some boards with USB-C sockets
need the BOOT sequence *after* plugging in power.

### 4.3 Verify the firmware is running

After upload, open a serial monitor to see the boot log:

```bash
# arduino-cli
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
```

You should see `HERO boot`, then `settings ok` / `motors ok` / `oled ok` (or
`oled missing`), `wifi ...`, `camera ok` (or a camera error) and `HERO ready`.
If you only ever see the ROM dump (`ESP-ROM:esp32s3-...` / `rst:0x8
(TG1WDT_SYS_RST)`) the chip is crashing before the sketch prints — reflash with
`FlashSize=16M,PSRAM=opi` and confirm the OLED is on GPIO 40/41, not 35/36.
If you instead see a camera PID other than OV5640, check the `CAM_*` pins in
`config.h`. Enable **USB CDC On Boot** in the board options so USB serial
works after reset.

### 4.4 Alternative: upload with the Arduino IDE

With the board set up as in section 2.5 and in download mode, select the port
under **Tools > Port**, then click the **Upload** button (right arrow). Watch
the console at the bottom of the IDE for the progress and result.

### 4.5 Alternative: upload with esptool.py

`esptool.py` is included with the ESP32 core. Compile first, then flash the
built binary with a single command:

```bash
# find the built binary
ls HERO/build/*/HERO.ino.bin

# flash (merge puts the app at the right 0x10000 offset with the correct flash size)
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 \
  --baud 921600 write_flash 0x0 HERO/build/*/HERO.ino.merged.bin
```

Using the `.merged.bin` (if the core produces one) sets the correct bootloader,
partition table and app offsets automatically. Otherwise use `esptool merge_bin`
or let the IDE/arduino-cli handle offsets.

---

## 5. Using the rover

### 5.1 First-time Wi-Fi setup

The rover has no network saved on first boot, so it opens its own Wi-Fi network
(called a SoftAP):

1. Power the rover.
2. On your phone, join the Wi-Fi network **`HERO`** (password
   **`hero1234`**). A captive-portal login window may pop up automatically.
3. Open `http://192.168.4.1` in a browser.
4. Enter your lab Wi-Fi name (SSID) and password, then tap **Save / Connect**.
5. The rover switches to your network. Back on your phone, join the **lab
   Wi-Fi** again.

The rover remembers these credentials, so you only do this once.

### 5.2 Opening the control page

On the lab Wi-Fi, open one of these on your phone:

- `http://hero.local` (mDNS - works on most phones)
- `http://<rover-ip>` where `<rover-ip>` is the address shown on the rover's
  status screen (or shown in the OLED CONNECTION screen / on the AP page).

You will be asked for the **access token** - the default is **`hero`**. Change
it later in the ⚙️ settings panel.

### 5.3 The control page (cockpit)

- **Top bar** - the "HERO" logo, a status pill (green dot = connected to the
  rover, and a camera dot that lights when live video is arriving), then three
  buttons: **🔦** flashlight, **👀** quick mood popup, **⚙️** settings.
- **Video panel** - live stream. Overlay buttons: **🔄** flip/refresh (mirrors
  + flips the image and reloads the stream), **📷** takes a photo,
  **●** records a video clip (turns **⏹** while recording, a red REC badge
  shows; tap again to finish).
- **Steering / throttle pads** - four round buttons:
  - **◀ ▶** (STEERING) and **▲ ▼** (THROTTLE).
  - Hold to move, release to stop. Combine them to drive in arcs, e.g. hold
    ▲ and ◀ together to turn while moving forward.
  - Every command is acknowledged by the rover (the OLED shows driving-reaction
    eyes).
- **Drive safety watchdog** - while you hold a button the app re-sends the drive
  command every 300 ms. If the connection drops, the browser tab is closed, or
  the app is killed, the rover receives no more commands and stops the motors on
  its own after ~1.5 s. You never need to "find" a stuck rover that is driving
  itself.
- **Message to OLED** - type text and tap **Send**; the message appears on the
  rover's OLED. Tap **✕** to clear it and restore the face.
- **Mood** - dropdown with six moods: Happy, Angry, Curious, Dead, Sleepy, Wink.
  The face shows the mood for about 4 seconds, then returns to the idle face.
- **Speed** - slider from 0 to 255; controls how fast the motors run. Start low
  on a bench and raise it once you can control the rover.

### 5.4 What the OLED shows

- **Boot screen** - "HERO / Booting..." with a progress bar.
- **Connection screen** - the rover's IP address, or `AP: 192.168.4.1` in setup
  mode.
- **The face** - the little robot with animated blinking eyes and idle
  behaviour. While sleeping it shows closed eyes with floating "Zzz".
- **Message box** - when you send a message it replaces the big face; the eyes
  shrink to a small strip above the message.
- **Driving** - the eyes react to throttle/steering.
- **Sleep** - after 60 seconds without any input the face "falls asleep" (Zzz);
  any drive/mood/message command wakes it.

### 5.5 Photos and clips

- **Photo** - a still is captured at high resolution (1600x1200) and downloads
  as `hero_<timestamp>.jpg`.
- **Clip** - recording is done on your phone's browser (WebM), so no SD card is
  needed. It downloads as `hero_clip.webm` when you stop recording. Note that
  the stream must be live (green CAM dot) for recording to work.

### 5.6 Settings (⚙️)

- **Rover IP / Domain** - the address the app talks to; useful if you use a
  fixed IP.
- **Theme** - dark / light.
- **Change access token** - pick a new login password (min 4 characters). Write
  it down - it is stored on the rover.

### 5.7 Returning to setup mode

If the rover cannot reach the saved Wi-Fi (new lab, router restarted, wrong
password), power-cycle it. After ~10 seconds of failed connection it opens the
`HERO` setup network again and you can re-provision it from
`http://192.168.4.1`.

---

## 6. Troubleshooting

| Symptom | Fix |
| ------- | --- |
| Upload hangs / "no device found" | Use a **data** USB cable; put the board in download mode (BOOT + tap RST); try another USB port |
| Blank OLED + serial only shows `ESP-ROM` / `TG1WDT_SYS_RST` | Watchdog reset: OLED must be on GPIO **40/41**, never 35/36 (those are PSRAM). Reflash, tap RST. Motors do not need to be wired. |
| Serial shows `I2C bus id(1) has already been acquired` | Reflash this build - Wire1 was started twice; the OLED bus then stayed dead |
| OLED stays blank but serial prints `oled missing` | Check VCC=3.3 V, GND, SDA=40, SCL=41; some panels use I2C address 0x3D (the firmware tries both) |
| Serial monitor empty after `HERO boot` should appear | Enable **USB CDC On Boot**; 115200 baud; tap RST after opening the monitor |
| No image in the video panel, CAM dot red | Check the camera ribbon is seated; verify GPIO settings match your board; try reducing XCLK to 10 MHz (see camera_server.cpp) |
| Motors do not spin | Check DRV8833 power (VM/GND), `nSLEEP` tied to 3.3 V, and IN wires; try 100 on the speed slider |
| One motor spins backwards | Swap that motor's two wires on the driver |
| Motors spin but the rover turns wrong way | Swap the left/right motor channels (AO/BO) |
| Can't find `hero.local` | Use the rover's IP instead (shown on the OLED connection screen); mDNS needs your phone to be on the same network |
| Rover keeps entering setup mode | Wi-Fi password changed or wrong SSID - re-provision from the `HERO` AP |
| Lost the access token | Reflash the firmware to restore the default token `hero` |
| Flickering / purple video | Lower camera XCLK from 20 MHz to 10 MHz in `camera_server.cpp` |
| Face shows "AP:" IP | The rover is in setup mode - provision Wi-Fi from `http://192.168.4.1` |
| Video freezes briefly, CAM dot red, then recovers | The stream drops and auto-reconnects after ~2.5 s - normal if the camera is busy capturing a photo; avoid tapping 📷 while driving |
| Live view frozen / static frame on iPhone Safari | Older iOS Safari renders only the first MJPEG frame; use Chrome on Android or install Chrome on the iPhone |
| Motors keep running after closing the app / phone disconnects | The firmware's drive watchdog coasts the motors ~1.5 s after the last drive command, so an unreachable app can never drive the rover on its own. Ensure you flashed the latest build (older builds lacked the watchdog) |
| All buttons do nothing after logging in | The POST requests were not decoded by the server - make sure you flashed the current `web_assets.h` / `app.js` build (the UI must send `Content-Type: application/x-www-form-urlencoded`)
