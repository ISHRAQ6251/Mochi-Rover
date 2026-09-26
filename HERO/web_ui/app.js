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
  camEnabled: false,
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
function showGate(msg) {
  $("app").classList.add("hidden");
  $("authGate").classList.remove("hidden");
  $("authErr").textContent = msg || "";
  const t = $("authToken");
  if (t) t.focus();
}

async function boot() {
  if (!state.token) {
    showGate("");
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
    showGate("");
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
let videoRetry = 0;
let camBusy = false;

function setStreamUi(on) {
  const b = $("streamBtn");
  if (!b) return;
  b.classList.toggle("on", !!on);
  b.classList.toggle("stopped", !on);
  b.setAttribute("aria-pressed", on ? "true" : "false");
  const label = on ? "Stop camera" : "Start camera";
  b.title = label;
  b.setAttribute("aria-label", label);
}

function markCam(live) {
  state.camLive = !!live;
  const d = $("dotCam");
  if (d) d.classList.toggle("on", state.camLive);
  const off = $("videoOffline");
  if (off) {
    off.classList.toggle("hidden", state.camLive);
    const span = off.querySelector("span");
    const spin = off.querySelector(".spinner");
    if (span) span.textContent = state.camEnabled ? "Connecting to camera..." : "Camera stopped";
    if (spin) spin.classList.toggle("hidden", !state.camEnabled);
  }
}

function startVideo() {
  state.camEnabled = true;
  setStreamUi(true);
  const v = $("video");
  if (videoRetry) { clearTimeout(videoRetry); videoRetry = 0; }
  v.onload = () => { markCam(true); };
  v.onerror = () => {
    markCam(false);
    if (!state.camEnabled) return;
    videoRetry = setTimeout(() => { v.src = "/stream?t=" + Date.now(); }, 400);
  };
  markCam(false);
  v.src = "/stream?t=" + Date.now();
}

function stopVideoLocal() {
  state.camEnabled = false;
  if (videoRetry) { clearTimeout(videoRetry); videoRetry = 0; }
  if (recorder) stopRecording();
  const v = $("video");
  v.onload = null;
  v.onerror = null;
  v.removeAttribute("src");
  v.src = "";
  markCam(false);
  setStreamUi(false);
}

function refreshVideo() {
  if (!state.camEnabled) return;
  const v = $("video");
  markCam(false);
  v.src = "/stream?t=" + Date.now();
}

$("streamBtn").addEventListener("click", async () => {
  if (camBusy) return;
  camBusy = true;
  try {
    if (state.camEnabled) {
      stopVideoLocal();
      await api("/api/camera", { on: 0 });
      return;
    }
    const r = await api("/api/camera", { on: 1 });
    if (r.ok) startVideo();
    else { setStreamUi(false); markCam(false); }
  } finally {
    camBusy = false;
  }
});

$("flipBtn").addEventListener("click", async () => {
  if (!state.camEnabled) return;
  const next = !state.flip;
  const r = await api("/api/camera", { flip: next ? 1 : 0 });
  if (r.ok) state.flip = next;
  refreshVideo();
});

$("photoBtn").addEventListener("click", async () => {
  if (!state.camEnabled) return;
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
  $("recBtn").setAttribute("aria-pressed", "false");
  $("videoBadge").classList.add("hidden");
}

$("recBtn").addEventListener("click", () => {
  if (recorder) { stopRecording(); return; }
  if (!state.camEnabled) return;
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
  $("recBtn").setAttribute("aria-pressed", "true");
  $("videoBadge").textContent = "REC";
  $("videoBadge").classList.remove("hidden");
});

/* ---------------- drive (4-button pad) ---------------- */
const held = new Set();
let driveKeepAlive = null;
let driveBusy = false;
let driveQueued = null;

function flushDrive() {
  if (driveBusy || !driveQueued) return;
  driveBusy = true;
  const q = driveQueued;
  driveQueued = null;
  const p = q.stop ? api("/api/stop", {}) : api("/api/drive", { throttle: q.throttle, steering: q.steering });
  Promise.resolve(p).finally(() => {
    driveBusy = false;
    if (driveQueued) flushDrive();
  });
}

function queueDrive(throttle, steering) {
  driveQueued = { stop: false, throttle, steering };
  flushDrive();
}

function queueStop() {
  driveQueued = { stop: true };
  flushDrive();
}

function computeDrive() {
  const sp = state.speedScale;
  let throttle = 0, steering = 0;
  if (held.has("up")) throttle += sp;
  if (held.has("down")) throttle -= sp;
  if (held.has("left")) steering += sp;
  if (held.has("right")) steering -= sp;
  state.throttle = throttle;
  state.steering = steering;
  queueDrive(throttle, steering);
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
  if (state.token) queueStop();
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
      queueStop();
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
  // Keyboard parity for the drive pad: hold Enter/Space to drive, release to stop.
  el.addEventListener("keydown", (e) => {
    if ((e.key === "Enter" || e.key === " ") && !e.repeat) {
      e.preventDefault();
      down(e);
    }
  });
  el.addEventListener("keyup", (e) => {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      up(e);
    }
  });
  el.addEventListener("blur", () => {
    if (held.has(key)) up({ preventDefault() {} });
  });
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

function setMoodPopup(open) {
  $("moodPopup").classList.toggle("hidden", !open);
  $("moodBtn").setAttribute("aria-expanded", open ? "true" : "false");
  if (open) {
    const first = $("moodPopup").querySelector(".mood-opt");
    if (first) first.focus();
  }
}

$("moodBtn").addEventListener("click", () => {
  setMoodPopup($("moodPopup").classList.contains("hidden"));
});
document.querySelectorAll(".mood-opt").forEach((b) => {
  b.addEventListener("click", () => {
    setMood(b.dataset.mood);
    setMoodPopup(false);
    $("moodBtn").focus();
  });
});
document.addEventListener("pointerdown", (e) => {
  if (!$("moodPopup").classList.contains("hidden") &&
      !e.target.closest("#moodPopup") && e.target.id !== "moodBtn") {
    setMoodPopup(false);
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
  if (r.ok) { updateMsgState(true); $("msgLive").textContent = "Message sent to OLED"; }
  $("msgInput").value = "";
});
$("msgInput").addEventListener("keydown", (e) => { if (e.key === "Enter") $("msgSend").click(); });
$("msgClear").addEventListener("click", async () => {
  const r = await api("/api/message", {});
  if (r.ok) { updateMsgState(false); $("msgLive").textContent = "OLED message cleared"; }
});

/* ---------------- flash ---------------- */
function setFlashUi(on) {
  state.flashlight = !!on;
  const t = $("flashToggle");
  if (t) t.checked = state.flashlight;
}

async function setFlashlight(on) {
  const prev = state.flashlight;
  setFlashUi(on);
  const r = await api("/api/flash", { on: state.flashlight ? 1 : 0 });
  if (!r.ok) setFlashUi(prev);
}
$("flashToggle").addEventListener("change", () => setFlashlight($("flashToggle").checked));

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

let lastFocus = null;

function openSettings() {
  lastFocus = document.activeElement;
  $("setTheme").value = localStorage.getItem("hero_theme") || "dark";
  setFlashUi(state.flashlight);
  $("revLeft").checked = !!state.reverseLeft;
  $("revRight").checked = !!state.reverseRight;
  syncTrimUi();
  $("setToken").value = "";
  $("settingsMsg").textContent = "";
  $("settingsModal").classList.remove("hidden");
  $("settingsClose").focus();
}
function closeSettings() {
  $("settingsModal").classList.add("hidden");
  if (lastFocus && typeof lastFocus.focus === "function") lastFocus.focus();
}
$("settingsBtn").addEventListener("click", openSettings);
$("settingsClose").addEventListener("click", closeSettings);

// Keep keyboard focus inside the modal while it is open.
function trapFocus(e, root) {
  const items = root.querySelectorAll(
    'button, input, select, textarea, a[href], [tabindex]:not([tabindex="-1"])'
  );
  const visible = Array.prototype.filter.call(items, (el) => el.offsetParent !== null);
  if (visible.length === 0) return;
  const first = visible[0];
  const last = visible[visible.length - 1];
  if (e.shiftKey && document.activeElement === first) {
    e.preventDefault();
    last.focus();
  } else if (!e.shiftKey && document.activeElement === last) {
    e.preventDefault();
    first.focus();
  }
}

document.addEventListener("keydown", (e) => {
  const modalOpen = !$("settingsModal").classList.contains("hidden");
  const popupOpen = !$("moodPopup").classList.contains("hidden");
  if (e.key === "Escape") {
    if (modalOpen) { closeSettings(); return; }
    if (popupOpen) { setMoodPopup(false); $("moodBtn").focus(); }
    return;
  }
  if (e.key === "Tab" && modalOpen) {
    trapFocus(e, $("settingsModal").querySelector(".modal-card"));
  }
});

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
      $("setToken").focus();
      return;
    }
  } else {
    msg.textContent = "Saved.";
  }
  setTimeout(closeSettings, 1200);
});

/* ---------------- state polling ---------------- */
let pollTimer = null;

function lockOut() {
  stopMotorsLocal();
  localStorage.removeItem("hero_token");
  state.token = "";
  showGate("Session expired. Enter the token again.");
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
  if (!camBusy && typeof s.camRunning === "boolean" && s.camRunning !== state.camEnabled) {
    if (s.camRunning) startVideo();
    else stopVideoLocal();
  }
  state.flashlight = !!s.flashlightOn;
  state.messageActive = !!s.messageActive;
  updateMsgState(state.messageActive);
  setFlashUi(state.flashlight);
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
  setFlashUi(state.flashlight);
  $("dotCtrl").classList.toggle("on", true);
  if (data.camRunning === true) startVideo();
  else stopVideoLocal();
  if (!pollTimer) pollTimer = setInterval(pollState, 3000);
}

boot();
