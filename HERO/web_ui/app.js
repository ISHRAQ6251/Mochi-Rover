"use strict";

const $ = (id) => document.getElementById(id);

const state = {
  token: localStorage.getItem("hero_token") || "",
  throttle: 0,
  steering: 0,
  speedScale: 255,
  mood: "idle",
  flashlight: false,
  flip: false,
  camLive: false,
  reverseLeft: false,
  reverseRight: false,
  trimLeft: 100,
  trimRight: 100,
};

const authHeaders = () => ({ "X-Auth-Token": state.token });

async function api(path, body) {
  const opts = { method: body ? "POST" : "GET", headers: authHeaders() };
  if (body) {
    // ESPAsyncWebServer only parses POST params for urlencoded bodies; the
    // browser default (text/plain) would not be decoded.
    opts.headers["Content-Type"] = "application/x-www-form-urlencoded";
    opts.body = new URLSearchParams(body).toString();
  }
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

function applyTheme(t) {
  document.documentElement.classList.toggle("light", t === "light");
}
applyTheme(localStorage.getItem("hero_theme") || "dark");

/* ---------------- boot flow ---------------- */
async function boot() {
  if (!state.token) {
    $("authGate").classList.remove("hidden");
    return;
  }
  const r = await api("/api/state");
  if (r.ok) {
    $("app").classList.remove("hidden");
    initCockpit(r.data);
  } else {
    if (r.status === 401) {
      localStorage.removeItem("hero_token");
      state.token = "";
    }
    $("authGate").classList.remove("hidden");
  }
}

/* ---------------- auth ---------------- */
async function unlock() {
  const t = $("authToken").value.trim();
  if (!t) return;
  const r = await api("/api/auth", { token: t });
  if (r.ok && r.data.ok) {
    state.token = t;
    localStorage.setItem("hero_token", t);
    $("authGate").classList.add("hidden");
    $("authErr").textContent = "";
    const st = await api("/api/state");
    $("app").classList.remove("hidden");
    initCockpit(st.ok ? st.data : {});
  } else {
    $("authErr").textContent = "Wrong token. Try again.";
  }
}
$("authBtn").addEventListener("click", unlock);
$("authToken").addEventListener("keydown", (e) => { if (e.key === "Enter") unlock(); });

/* ---------------- video ---------------- */
function markCam(live) {
  state.camLive = !!live;
  const d = $("dotCam");
  if (d) d.classList.toggle("on", state.camLive);
}

function startVideo() {
  const v = $("video");
  v.onload = () => { markCam(true); };
  v.onerror = () => {
    markCam(false);
    setTimeout(() => { v.src = "/stream?t=" + Date.now(); }, 2500);
  };
  v.src = "/stream?t=" + Date.now();
}
function refreshVideo() {
  const v = $("video");
  markCam(false);
  v.src = "/stream?t=" + Date.now();
}

$("flipBtn").addEventListener("click", async () => {
  const next = !state.flip;
  const r = await api("/api/camera", { flip: next ? 1 : 0 });
  if (r.ok) state.flip = next;
  refreshVideo();
});

$("photoBtn").addEventListener("click", async () => {
  try {
    const res = await fetch("/capture?t=" + Date.now(), { headers: authHeaders() });
    if (!res.ok) return;
    const blob = await res.blob();
    const a = document.createElement("a");
    const url = URL.createObjectURL(blob);
    a.href = url;
    a.download = "hero_" + new Date().toISOString().replace(/[:.]/g, "-") + ".jpg";
    a.rel = "noopener";
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 3000);
  } catch (e) { /* ignore */ }
  refreshVideo();
});

/* ------- clip recording (client-side MediaRecorder) ------- */
let recorder = null;
let recCanvas = null;
let recRaf = 0;

function pickMime() {
  const m = ["video/webm;codecs=vp9", "video/webm;codecs=vp8", "video/webm"];
  for (const c of m) { if (window.MediaRecorder && MediaRecorder.isTypeSupported(c)) return c; }
  return "";
}

function downloadBlob(blob, name) {
  const a = document.createElement("a");
  const url = URL.createObjectURL(blob);
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 3000);
}

function stopRecording() {
  if (recorder) {
    recorder.stop();
    recorder = null;
  }
  if (recRaf) { cancelAnimationFrame(recRaf); recRaf = 0; }
  $("recBtn").classList.remove("recording");
  $("videoBadge").classList.add("hidden");
}

$("recBtn").addEventListener("click", () => {
  if (recorder) { stopRecording(); return; }
  const v = $("video");
  if (!v.complete || v.naturalWidth === 0) return;

  recCanvas = document.createElement("canvas");
  recCanvas.width = v.naturalWidth || 800;
  recCanvas.height = v.naturalHeight || 600;
  const ctx = recCanvas.getContext("2d");
  const stream = recCanvas.captureStream(20);
  const mime = pickMime();
  if (!mime) { $("videoBadge").textContent = "recording unsupported"; $("videoBadge").classList.remove("hidden"); return; }

  const chunks = [];
  recorder = new MediaRecorder(stream, { mimeType: mime });
  recorder.ondataavailable = (e) => { if (e.data.size) chunks.push(e.data); };
  recorder.onstop = () => {
    downloadBlob(new Blob(chunks, { type: mime }), "hero_clip.webm");
    recCanvas = null;
  };
  recorder.start(1000);

  const draw = () => {
    if (!recCanvas) return;
    const w = v.naturalWidth || recCanvas.width;
    const h = v.naturalHeight || recCanvas.height;
    if (recCanvas.width !== w) recCanvas.width = w;
    if (recCanvas.height !== h) recCanvas.height = h;
    ctx.drawImage(v, 0, 0, recCanvas.width, recCanvas.height);
    recRaf = requestAnimationFrame(draw);
  };
  draw();
  $("recBtn").classList.add("recording");
  $("videoBadge").textContent = "REC";
  $("videoBadge").classList.remove("hidden");
});

/* ---------------- drive (4-button pad) ---------------- */
const held = new Set();
let driveKeepAlive = null;

function computeDrive() {
  const sp = state.speedScale;
  let throttle = 0, steering = 0;
  if (held.has("up")) throttle += sp;
  if (held.has("down")) throttle -= sp;
  if (held.has("left")) steering -= sp;
  if (held.has("right")) steering += sp;
  state.throttle = throttle;
  state.steering = steering;
  api("/api/drive", { throttle, steering });
}

// While any button is held, re-send the drive command every 300 ms. The rover
// stops the motors if no drive/stop command arrives for ~1.5 s, so this also
// acts as a keep-alive for the firmware's disconnect safety watchdog.
function startDriveKeepAlive() {
  if (driveKeepAlive) return;
  driveKeepAlive = setInterval(() => {
    if (held.size > 0) computeDrive();
  }, 300);
}
function stopDriveKeepAlive() {
  if (driveKeepAlive) { clearInterval(driveKeepAlive); driveKeepAlive = null; }
}

function stopMotorsLocal() {
  held.clear();
  stopDriveKeepAlive();
  ["btnUp", "btnDown", "btnLeft", "btnRight"].forEach((id) => {
    const el = $(id);
    if (el) el.classList.remove("active");
  });
  state.throttle = 0;
  state.steering = 0;
  if (state.token) api("/api/stop", {});
}

document.addEventListener("visibilitychange", () => {
  if (document.hidden) stopMotorsLocal();
});
window.addEventListener("pagehide", stopMotorsLocal);

function bindCtrl(id, key) {
  const el = $(id);
  const down = (e) => {
    e.preventDefault();
    if (e.pointerId !== undefined && el.setPointerCapture) {
      try { el.setPointerCapture(e.pointerId); } catch (_) {}
    }
    held.add(key);
    el.classList.add("active");
    computeDrive();
    startDriveKeepAlive();
  };
  const up = (e) => {
    e.preventDefault();
    held.delete(key);
    el.classList.remove("active");
    if (held.size === 0) {
      state.throttle = 0;
      state.steering = 0;
      api("/api/stop", {});
      stopDriveKeepAlive();
    } else {
      computeDrive();
    }
  };
  el.addEventListener("pointerdown", down);
  el.addEventListener("pointerup", up);
  el.addEventListener("pointercancel", up);
  el.addEventListener("lostpointercapture", () => {
    if (held.has(key)) up({ preventDefault() {} });
  });
  el.addEventListener("contextmenu", (e) => e.preventDefault());
}
bindCtrl("btnUp", "up");
bindCtrl("btnDown", "down");
bindCtrl("btnLeft", "left");
bindCtrl("btnRight", "right");

/* ---------------- speed ---------------- */
const speedInput = $("speed");
const speedVal = $("speedVal");
speedInput.addEventListener("input", () => {
  state.speedScale = parseInt(speedInput.value, 10);
  speedVal.textContent = state.speedScale;
  if (navigator.vibrate) navigator.vibrate(10);
});

/* ---------------- mood ---------------- */
async function setMood(m) {
  const r = await api("/api/mood", { mood: m });
  if (r.ok) {
    state.mood = m;
    $("moodSelect").value = m;
  }
}

$("moodSelect").addEventListener("change", () => setMood($("moodSelect").value));

$("moodBtn").addEventListener("click", () => {
  $("moodPopup").classList.toggle("hidden");
});
document.querySelectorAll(".mood-opt").forEach((b) => {
  b.addEventListener("click", () => {
    setMood(b.dataset.mood);
    $("moodPopup").classList.add("hidden");
  });
});
document.addEventListener("pointerdown", (e) => {
  if (!$("moodPopup").classList.contains("hidden") &&
      !e.target.closest("#moodPopup") && e.target.id !== "moodBtn") {
    $("moodPopup").classList.add("hidden");
  }
});

/* ---------------- message ---------------- */
function updateMsgState(active) {
  $("msgClear").classList.toggle("hidden", !active);
  $("msgInput").placeholder = active ? "New message replaces current" : "Message to OLED...";
}
$("msgSend").addEventListener("click", async () => {
  const t = $("msgInput").value.trim();
  if (!t) return;
  const r = await api("/api/message", { text: t });
  if (r.ok) { updateMsgState(true); }
  $("msgInput").value = "";
});
$("msgInput").addEventListener("keydown", (e) => { if (e.key === "Enter") $("msgSend").click(); });
$("msgClear").addEventListener("click", async () => {
  const r = await api("/api/message", {});
  if (r.ok) updateMsgState(false);
});

/* ---------------- flash ---------------- */
async function setFlashlight(on) {
  const prev = state.flashlight;
  state.flashlight = !!on;
  $("flashBtn").classList.toggle("on", state.flashlight);
  const r = await api("/api/flash", { on: state.flashlight ? 1 : 0 });
  if (!r.ok) {
    state.flashlight = prev;
    $("flashBtn").classList.toggle("on", prev);
  }
}
$("flashBtn").addEventListener("click", () => setFlashlight(!state.flashlight));

/* ---------------- settings modal ---------------- */
function clampTrim(v) {
  const n = parseInt(v, 10);
  if (!Number.isFinite(n)) return 100;
  return Math.max(50, Math.min(100, n));
}

function syncTrimUi() {
  const l = clampTrim(state.trimLeft);
  const r = clampTrim(state.trimRight);
  $("trimLeft").value = l;
  $("trimRight").value = r;
  $("trimLeftVal").textContent = l;
  $("trimRightVal").textContent = r;
}

function openSettings() {
  $("setTheme").value = localStorage.getItem("hero_theme") || "dark";
  $("revLeft").checked = !!state.reverseLeft;
  $("revRight").checked = !!state.reverseRight;
  syncTrimUi();
  $("setToken").value = "";
  $("settingsMsg").textContent = "";
  $("settingsModal").classList.remove("hidden");
}
$("settingsBtn").addEventListener("click", openSettings);
$("settingsClose").addEventListener("click", () => $("settingsModal").classList.add("hidden"));

$("trimLeft").addEventListener("input", () => {
  $("trimLeftVal").textContent = $("trimLeft").value;
});
$("trimRight").addEventListener("input", () => {
  $("trimRightVal").textContent = $("trimRight").value;
});

$("settingsSave").addEventListener("click", async () => {
  const msg = $("settingsMsg");
  const theme = $("setTheme").value;
  localStorage.setItem("hero_theme", theme);
  applyTheme(theme);

  const revL = $("revLeft").checked;
  const revR = $("revRight").checked;
  const trimL = clampTrim($("trimLeft").value);
  const trimR = clampTrim($("trimRight").value);
  const mot = await api("/api/settings", {
    reverseLeft: revL ? 1 : 0,
    reverseRight: revR ? 1 : 0,
    trimLeft: trimL,
    trimRight: trimR,
  });
  if (mot.ok) {
    state.reverseLeft = revL;
    state.reverseRight = revR;
    state.trimLeft = trimL;
    state.trimRight = trimR;
  }

  const newToken = $("setToken").value.trim();
  if (newToken) {
    const r = await api("/api/token", { token: newToken });
    if (r.ok) {
      state.token = newToken;
      localStorage.setItem("hero_token", newToken);
      msg.textContent = "Token updated. Saved.";
    } else {
      msg.textContent = "Token must be at least 4 characters.";
      return;
    }
  } else {
    msg.textContent = "Saved.";
  }
  setTimeout(() => $("settingsModal").classList.add("hidden"), 1200);
});

/* ---------------- state polling ---------------- */
let pollTimer = null;

function lockOut() {
  stopMotorsLocal();
  $("app").classList.add("hidden");
  localStorage.removeItem("hero_token");
  state.token = "";
  $("authGate").classList.remove("hidden");
}

async function pollState() {
  const r = await api("/api/state");
  if (!r.ok) {
    $("dotCtrl").classList.toggle("on", false);
    if (r.status === 401) lockOut();
    return;
  }
  const s = r.data;
  state.ip = s.ip;
  state.flip = !!s.camFlip;
  state.flashlight = !!s.flashlightOn;
  state.messageActive = !!s.messageActive;
  updateMsgState(state.messageActive);
  $("flashBtn").classList.toggle("on", state.flashlight);
  $("dotCtrl").classList.toggle("on", true);
  $("dotCam").classList.toggle("on", state.camLive);
  const settingsOpen = !$("settingsModal").classList.contains("hidden");
  if (!settingsOpen) {
    if (typeof s.reverseLeft === "boolean") state.reverseLeft = s.reverseLeft;
    if (typeof s.reverseRight === "boolean") state.reverseRight = s.reverseRight;
    if (typeof s.trimLeft === "number") state.trimLeft = clampTrim(s.trimLeft);
    if (typeof s.trimRight === "number") state.trimRight = clampTrim(s.trimRight);
  }
  if (s.mood && s.mood !== state.mood) {
    state.mood = s.mood;
    $("moodSelect").value = s.mood;
  }
}

/* ---------------- cockpit init ---------------- */
function initCockpit(data) {
  state.ip = data.ip || "192.168.4.1";
  state.flashlight = !!data.flashlightOn;
  state.flip = !!data.camFlip;
  state.mood = data.mood || "idle";
  state.reverseLeft = !!data.reverseLeft;
  state.reverseRight = !!data.reverseRight;
  state.trimLeft = clampTrim(data.trimLeft);
  state.trimRight = clampTrim(data.trimRight);
  $("moodSelect").value = state.mood;
  updateMsgState(!!data.messageActive);
  $("flashBtn").classList.toggle("on", state.flashlight);
  $("dotCtrl").classList.toggle("on", true);
  startVideo();
  if (!pollTimer) pollTimer = setInterval(pollState, 3000);
}

boot();
