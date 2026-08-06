"use strict";

const $ = (id) => document.getElementById(id);

const state = {
  token: localStorage.getItem("mochi_token") || "",
  throttle: 0,
  steering: 0,
  speedScale: 255,
  mood: "idle",
  flashlight: false,
  oledAnim: true,
  dragging: false,
};

const authHeaders = () => ({ "X-Auth-Token": state.token });

async function api(path, body) {
  const opts = { method: body ? "POST" : "GET", headers: authHeaders() };
  if (body) opts.body = new URLSearchParams(body).toString();
  try {
    const res = await fetch(path, opts);
    const text = await res.text();
    let data = {};
    try { data = JSON.parse(text); } catch (e) { data = { raw: text }; }
    return { ok: res.ok, status: res.status, data };
  } catch (e) {
    return { ok: false, status: 0, data: {} };
  }
}

/* ---------------- auth gate ---------------- */
async function unlock() {
  const t = $("authToken").value.trim();
  if (!t) return;
  const r = await api("/api/auth", { token: t });
  if (r.ok && r.data.ok) {
    state.token = t;
    localStorage.setItem("mochi_token", t);
    $("authGate").classList.add("hidden");
    $("app").classList.remove("hidden");
    $("authErr").textContent = "";
    init();
  } else {
    $("authErr").textContent = "Wrong token. Try again.";
  }
}

/* ---------------- video ---------------- */
function startVideo() {
  const v = $("video");
  v.onload = () => { $("video").classList.remove("hidden"); };
  v.onerror = () => {
    $("video").classList.add("hidden");
    setTimeout(() => { v.src = "/stream?t=" + Date.now(); }, 2500);
  };
  v.src = "/stream?t=" + Date.now();
}

/* ---------------- drive ---------------- */
const steer = $("steering");
const throttlePad = $("throttle");
const steerKnob = $("steerKnob");
const throttleKnob = $("throttleKnob");

function sendDrive() {
  api("/api/drive", { throttle: state.throttle, steering: state.steering });
}

function updateKnob(knob, cx, cy, maxR, ox, oy) {
  knob.style.transform = `translate(calc(${cx}px - 50%), calc(${cy}px - 50%))`;
}

function attachPad(pad, knob, onValue, maxR, vertical) {
  const down = (e) => {
    state.dragging = true;
    pad.setPointerCapture(e.pointerId);
    move(e);
  };
  const move = (e) => {
    if (!state.dragging) return;
    const rect = pad.getBoundingClientRect();
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    let dx = e.clientX - rect.left - cx;
    let dy = e.clientY - rect.top - cy;
    if (vertical) dx = 0; else dy = 0;
    const dist = Math.hypot(dx, dy);
    if (dist > maxR) {
      dx = (dx / dist) * maxR;
      dy = (dy / dist) * maxR;
    }
    updateKnob(knob, dx, dy, maxR);
    onValue(dx, dy, maxR);
  };
  const up = (e) => {
    if (!state.dragging) return;
    state.dragging = false;
    knob.style.transform = `translate(-50%, -50%)`;
    onValue(0, 0, maxR);
  };
  pad.addEventListener("pointerdown", down);
  pad.addEventListener("pointermove", move);
  pad.addEventListener("pointerup", up);
  pad.addEventListener("pointercancel", up);
}

attachPad(steer, steerKnob, (dx) => {
  state.steering = Math.round((dx / 65) * 255);
  sendDrive();
}, 65, false);

attachPad(throttlePad, throttleKnob, (dx, dy) => {
  // up = forward (negative dy)
  const raw = Math.round((-dy / 80) * 255);
  state.throttle = Math.round((raw * state.speedScale) / 255);
  sendDrive();
}, 80, true);

$("stopBtn").addEventListener("pointerdown", () => {
  state.throttle = 0;
  state.steering = 0;
  steerKnob.style.transform = `translate(-50%, -50%)`;
  throttleKnob.style.transform = `translate(-50%, -50%)`;
  api("/api/stop", {});
});

/* ---------------- speed slider ---------------- */
const speedInput = $("speed");
const speedVal = $("speedVal");
speedInput.addEventListener("input", () => {
  const v = parseInt(speedInput.value, 10);
  speedVal.textContent = v;
  state.speedScale = v;
  if (navigator.vibrate) navigator.vibrate(10);
});
speedInput.addEventListener("change", () => {
  if (navigator.vibrate) navigator.vibrate(40);
});

/* ---------------- mood ---------------- */
async function setMood(m) {
  const r = await api("/api/mood", { mood: m });
  if (r.ok) {
    state.mood = m;
    document.querySelectorAll(".mood").forEach((b) => {
      b.classList.toggle("active", b.dataset.mood === m);
    });
  }
}
document.querySelectorAll(".mood").forEach((b) => {
  b.addEventListener("click", () => setMood(b.dataset.mood));
});

/* ---------------- message ---------------- */
function updateMsgState() {
  $("msgClear").classList.toggle("active", !!state.messageActive);
  $("msgInput").placeholder = state.messageActive
    ? "New message replaces current"
    : "Message to OLED";
}
$("msgSend").addEventListener("click", async () => {
  const t = $("msgInput").value.trim();
  if (!t) return;
  const r = await api("/api/message", { text: t });
  if (r.ok) state.messageActive = true;
  $("msgInput").value = "";
  updateMsgState();
});
$("msgInput").addEventListener("keydown", (e) => {
  if (e.key === "Enter") $("msgSend").click();
});
$("msgClear").addEventListener("click", async () => {
  const r = await api("/api/message", {});
  if (r.ok) state.messageActive = false;
  updateMsgState();
});

/* ---------------- flash / anim ---------------- */
async function setFlashlight(on) {
  state.flashlight = !!on;
  const r = await api("/api/flash", { on: state.flashlight ? 1 : 0 });
  $("flashBtn").classList.toggle("on", state.flashlight);
}

async function setAnim(on) {
  state.oledAnim = !!on;
  await api("/api/settings", { oledAnim: state.oledAnim ? 1 : 0 });
  $("animBtn").classList.toggle("on", state.oledAnim);
}

$("flashBtn").addEventListener("click", () => setFlashlight(!state.flashlight));
$("animBtn").addEventListener("click", () => setAnim(!state.oledAnim));

/* ---------------- settings modal ---------------- */
function openSettings() {
  const s = state;
  $("setIp").value = s.ip || "";
  $("setSsid").value = s.ssid || "";
  $("setPass").value = "";
  $("setTheme").value = localStorage.getItem("mochi_theme") || "dark";
  $("setRes").value = String(s.camResolution ?? 8);
  $("setQuality").value = s.camQuality ?? 12;
  $("setFps").value = s.camFps ?? 25;
  $("setFormat").value = String(s.camFormat ?? 1);
  $("setAnim").value = s.oledAnim ? "1" : "0";
  $("settingsMsg").textContent = "";
  $("settingsModal").classList.remove("hidden");
}
$("settingsBtn").addEventListener("click", openSettings);
$("settingsCancel").addEventListener("click", () => {
  $("settingsModal").classList.add("hidden");
});

$("settingsSave").addEventListener("click", async () => {
  const msg = $("settingsMsg");
  const theme = $("setTheme").value;
  localStorage.setItem("mochi_theme", theme);
  applyTheme(theme);

  const cam = {
    resolution: parseInt($("setRes").value, 10),
    quality: parseInt($("setQuality").value, 10),
    fps: parseInt($("setFps").value, 10),
    format: parseInt($("setFormat").value, 10),
    oledAnim: parseInt($("setAnim").value, 10),
  };
  let r = await api("/api/settings", cam);
  if (!r.ok) { msg.textContent = "Camera settings failed"; return; }
  state.camResolution = cam.resolution;
  state.camQuality = cam.quality;
  state.camFps = cam.fps;
  state.camFormat = cam.format;
  state.oledAnim = !!cam.oledAnim;
  $("animBtn").classList.toggle("on", state.oledAnim);

  const newToken = $("setToken").value.trim();
  if (newToken) {
    r = await api("/api/token", { token: newToken });
    if (r.ok) {
      state.token = newToken;
      localStorage.setItem("mochi_token", newToken);
    }
  }

  const ssid = $("setSsid").value.trim();
  if (ssid) {
    const pass = $("setPass").value;
    msg.textContent = "Wi-Fi credentials saved, reconnecting...";
    await api("/api/wifi", { ssid, pass });
  } else {
    msg.textContent = "Saved.";
  }
  setTimeout(() => { $("settingsModal").classList.add("hidden"); }, 1200);
});

function applyTheme(t) {
  document.documentElement.classList.toggle("light", t === "light");
}

/* ---------------- state polling ---------------- */
async function pollState() {
  const r = await api("/api/state");
  if (!r.ok) {
    if (r.status === 401) {
      $("app").classList.add("hidden");
      $("authGate").classList.remove("hidden");
      localStorage.removeItem("mochi_token");
    }
    return;
  }
  const s = r.data;
  state.ip = s.ip;
  state.ssid = s.ssid || "";
  state.camResolution = s.camResolution;
  state.camQuality = s.camQuality;
  state.camFps = s.camFps;
  state.camFormat = s.camFormat;
  state.oledAnim = s.oledAnim;
  state.flashlight = s.flashlightOn;
  state.messageActive = !!s.messageActive;
  $("flashBtn").classList.toggle("on", state.flashlight);
  $("animBtn").classList.toggle("on", state.oledAnim);
  updateMsgState();
  if (s.mood !== state.mood) {
    state.mood = s.mood;
    document.querySelectorAll(".mood").forEach((b) => {
      b.classList.toggle("active", b.dataset.mood === s.mood);
    });
  }
}

/* ---------------- init ---------------- */
async function init() {
  startVideo();
  const r = await api("/api/state");
  if (!r.ok) {
    if (r.status === 401) {
      $("app").classList.add("hidden");
      $("authGate").classList.remove("hidden");
      return;
    }
  } else {
    state.mood = r.data.mood;
    state.ip = r.data.ip;
    state.ssid = r.data.ssid || "";
    state.flashlight = r.data.flashlightOn;
    state.oledAnim = r.data.oledAnim;
    state.messageActive = !!r.data.messageActive;
    $("flashBtn").classList.toggle("on", state.flashlight);
    $("animBtn").classList.toggle("on", state.oledAnim);
    updateMsgState();
    document.querySelectorAll(".mood").forEach((b) => {
      b.classList.toggle("active", b.dataset.mood === r.data.mood);
    });
  }
  setInterval(pollState, 3000);
}

applyTheme(localStorage.getItem("mochi_theme") || "dark");

// auto-unlock with stored token on boot
(async () => {
  if (!state.token) {
    $("authGate").classList.remove("hidden");
    return;
  }
  const r = await api("/api/state");
  if (r.ok) {
    $("app").classList.remove("hidden");
    init();
  } else {
    $("authGate").classList.remove("hidden");
  }
})();

$("authBtn").addEventListener("click", unlock);
$("authToken").addEventListener("keydown", (e) => {
  if (e.key === "Enter") unlock();
});
