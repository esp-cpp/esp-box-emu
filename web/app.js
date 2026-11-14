// Emulator Console: the web app the emulators advertise as their WebUSB
// landing page. Shared, byte for byte, by esp-box-emu and tab5-emu.
import { BUTTONS, Control, ControlError, DEVICES, MODULE, PID, ROOT, UsbLink, VID, rgb565ToImageData } from "./link.js";

const $ = (id) => document.getElementById(id);
const h = (tag, attrs = {}, ...kids) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (v == null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else if (k === "style") e.style.cssText = v;
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const kid of kids.flat()) if (kid != null && kid !== false) e.append(kid.nodeType ? kid : String(kid));
  return e;
};

// the consoles' accent colors (rom_info.cpp, both firmwares)
const CONSOLE_COLORS = {
  nes: "#B22222", snes: "#6A5ACD", gb: "#5B7F3A", gbc: "#8E44AD", sms: "#1F5FAF", gg: "#4F6D8F",
  genesis: "#C8102E", msx: "#3A7D7D", pc: "#5A6B7B", pico8: "#FF004D", gba: "#4B3FA8", playstation: "#2E6DB4",
  other: "#606060",
};
const ESPP_APPS = "https://esp-cpp.github.io/espp/apps/";

const state = {
  link: null, ctl: null, discovery: null, status: null, roms: [], romsLoaded: false,
  tab: "dashboard", statusTimer: null, lastFrames: null, fpsHistory: [], memHistory: [],
  filter: "", search: "", cwd: ROOT, files: [], live: false, lastShot: null, slot: 1,
  boxart: new Map(), boxartQueue: [], boxartBusy: false, settings: null, serial: null,
  // a device that comes back is opened again without the chooser, unless the
  // page let go of it on purpose (Disconnect, or a hand-off to another app)
  autoReconnect: true,
  session: 0, // bumped on every disconnect: replies for an older one are dropped
};

// ------------------------------------------------------------------ helpers --
function toast(text, err = false) {
  const t = $("toast");
  t.textContent = text; t.className = "toast" + (err ? " err" : ""); t.hidden = false;
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => (t.hidden = true), err ? 6000 : 3000);
}

function confirmModal(title, text, ok = "OK") {
  return new Promise((resolve) => {
    $("modalTitle").textContent = title; $("modalText").textContent = text; $("modalOk").textContent = ok;
    // the page behind is inert (no Tab or clicks into it) until the answer
    confirmModal.pending?.(false); // (a question still open is answered "no")
    const back = [...document.body.children].filter((e) => e !== $("modal") && e !== $("toast") && !e.inert);
    const opener = document.activeElement;
    for (const e of back) e.inert = true;
    $("modal").hidden = false; $("modalOk").focus();
    const done = (v) => {
      confirmModal.pending = null;
      $("modal").hidden = true; $("modalOk").onclick = $("modalCancel").onclick = $("modal").onkeydown = null;
      for (const e of back) e.inert = false;
      opener?.focus?.();
      resolve(v);
    };
    $("modalOk").onclick = () => done(true);
    $("modalCancel").onclick = () => done(false);
    confirmModal.pending = done;
    $("modal").onkeydown = (ev) => {
      if (ev.key === "Escape") { ev.preventDefault(); done(false); }
      // Tab stays on the two buttons
      if (ev.key === "Tab") { ev.preventDefault(); (document.activeElement === $("modalOk") ? $("modalCancel") : $("modalOk")).focus(); }
    };
  });
}

const fmtBytes = (n) => n == null ? "—" : n >= 1 << 20 ? (n / (1 << 20)).toFixed(2) + " MB" : n >= 1024 ? (n / 1024).toFixed(1) + " KB" : n + " B";
const fmtUptime = (ms) => {
  if (ms == null) return "—";
  const s = Math.floor(ms / 1000), d = Math.floor(s / 86400), hh = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return (d ? d + "d " : "") + (d || hh ? hh + "h " : "") + m + "m " + (s % 60) + "s";
};
const basename = (p) => p.split("/").filter(Boolean).pop() || p;
const dirname = (p) => p.replace(/\/[^/]*\/?$/, "") || ROOT;
const badge = (key, name) => h("span", { class: "badge", style: `background:${CONSOLE_COLORS[key] || CONSOLE_COLORS.other}` }, name || key);

async function guarded(fn, okText) {
  try {
    const r = await fn();
    if (okText) toast(okText);
    return r;
  } catch (e) {
    toast(e.message || String(e), true);
    return undefined;
  }
}

function download(name, data, type = "application/octet-stream") {
  const url = URL.createObjectURL(new Blob([data], { type }));
  const a = h("a", { href: url, download: name });
  document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 5000);
}

// -------------------------------------------------------------- connection --
async function connect(device) {
  if (state.link) await disconnect();
  setPill("", "Connecting…");
  let link;
  try {
    link = device ? new UsbLink(device) : await UsbLink.request();
    await link.open();
  } catch (e) {
    setPill("", "Not connected");
    if (e && e.name === "NotFoundError") return; // the chooser was closed
    const busy = /claim|access|busy|in use/i.test(String(e.message || e));
    toast(busy ? "The link is in use: close emulink (or another tab using it) and connect again." : (e.message || String(e)), true);
    try { await link?.close(); } catch (_) {}
    return;
  }
  state.link = link;
  state.ctl = new Control(link);
  state.autoReconnect = true;
  // input a page lost track of (its link died with a button held) is let go
  state.ctl.buttons(0, 0).catch(() => {});
  link.onclose = () => onLinkLost();
  try { state.discovery = await link.discover(); } catch (_) { state.discovery = null; }
  if (state.link !== link) return; // (lost while discovering: already shown as disconnected)
  try { localStorage.setItem("emu-console-serial", link.serial); } catch (_) {}
  $("connectBtn").hidden = true; $("disconnectBtn").hidden = false;
  $("welcome").hidden = true;
  $("deviceSub").textContent = `${link.known.name} · ${link.serial || "?"}`;
  document.title = `${link.known.name} · Emulator Console`;
  const control = state.discovery?.modules.find((m) => m.id === MODULE.CONTROL);
  if (state.discovery && !control) toast("This device has no emulator control module", true);
  else if (control && control.version < 4) toast(`Firmware speaks control protocol ${control.version}: update it for every feature here (4).`, true);
  showTab(state.tab);
  if (state.tab !== "library") loadLibrary().catch(() => {}); // (the names for the status)
  pollStatus();
  state.statusTimer = setInterval(pollStatus, 1000);
}

async function disconnect() {
  clearInterval(state.statusTimer); state.statusTimer = null;
  stopLive();
  const link = state.link, ctl = state.ctl;
  state.session++;
  state.link = null; state.ctl = null; state.status = null; state.romsLoaded = false; state.settings = null;
  // a button held from this page would stay pressed in the game
  if (heldMask && ctl) {
    await Promise.race([ctl.buttons(0, 0).catch(() => {}), new Promise((r) => setTimeout(r, 500))]);
  }
  releasePad();
  clearDeviceCaches();
  if (link) { link.onclose = null; await link.close(); }
  $("connectBtn").hidden = false; $("disconnectBtn").hidden = true;
  $("welcome").hidden = false;
  for (const p of document.querySelectorAll("[data-panel]")) p.hidden = true;
  setPill("", "Not connected");
  $("deviceSub").textContent = "esp-box-emu · tab5-emu over USB";
}

function onLinkLost() {
  const serial = state.link?.serial;
  disconnect().then(() => {
    setPill("warn", "Disconnected");
    if (!state.autoReconnect) return; // (handed off: the updater went down with it)
    toast("The device went away (restarted or unplugged); it reconnects when it is back.");
    state.waitingFor = serial;
  });
}

// Images and the last screenshot belong to the device (and SD card) they came
// from: another board can have the same paths.
function clearDeviceCaches() {
  for (const url of state.boxart.values()) if (url) URL.revokeObjectURL(url);
  state.boxart.clear();
  for (const { done } of state.boxartQueue.splice(0)) done(null);
  state.lastShot = null;
  const c = $("screen"); c.getContext("2d").clearRect(0, 0, c.width, c.height);
  $("screenInfo").textContent = ""; $("savePngBtn").disabled = true;
}

// a device that comes back (after a restart) is opened again without the chooser
if (navigator.usb) {
  navigator.usb.addEventListener("connect", (ev) => {
    const ok = () => state.autoReconnect && !state.link && DEVICES[ev.device.serialNumber] &&
      (!state.waitingFor || ev.device.serialNumber === state.waitingFor);
    if (ok()) setTimeout(() => { if (ok()) connect(ev.device); }, 1200);
  });
}

function setPill(cls, text) {
  const p = $("statusPill");
  p.className = "pill" + (cls ? " " + cls : ""); p.textContent = text;
}

async function autoConnect() {
  const q = new URLSearchParams(location.search);
  const wanted = q.get("serial") || (() => { try { return localStorage.getItem("emu-console-serial"); } catch (_) { return null; } })();
  const devs = await UsbLink.permitted();
  const dev = devs.find((d) => d.serialNumber === wanted) || devs.find((d) => DEVICES[d.serialNumber]);
  if (dev && (q.get("autoconnect") || DEVICES[dev.serialNumber])) connect(dev);
}

// ----------------------------------------------------------------- status --
async function pollStatus() {
  if (!state.ctl || pollStatus.busy) return;
  pollStatus.busy = true;
  try {
    const st = await state.ctl.status();
    const now = performance.now();
    if (state.lastFrames) {
      const dt = (now - state.lastFrames.t) / 1000;
      const fps = dt > 0 ? (st.frames_presented - state.lastFrames.n) / dt : 0;
      state.fpsHistory.push(Math.max(0, fps));
      if (state.fpsHistory.length > 120) state.fpsHistory.shift();
    }
    state.lastFrames = { t: now, n: st.frames_presented };
    state.memHistory.push([st.internal_free, st.psram_free]);
    if (state.memHistory.length > 120) state.memHistory.shift();
    const prev = state.status;
    state.status = st;
    renderStatus(prev);
  } catch (e) {
    if (state.link) setPill("warn", "Busy");
  } finally {
    pollStatus.busy = false;
  }
}

function renderStatus(prev) {
  const st = state.status;
  if (!st) return;
  const rom = state.roms.find((r) => r.path === st.rom);
  if (st.usb_drive) setPill("warn", "SD card on the PC");
  else if (st.state === "game") setPill("playing", "Playing " + (rom?.name || basename(st.rom)));
  else setPill("ok", "In the launcher");

  $("nowState").textContent = st.state === "game" ? "Playing" : "In the launcher";
  $("nowGame").textContent = st.state === "game" ? (rom?.name || basename(st.rom)) : "No game running";
  $("nowConsole").replaceChildren(st.state === "game" && st.console ? badge(rom?.key, st.console) : "");
  $("nowQuitBtn").hidden = st.state !== "game";
  const art = st.state === "game" && rom?.boxart ? state.boxart.get(rom.boxart) : null;
  if (art) $("nowArt").src = art; else $("nowArt").removeAttribute("src");
  if (st.state === "game" && rom?.boxart && !state.boxart.has(rom.boxart)) queueBoxart(rom.boxart, () => renderStatus());

  const fps = state.fpsHistory.at(-1);
  $("fps").textContent = fps == null ? "—" : fps.toFixed(1) + " / s";
  $("dropped").textContent = st.frames_dropped;
  $("gameFrames").textContent = st.state === "game" ? st.game_frames : "—";
  drawSpark($("fpsSpark"), [state.fpsHistory], ["#3e8ef7"]);
  const memRow = (label, free, largest, min) => h("div", { class: "mem-row" },
    h("span", {}, label),
    h("span", { class: "num strong" }, fmtBytes(free) + " free"),
    h("span", { class: "num" }, `largest ${fmtBytes(largest)}` + (min != null ? ` · lowest ${fmtBytes(min)}` : "")));
  $("memRows").replaceChildren(
    memRow("Internal", st.internal_free, st.internal_largest, st.internal_min_free),
    memRow("PSRAM", st.psram_free, st.psram_largest, st.psram_min_free),
    memRow("DMA", st.dma_free, st.dma_largest, null));
  drawSpark($("memSpark"), [state.memHistory.map((m) => m[0]), state.memHistory.map((m) => m[1])], ["#30a46c", "#f5a524"]);
  $("fw").textContent = st.firmware; $("built").textContent = st.built;
  $("partition").textContent = st.partition + (st.elf_sha256 ? ` · ${st.elf_sha256}` : "");
  $("uptime").textContent = fmtUptime(st.uptime_ms);
  $("reset").textContent = ({ 1: "power-on", 3: "software", 4: "panic", 5: "interrupt WDT", 6: "task WDT", 7: "other WDT", 9: "brownout", 11: "USB" })[st.reset_reason] || `#${st.reset_reason}`;
  $("driveText").textContent = st.usb_drive ? "The PC has the SD card (a USB drive)." : "The emulator has the SD card.";
  $("driveBtn").textContent = st.usb_drive ? "Take the SD card back" : "Give the SD card to this PC";
  $("panicCard").hidden = !st.last_panic;
  $("panicText").textContent = st.last_panic || "";
  $("coredumpLink").href = handoffUrl("coredump_console.html", MODULE.COREDUMP);
  // the game started or ended: the list, the screen and the buttons follow
  if (prev && (prev.state !== st.state || prev.rom !== st.rom)) {
    if (state.tab === "library") renderLibrary();
    if (state.tab === "screen" && !state.live) capture();
  }
  if (prev?.usb_drive && !st.usb_drive) forgetBoxartFailures();
  if (prev && prev.filter !== st.filter && state.tab === "library") { state.filter = st.filter || ""; renderLibrary(); }
}

function drawSpark(canvas, series, colors) {
  const dpr = window.devicePixelRatio || 1, w = canvas.clientWidth || 300, hh = canvas.clientHeight || 56;
  if (canvas.width !== w * dpr) { canvas.width = w * dpr; canvas.height = hh * dpr; }
  const g = canvas.getContext("2d");
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, w, hh);
  series.forEach((s, k) => {
    if (s.length < 2) return;
    const max = Math.max(...s) * 1.1 || 1, min = 0;
    g.strokeStyle = colors[k]; g.lineWidth = 1.5; g.beginPath();
    s.forEach((v, i) => {
      const x = (i / (s.length - 1)) * w, y = hh - 2 - ((v - min) / (max - min)) * (hh - 4);
      i ? g.lineTo(x, y) : g.moveTo(x, y);
    });
    g.stroke();
  });
}

// ------------------------------------------------------------------- tabs --
function showTab(tab) {
  state.tab = tab;
  for (const b of document.querySelectorAll("#tabs button")) b.classList.toggle("active", b.dataset.tab === tab);
  if (!state.link) return;
  for (const p of document.querySelectorAll("[data-panel]")) p.hidden = p.dataset.panel !== tab;
  if (tab !== "screen") stopLive();
  if (tab === "library") loadLibrary();
  if (tab === "files") loadFiles(state.cwd);
  if (tab === "settings") loadSettings();
  if (tab === "system") loadSystem();
  if (tab === "screen" && !state.lastShot) capture();
}

// ----------------------------------------------------------------- screen --
async function capture() {
  if (!state.ctl || capture.busy) return;
  capture.busy = true;
  const prog = $("screenProgress");
  const t0 = performance.now(), session = state.session;
  try {
    const f = await state.ctl.frame((n, total) => { prog.hidden = false; prog.max = total; prog.value = n; });
    if (session !== state.session) return; // (disconnected meanwhile)
    const c = $("screen");
    c.width = f.width; c.height = f.height;
    c.getContext("2d").putImageData(rgb565ToImageData(f.data, f.width, f.height, f.format === "rgb565be"), 0, 0);
    state.lastShot = f;
    $("screenInfo").textContent = `${f.source === "launcher" ? "Launcher" : "Game"} · ${f.width}×${f.height} · ${Math.round(performance.now() - t0)} ms`;
    $("savePngBtn").disabled = false;
  } catch (e) {
    $("screenInfo").textContent = e.message || String(e);
  } finally {
    prog.hidden = true;
    capture.busy = false;
  }
}

async function liveLoop() {
  while (state.live && state.ctl && state.tab === "screen") {
    await capture();
    await new Promise((r) => setTimeout(r, 150));
  }
}
function stopLive() { state.live = false; $("liveToggle").checked = false; }

let heldMask = 0;
const setButtons = (mask) => {
  heldMask = mask;
  if (state.ctl) state.ctl.buttons(mask, 0).catch((e) => toast("Buttons: " + (e.message || e), true));
};
// forget the held buttons (the link is going: the device side is released by the caller)
function releasePad() {
  heldMask = 0;
  for (const b of document.querySelectorAll("#pad .down-now")) b.classList.remove("down-now");
}
function initPad() {
  for (const b of document.querySelectorAll("#pad [data-btn]")) {
    const bit = BUTTONS[b.dataset.btn];
    const down = (ev) => { ev.preventDefault(); b.classList.add("down-now"); b.setPointerCapture?.(ev.pointerId); setButtons(heldMask | bit); };
    const up = () => { if (!(heldMask & bit)) return; b.classList.remove("down-now"); setButtons(heldMask & ~bit); };
    b.addEventListener("pointerdown", down);
    b.addEventListener("pointerup", up);
    b.addEventListener("pointercancel", up);
    b.addEventListener("lostpointercapture", up);
    // the keyboard's activation keys on a focused pad button press that button
    const activate = (ev) => ev.key === "Enter" || ev.key === " ";
    b.addEventListener("keydown", (ev) => {
      if (!activate(ev)) return;
      ev.preventDefault(); ev.stopPropagation();
      if (!ev.repeat) { b.classList.add("down-now"); setButtons(heldMask | bit); }
    });
    b.addEventListener("keyup", (ev) => { if (activate(ev)) { ev.stopPropagation(); up(); } });
    b.addEventListener("blur", up);
  }
  const keys = { ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right", z: "a", x: "b", a: "x", s: "y", q: "l", w: "r", Enter: "start", Shift: "select" };
  const keyBit = (ev) => BUTTONS[keys[ev.key] || keys[ev.key.toLowerCase()]];
  addEventListener("keydown", (ev) => {
    if (state.tab !== "screen" || (ev.target instanceof Element && ev.target.matches("input, select, textarea")) || ev.repeat) return;
    // (Enter / Space on a focused control activate it: Save, Load, Capture...)
    if ((ev.key === "Enter" || ev.key === " ") && ev.target instanceof Element && ev.target.closest("button, a")) return;
    const bit = keyBit(ev);
    if (!bit) return;
    ev.preventDefault(); setButtons(heldMask | bit);
  });
  addEventListener("keyup", (ev) => {
    const bit = keyBit(ev);
    if (bit && heldMask & bit) setButtons(heldMask & ~bit);
  });
  addEventListener("blur", () => { if (heldMask) setButtons(0); });
}

// ---------------------------------------------------------------- library --
async function loadLibrary(force = false) {
  if (!state.ctl) return;
  if (!state.romsLoaded || force) {
    const roms = await guarded(() => state.ctl.roms());
    if (!roms) return;
    state.roms = roms; state.romsLoaded = true;
    forgetBoxartFailures();
    state.filter = state.status?.filter || "";
    const folders = [...new Set(roms.map((r) => dirname(r.path).slice(ROOT.length + 1)).filter(Boolean))].sort();
    $("folders").replaceChildren(...folders.map((f) => h("option", { value: f })));
  }
  renderLibrary();
}

function renderLibrary() {
  const consoles = [];
  for (const r of state.roms) {
    let c = consoles.find((x) => x.key === r.key);
    if (!c) consoles.push(c = { key: r.key, name: r.console, n: 0 });
    c.n++;
  }
  const chip = (key, name, n) => h("button", { class: "chip" + (state.filter === key ? " active" : ""), onclick: () => pickFilter(key) },
    key ? h("span", { class: "dot", style: `background:${CONSOLE_COLORS[key] || CONSOLE_COLORS.other}` }) : null, name, h("span", { class: "n" }, n));
  $("chips").replaceChildren(chip("", "All consoles", state.roms.length), ...consoles.map((c) => chip(c.key, c.name, c.n)));

  const q = state.search.trim().toLowerCase();
  const playing = state.status?.state === "game" ? state.status.rom : null;
  const groups = consoles.filter((c) => !state.filter || c.key === state.filter).map((c) => {
    const games = state.roms.filter((r) => r.key === c.key && (!q || r.name.toLowerCase().includes(q) || r.path.toLowerCase().includes(q)));
    if (!games.length) return null;
    return h("section", { class: "lib-group" },
      h("h3", {}, badge(c.key, c.name), h("span", { class: "muted small" }, games.length + (games.length === 1 ? " game" : " games"))),
      h("div", { class: "lib-grid" }, games.map((r) => gameCard(r, playing))));
  }).filter(Boolean);
  $("lib").replaceChildren(...(groups.length ? groups : [h("p", { class: "muted" }, state.roms.length ? "No game matches." : "No games: add metadata.csv to the SD card, or add a game above.")]));
  observeArt();
}

function gameCard(r, playing) {
  const isPlaying = playing === r.path;
  const img = h("img", { alt: "", "data-art": r.boxart || null });
  const cached = r.boxart && state.boxart.get(r.boxart);
  if (cached) img.src = cached;
  return h("article", { class: "game" + (isPlaying ? " playing" : ""), title: r.path },
    h("div", { class: "art" }, r.boxart ? img : "no boxart"),
    h("div", { class: "body" },
      h("div", { class: "title" }, r.name),
      h("div", { class: "actions" },
        isPlaying
          ? h("button", { class: "btn small", onclick: () => guarded(() => state.ctl.quit(), "Quit") }, "Quit")
          : h("button", { class: "btn small primary", disabled: !!playing || state.status?.usb_drive, onclick: () => launch(r) }, "Play"),
        h("button", { class: "btn small", title: "Take it out of metadata.csv (the file stays on the card)", onclick: () => removeGame(r) }, "Remove"))));
}

async function launch(r) {
  await guarded(() => state.ctl.launch(r.path), "Starting " + r.name);
  pollStatus();
}

async function removeGame(r) {
  if (!(await confirmModal("Remove " + r.name + "?", "It is taken out of metadata.csv; the ROM stays on the SD card.", "Remove"))) return;
  if (await guarded(() => state.ctl.addRom(r.path, r.name, "", true), "Removed " + r.name) !== undefined) loadLibrary(true);
}

async function pickFilter(key) {
  state.filter = key;
  renderLibrary();
  if ($("syncFilter").checked) await guarded(() => state.ctl.consoleFilter(key));
}

let artObserver = null;
function observeArt() {
  artObserver?.disconnect();
  artObserver = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (!e.isIntersecting) continue;
      const img = e.target, path = img.dataset.art;
      artObserver.unobserve(img);
      if (!path || img.src) continue;
      queueBoxart(path, (url) => { if (url) img.src = url; });
    }
  }, { rootMargin: "200px" });
  for (const img of document.querySelectorAll("#lib img[data-art]")) if (!img.src) artObserver.observe(img);
}

// Missing box art is not asked for again on every status poll, only once the
// card may have changed: the library reloaded, or the card back from the PC.
function forgetBoxartFailures() {
  for (const [path, url] of state.boxart) if (!url) state.boxart.delete(path);
}

function queueBoxart(path, done) {
  if (state.boxart.has(path)) { done(state.boxart.get(path)); return; }
  state.boxartQueue.push({ path, done });
  pumpBoxart();
}
async function pumpBoxart() {
  if (state.boxartBusy) return;
  state.boxartBusy = true;
  while (state.boxartQueue.length && state.ctl) {
    const { path, done } = state.boxartQueue.shift();
    if (state.boxart.has(path)) { done(state.boxart.get(path)); continue; }
    const session = state.session;
    try {
      const data = await state.ctl.readFile(path);
      if (session !== state.session) { done(null); continue; } // (another connection's now)
      const url = URL.createObjectURL(new Blob([data], { type: "image/jpeg" }));
      state.boxart.set(path, url);
      done(url);
    } catch (_) {
      if (session === state.session) state.boxart.set(path, null);
      done(null);
    }
  }
  state.boxartBusy = false;
}

async function addGame(ev) {
  ev.preventDefault();
  const rom = $("addRomFile").files[0], art = $("addArt").files[0];
  const folder = $("addFolder").value.trim().replace(/^\/+|\/+$/g, "").replace(/^sdcard\//, "");
  const name = $("addName").value.trim();
  if (!rom || !folder || !name) return;
  const romPath = `${ROOT}/${folder}/${rom.name}`;
  const artPath = art ? `${ROOT}/boxart/${art.name}` : "";
  const msg = $("addMsg");
  // every step on the board it started on (see uploadFiles)
  const ctl = state.ctl;
  const still = () => { if (state.ctl !== ctl) throw new Error("Disconnected: the game was not added"); };
  try {
    msg.textContent = "Copying the ROM…";
    const romData = new Uint8Array(await rom.arrayBuffer());
    still();
    await ctl.writeFile(romPath, romData, (n, t) => (msg.textContent = `ROM ${Math.round((n / t) * 100)}%`));
    if (art) {
      msg.textContent = "Copying the boxart…";
      const artData = new Uint8Array(await art.arrayBuffer());
      still();
      await ctl.writeFile(artPath, artData);
    }
    still();
    msg.textContent = "Adding to metadata.csv…";
    const total = await ctl.addRom(romPath, name, artPath ? artPath.slice(ROOT.length + 1) : "");
    msg.textContent = `Added: ${total} games now.`;
    $("addGame").reset();
    loadLibrary(true);
  } catch (e) {
    msg.textContent = "";
    toast(e.message || String(e), true);
  }
}

// ------------------------------------------------------------------ files --
async function loadFiles(path) {
  const entries = await guarded(() => state.ctl.ls(path));
  if (!entries) return;
  state.cwd = path;
  state.files = entries.sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name));
  renderFiles();
}

function renderFiles() {
  const parts = state.cwd.slice(ROOT.length).split("/").filter(Boolean);
  const crumbs = [h("button", { onclick: () => loadFiles(ROOT) }, "SD card")];
  parts.forEach((p, i) => {
    crumbs.push(h("span", {}, "/"));
    crumbs.push(h("button", { onclick: () => loadFiles(ROOT + "/" + parts.slice(0, i + 1).join("/")) }, p));
  });
  $("crumbs").replaceChildren(...crumbs);
  const rows = state.files.map((e) => {
    const path = state.cwd + "/" + e.name;
    return h("tr", {},
      h("td", {}, h("button", { class: "file-name" + (e.dir ? " dir" : ""), onclick: () => (e.dir ? loadFiles(path) : downloadFile(path, e.size)) }, (e.dir ? "📁 " : "") + e.name)),
      h("td", { class: "num" }, e.dir ? "" : fmtBytes(e.size)),
      h("td", { class: "acts" },
        e.dir ? null : h("button", { class: "btn small", onclick: () => downloadFile(path, e.size) }, "Download"),
        " ",
        h("button", { class: "btn small", onclick: () => deletePath(path, e.dir) }, "Delete")));
  });
  if (state.cwd !== ROOT) rows.unshift(h("tr", {}, h("td", { colspan: 3 }, h("button", { class: "file-name dir", onclick: () => loadFiles(dirname(state.cwd)) }, "⬑ up"))));
  if (!state.files.length) rows.push(h("tr", {}, h("td", { colspan: 3, class: "muted" }, "Empty folder")));
  $("files").tBodies[0].replaceChildren(...rows);
}

function transfer(text, n, total) {
  $("transfer").hidden = !text;
  $("transferText").textContent = text || "";
  $("transferProgress").max = total || 1; $("transferProgress").value = n || 0;
}

async function downloadFile(path, size) {
  try {
    transfer("Downloading " + basename(path), 0, size);
    const data = await state.ctl.readFile(path, (n, t) => transfer(`Downloading ${basename(path)} (${fmtBytes(n)} of ${fmtBytes(t)})`, n, t));
    download(basename(path), data);
  } catch (e) {
    toast(e.message || String(e), true);
  } finally {
    transfer("");
  }
}

async function uploadFiles(files) {
  // where they were dropped, on the device they were dropped on (the folder
  // shown can change while they are copied)
  const dir = state.cwd, ctl = state.ctl;
  for (const f of files) {
    if (state.ctl !== ctl) break; // (disconnected)
    try {
      const data = new Uint8Array(await f.arrayBuffer());
      if (state.ctl !== ctl) break;
      await ctl.writeFile(dir + "/" + f.name, data, (n, t) => transfer(`Copying ${f.name} (${fmtBytes(n)} of ${fmtBytes(t)})`, n, t));
    } catch (e) {
      toast(`${f.name}: ${e.message || e}`, true);
      break;
    }
  }
  transfer("");
  if (state.ctl === ctl) loadFiles(state.cwd);
}

async function deletePath(path, dir) {
  if (!(await confirmModal("Delete " + basename(path) + "?", dir ? "The folder and everything in it are deleted from the SD card." : "The file is deleted from the SD card.", "Delete"))) return;
  await guarded(() => state.ctl.rm(path), "Deleted " + basename(path));
  loadFiles(state.cwd);
}

// --------------------------------------------------------------- settings --
const SETTING_INFO = {
  volume: { label: "Volume", unit: "%" },
  muted: { label: "Mute", bool: true },
  brightness: { label: "Brightness", unit: "%" },
  video: { label: "Video scaling", options: ["Original", "Fit", "Fill"] },
  resume: { label: "Resume last save", bool: true },
  haptic: { label: "Haptic effect" },
};

async function loadSettings() {
  const r = await guarded(() => state.ctl.settings());
  if (!r) { $("settingsForm").replaceChildren(h("p", { class: "muted" }, "This firmware has no settings over USB (control protocol 4).")); return; }
  state.settings = r;
  // (a change still waiting when this connection ends is dropped, not sent to the next one)
  const ctl = state.ctl, session = state.session;
  const send = debounce((name, value) => {
    if (state.session === session) guarded(() => ctl.settings(`${name}=${value}`));
  }, 250);
  $("settingsForm").replaceChildren(...Object.entries(r.settings).map(([name, value]) => {
    const info = SETTING_INFO[name] || { label: name };
    const [min, max] = r.limits[name] || [0, 100];
    const id = "set-" + name;
    let input, out = h("output", { for: id });
    if (info.bool) {
      input = h("input", { type: "checkbox", id, checked: !!value, onchange: (e) => send(name, e.target.checked ? 1 : 0) });
      input = h("label", { class: "toggle" }, input, value ? "On" : "Off");
      input.querySelector("input").addEventListener("change", (e) => (input.lastChild.textContent = e.target.checked ? "On" : "Off"));
    } else if (info.options) {
      input = h("select", { id, onchange: (e) => send(name, e.target.value) }, info.options.map((o, i) => h("option", { value: i, selected: i === value }, o)));
    } else {
      out.textContent = value + (info.unit || "");
      input = h("input", { type: "range", id, min, max, value, oninput: (e) => { out.textContent = e.target.value + (info.unit || ""); send(name, e.target.value); } });
    }
    return h("div", { class: "setting" }, h("label", { for: id }, info.label), input, out);
  }));
}

function debounce(fn, ms) {
  const timers = {};
  return (key, ...args) => { clearTimeout(timers[key]); timers[key] = setTimeout(() => fn(key, ...args), ms); };
}

// ----------------------------------------------------------------- system --
async function loadSystem() {
  const info = await guarded(() => state.ctl.info());
  const rows = [];
  const add = (k, v) => { if (v != null && v !== "") rows.push(h("dt", {}, k), h("dd", {}, v)); };
  if (info) {
    add("Chip", `${info.chip}${info.chip_revision != null ? " rev " + info.chip_revision : ""}${info.cores ? ", " + info.cores + " cores" : ""}${info.cpu_mhz ? " @ " + info.cpu_mhz + " MHz" : ""}`);
    add("Project", info.project); add("Firmware", info.firmware); add("ESP-IDF", info.idf);
    add("Built", [info.build_date, info.build_time].filter(Boolean).join(" "));
    add("Running from", info.running_partition); add("Boots from", info.boot_partition); add("OTA state", info.ota_state);
    add("Flash", fmtBytes(info.flash_size)); add("PSRAM", fmtBytes(info.psram_size));
    add("Free heap", fmtBytes(info.free_heap)); add("Lowest free heap", fmtBytes(info.min_free_heap));
    add("Last reset", info.reset_reason); add("Uptime", fmtUptime(info.uptime_ms)); add("MAC", info.mac);
    add("ELF SHA-256", info.elf_sha256 ? h("code", {}, info.elf_sha256.slice(0, 16) + "…") : null);
  }
  $("infoList").replaceChildren(...rows);
  const mods = state.discovery?.modules || [];
  const registry = await loadRegistry();
  $("modules").tBodies[0].replaceChildren(...mods.map((m) => {
    const app = m.id === MODULE.CONTROL ? null : appFor(registry, m);
    return h("tr", {},
      h("td", {}, `${m.name} `, h("span", { class: "muted small" }, `0x${m.id.toString(16).padStart(2, "0")}`)),
      h("td", {}, h("code", {}, m.protocol ? `${m.protocol} v${m.version}` : "—")),
      h("td", { class: "acts" }, app ? h("button", { class: "btn small", title: app, onclick: () => handOff(app, m.id) }, "Open app")
        : m.id === MODULE.CONTROL ? h("span", { class: "muted small" }, "this page") : null));
  }));
}

// espp's hosted apps and the protocols each speaks (the Device Hub's
// registry.js); a built-in table when it cannot be loaded
const FALLBACK_APPS = { "espp.ota": "ota_console.html", "espp.coredump": "coredump_console.html", "espp.system": "system_console.html", "espp.monitor": "system_console.html" };
function loadRegistry() {
  if (!loadRegistry.p) {
    loadRegistry.p = new Promise((resolve) => {
      if (window.ESPP_APPS) { resolve(window.ESPP_APPS); return; }
      const s = h("script", { src: ESPP_APPS + "registry.js" });
      s.onload = () => resolve(window.ESPP_APPS || null);
      s.onerror = () => resolve(null);
      document.head.append(s);
      setTimeout(() => resolve(window.ESPP_APPS || null), 4000);
    });
  }
  return loadRegistry.p;
}
function appFor(registry, m) {
  const ok = (f) => typeof f === "string" && /^[\w.-]+\.html$/.test(f);
  if (registry?.apps && m.protocol) {
    const app = registry.apps.find((a) => (a.protocols || []).some((p) => p.id === m.protocol && !p.optional))
      || registry.apps.find((a) => (a.protocols || []).some((p) => p.id === m.protocol));
    if (app && ok(app.file)) return app.file;
  }
  if (FALLBACK_APPS[m.protocol]) return FALLBACK_APPS[m.protocol];
  return ok(m.app) ? m.app : null;
}

function handoffUrl(app, moduleId) {
  const q = new URLSearchParams({ module: String(moduleId), autoconnect: "1", transport: "usb", vid: "0x" + VID.toString(16).padStart(4, "0"), pid: "0x" + PID.toString(16).padStart(4, "0") });
  if (state.link?.serial) q.set("serial", state.link.serial);
  return ESPP_APPS + app + "?" + q;
}

// an espp app opens the device itself: let go of it first
async function handOff(app, moduleId) {
  const tab = window.open("about:blank", "_blank");
  if (!tab) { toast("Allow pop-ups for this page to open the app", true); return; }
  const url = handoffUrl(app, moduleId);
  state.autoReconnect = false; // (the app has it now)
  await disconnect();
  tab.location.href = url;
  toast("The device was handed to " + app + "; connect again here when you are done.");
}

async function updateFirmware() {
  if (!(await confirmModal("Update the firmware?", "The emulator restarts into its updater (a running game is left without saving). The espp OTA console then opens in a new tab: pick build/<app>.bin there.", "Restart into the updater"))) return;
  const tab = window.open("about:blank", "_blank");
  if (!tab) { toast("Allow pop-ups for this page to open the OTA console", true); return; }
  const url = handoffUrl("ota_console.html", MODULE.OTA);
  // the updater comes up with the same serial: it is the OTA console's to open
  state.autoReconnect = false;
  try { await state.ctl.enterUpdater(); } catch (e) { state.autoReconnect = true; tab.close(); toast(e.message || String(e), true); return; }
  await disconnect();
  tab.location.href = url;
}

async function reboot(bootloader) {
  const text = bootloader ? "The emulator restarts into the ROM download mode, for idf.py flash. It comes back after a flash or a reset." : "The emulator restarts (a running game is left without saving).";
  if (!(await confirmModal(bootloader ? "Restart into download mode?" : "Restart the emulator?", text, "Restart"))) return;
  await guarded(() => state.ctl.reboot(bootloader));
}

// ---------------------------------------------------------------- console --
const ANSI = /\x1b\[[0-9;]*m/g;
let logLines = [];
async function openSerial() {
  if (!navigator.serial) { toast("This browser has no Web Serial", true); return; }
  if (state.serial) { await closeSerial(); return; }
  let port;
  try {
    port = await navigator.serial.requestPort({ filters: [{ usbVendorId: VID, usbProductId: PID }] });
    await port.open({ baudRate: 115200 });
  } catch (e) {
    if (e.name !== "NotFoundError") toast(e.message || String(e), true);
    return;
  }
  state.serial = { port, reader: null };
  $("serialBtn").textContent = "Close the console port";
  const decoder = new TextDecoder();
  let partial = "";
  try {
    while (state.serial && port.readable) {
      const reader = port.readable.getReader();
      state.serial.reader = reader;
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          partial += decoder.decode(value, { stream: true });
          const lines = partial.split(/\r?\n/);
          partial = lines.pop();
          if (lines.length) appendLog(lines);
        }
      } finally {
        reader.releaseLock();
      }
    }
  } catch (e) {
    if (state.serial) toast("Console: " + (e.message || e), true);
  }
  await closeSerial();
}
async function closeSerial() {
  const s = state.serial;
  state.serial = null;
  $("serialBtn").textContent = "Open the console port";
  if (!s) return;
  try { await s.reader?.cancel(); } catch (_) {}
  try { await s.port.close(); } catch (_) {}
}
function appendLog(lines) {
  for (const raw of lines) logLines.push(raw.replace(ANSI, ""));
  if (logLines.length > 5000) logLines = logLines.slice(-5000);
  renderLog(lines.map((l) => l.replace(ANSI, "")));
}
function logLine(text) {
  const m = /^(?:\[[^\]]*\/)?([IWEDV])[\] (]/.exec(text) || /^\[[^/\]]*\/([IWEDV])\]/.exec(text);
  return h("div", { class: m ? m[1] : null }, text);
}
function renderLog(fresh) {
  const log = $("log"), f = $("logFilter").value.toLowerCase();
  if (fresh && !f) {
    log.append(...fresh.map(logLine));
    while (log.childElementCount > 5000) log.firstElementChild.remove();
  } else {
    log.replaceChildren(...logLines.filter((l) => !f || l.toLowerCase().includes(f)).slice(-2000).map(logLine));
  }
  if ($("autoscroll").checked) log.scrollTop = log.scrollHeight;
}

// ------------------------------------------------------------------- init --
function init() {
  if (!navigator.usb) { $("unsupported").hidden = false; $("connectBtn").disabled = true; $("welcomeConnectBtn").disabled = true; }
  $("connectBtn").onclick = $("welcomeConnectBtn").onclick = () => connect();
  $("disconnectBtn").onclick = () => { state.autoReconnect = false; disconnect(); };
  // (as the System tab's Open app buttons: the core dump console needs the device)
  $("coredumpLink").onclick = (e) => { e.preventDefault(); handOff("coredump_console.html", MODULE.COREDUMP); };
  for (const b of document.querySelectorAll("#tabs button")) b.onclick = () => showTab(b.dataset.tab);
  for (const b of document.querySelectorAll("[data-goto]")) b.onclick = () => showTab(b.dataset.goto);
  $("nowQuitBtn").onclick = $("quitBtn").onclick = () => guarded(() => state.ctl.quit(), "Quit");
  $("driveBtn").onclick = () => guarded(async () => {
    const host = !state.status?.usb_drive;
    await state.ctl.usbDrive(host);
    pollStatus();
  }, state.status?.usb_drive ? "The emulator has the SD card again" : "The PC has the SD card");
  $("captureBtn").onclick = () => capture();
  $("liveToggle").onchange = (e) => { state.live = e.target.checked; if (state.live) liveLoop(); };
  $("savePngBtn").onclick = () => $("screen").toBlob((b) => {
    if (b) download(`screenshot-${Date.now()}.png`, b, "image/png");
    else toast("The PNG could not be made", true);
  }, "image/png");
  $("slot").replaceChildren(...[0, 1, 2, 3, 4, 5].map((s) => h("option", { value: s, selected: s === 1 }, "Slot " + s)));
  $("saveBtn").onclick = () => guarded(() => state.ctl.save(Number($("slot").value)), "Saved to slot " + $("slot").value);
  $("loadBtn").onclick = () => guarded(() => state.ctl.load(Number($("slot").value)), "Loaded slot " + $("slot").value);
  initPad();
  $("libSearch").oninput = (e) => { state.search = e.target.value; renderLibrary(); };
  $("reloadRomsBtn").onclick = async () => { await guarded(() => state.ctl.reloadRoms()); loadLibrary(true); };
  $("addGameToggle").onclick = () => ($("addGame").hidden = !$("addGame").hidden);
  $("addGame").onsubmit = addGame;
  $("refreshFilesBtn").onclick = () => loadFiles(state.cwd);
  $("mkdirBtn").onclick = async () => {
    const name = $("newFolder").value.trim();
    if (!name || name.includes("/")) return;
    if (await guarded(() => state.ctl.mkdir(state.cwd + "/" + name)) !== undefined) { $("newFolder").value = ""; loadFiles(state.cwd); }
  };
  $("uploadInput").onchange = (e) => { uploadFiles([...e.target.files]); e.target.value = ""; };
  const drop = $("drop");
  drop.addEventListener("dragover", (e) => { e.preventDefault(); drop.classList.add("over"); });
  drop.addEventListener("dragleave", () => drop.classList.remove("over"));
  drop.addEventListener("drop", (e) => { e.preventDefault(); drop.classList.remove("over"); if (state.ctl) uploadFiles([...e.dataTransfer.files]); });
  $("infoBtn").onclick = () => loadSystem();
  $("updateBtn").onclick = () => updateFirmware();
  $("rebootBtn").onclick = () => reboot(false);
  $("bootloaderBtn").onclick = () => reboot(true);
  $("serialBtn").onclick = () => openSerial();
  $("logFilter").oninput = () => renderLog();
  $("clearLogBtn").onclick = () => { logLines = []; $("log").replaceChildren(); };
  $("saveLogBtn").onclick = () => download(`console-${Date.now()}.log`, logLines.join("\n"), "text/plain");
  addEventListener("keydown", (e) => { if (e.key === "Escape" && !$("modal").hidden) $("modalCancel").click(); });
  autoConnect();
}

init();
// (for the browser console: emuConsole.ctl.status() and so on)
window.emuConsole = state;
