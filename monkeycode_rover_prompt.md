# ESP32-S3-CAM RC Rover with Animated OLED Face — Project Brief

## Role
Act as an embedded systems engineer helping a 3rd-year EEE student build a university microcontroller lab project: a Wi-Fi controlled RC rover with a live camera feed and an animated character face on an OLED display.

## 0. Before doing anything else
1. Read every file in the currently attached repository so you fully understand what exists so far.
2. Check the repo root for a file named `PROJECT_STATE.md`.
   - **If it exists** → this is a resumed session. Read it fully, silently pick up exactly where its "In Progress" section says to continue, and do NOT re-ask questions already answered there or redo completed checklist items. Give me a one-paragraph recap of where we left off, then continue the "next action."
   - **If it does not exist** → this is a new project. Once you understand the old repo's context, delete every file currently in the repository (I'm intentionally starting from scratch) and create `PROJECT_STATE.md` using the template in section 7.
3. For a new project only: before writing any code, list every ambiguity, missing spec, or assumption you'd otherwise have to guess at (pin choices excepted — see section 4). Wait for my answers.

## 1. Hardware
- MCU: ESP32-S3-CAM module with OV5640 camera sensor
- Motor driver: DRV8833 dual H-bridge
- Motors: 2x N20 DC gear motors (differential/skid steering)
- Display: 1.3" I2C OLED, SH1106 driver
- Assume standard 5V/3.3V power distribution appropriate for motors + logic; flag in the docs if a separate motor supply/regulator is advisable.

## 2. Core Functionality

### 2a. Driving
Web UI sends forward/backward/turn commands and a speed value (slider) to the ESP32, which drives the two N20 motors via the DRV8833 accordingly.

### 2b. Camera / video
- ESP32 hosts an MJPEG live stream from the OV5640, embedded in the web UI.
- Web UI has buttons to capture a still photo and record a short clip (see attached screenshots for placement). Decide whether captures are stored on an SD/TF card (if the board has one) or streamed straight to the browser for download, and document whichever you choose.

### 2c. OLED "Mochi eyes" character
All state-driven, continuous behavior:
- **Boot**: show the device's IP address and a "connecting…" message until a client authenticates with the password; only then start normal behavior.
- **Idle**: lively randomized eye animation/blinking; after N seconds with no driving input, transition to a "sleep" expression.
- **Driving**: eyes shift to reflect direction (e.g., look right while turning right) instead of idle animation.
- **Mood override**: selecting a mood from the web UI's mood list (angry, happy, confused, exclaimed, sad, etc.) shows that expression for a few seconds, then returns to whatever state would normally be active.
- **Text message mode**: sending text from the web UI shrinks the eyes and moves them upward, revealing a rounded-corner text box with the message underneath; this persists until "clear message" is pressed, then eyes return to normal and resume idle/driving behavior.
- Pick sensible simple defaults (timings, frame counts, etc.) rather than asking about every constant — just record what you chose in the docs.

### 2d. Web interface
- Password gate on load; controls/video only appear after successful auth.
- Clean, modern, minimal-footprint (efficiently served static assets, no heavy frameworks) — it's served off the ESP32's limited flash/RAM.
- Optimized for a smartphone in portrait mode; forward/back/steer controls positioned for two-thumb reach.
- Layout for controls, mood popup, text box, and capture buttons should follow the attached reference screenshots.
- On/off toggle for the onboard camera flash LED.

## 3. Code style
Keep the implementation as simple and readable as possible — favor clear, linear logic over clever abstraction, and comment non-obvious decisions. This is a student lab project, not a commercial product; don't over-engineer it.

## 4. Wiring
Camera-enabled ESP32-S3 boards use most GPIOs for the camera's parallel interface, so pin choice for the I2C OLED and the DRV8833's control pins needs care. Choose the best available pin assignment given the module's constraints, note any pins that must be avoided and why, and write the final pinout as a clear table in the documentation (section 6) — the hardware will be wired according to what you specify.

## 5. Explaining your work
Every time you finish writing or meaningfully changing a piece of code (a file, or a logical chunk within one), follow it with a plain-language explanation of what it does and why you built it that way — don't save all explanation for the very end. Once the whole project is complete, also produce one consolidated document walking through every file, every function, and every design decision (including the wiring choices and why).

## 6. Deliverables
- Firmware source, organized into sensible files/modules (not one giant .ino)
- Web UI assets served by the ESP32
- `README.md`: setup steps, wiring table, how to flash, how to use the web UI
- `PROJECT_STATE.md`: living project-state file (template below)
- The consolidated explanation document from section 5

## 7. `PROJECT_STATE.md` — keep this current
This project may span multiple sessions because of a limited daily AI usage quota, so treat this file as the single source of truth for yourself in a future session — or any other AI agent — to resume without re-reading this whole brief. **Update it immediately after every meaningful change**, not just at the end of a session, since a session can be cut off without warning.

Template:
```
# Project State

## Summary
(one paragraph: what this project is)

## Finalized Decisions
- Pinout (table)
- Key architecture choices and why

## Completed
- [x] item

## In Progress
- Current file/task, and the EXACT next action to take

## Open Questions For The User
- (anything still unanswered)

## Known Issues / TODO
-
```

## 8. Ground rules
- Don't start writing code until step 0's clarifying questions (if any) are answered.
- Never silently guess on anything material (pin conflicts, password storage, etc.) — ask, or state the assumption explicitly in `PROJECT_STATE.md`.
- Usage is metered, so keep responses efficient: don't restate this brief back to me, avoid redundant re-explanations, and keep `PROJECT_STATE.md` concise but complete.
- If you get cut off mid-task, make sure `PROJECT_STATE.md` reflects reality first — don't leave it stale.
