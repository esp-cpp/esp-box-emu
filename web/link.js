// The emulator link in the browser: espp stream_frame frames over the
// device's vendor (WebUSB) interface, routed by module, the same protocol as
// tools/emulink (frame.py, link.py, control.py). Shared, byte for byte, by
// esp-box-emu and tab5-emu.

export const VID = 0x1209;
export const PID = 0x0d32;
/** The USB serial numbers of the emulators, and what to call them. */
export const DEVICES = {
  "esp-box-emu": { name: "ESP-Box-Emu", site: "https://esp-cpp.github.io/esp-box-emu/" },
  "tab5-emu": { name: "Tab5 Emu", site: "https://esp-cpp.github.io/tab5-emu/" },
};

export const MODULE = { OTA: 0x00, COREDUMP: 0x04, SYSTEM: 0x07, MONITOR: 0x08, CONTROL: 0x20, DISCOVERY: 0xff };

// emu.control request types (main/remote_control.hpp)
export const T = {
  STATUS: 0x01, LIST_ROMS: 0x02, LAUNCH: 0x03, QUIT: 0x04, BUTTONS: 0x05, SAVE: 0x06, LOAD: 0x07,
  SCREENSHOT: 0x08, USB_DRIVE: 0x09, RELOAD_ROMS: 0x0a, ENTER_UPDATER: 0x0b, SCREENSHOT_READ: 0x0c,
  CONSOLE_FILTER: 0x0d, SETTINGS: 0x0e,
  FILE_LIST: 0x10, FILE_READ: 0x11, FILE_WRITE: 0x12, FILE_DELETE: 0x13, MKDIR: 0x14, FILE_STAT: 0x15,
  FILE_CLOSE: 0x16,
};

/** GamepadState::buttons bits. */
export const BUTTONS = {
  a: 1 << 0, b: 1 << 1, x: 1 << 2, y: 1 << 3, select: 1 << 4, start: 1 << 5,
  up: 1 << 6, down: 1 << 7, left: 1 << 8, right: 1 << 9, l: 1 << 10, r: 1 << 11, l2: 1 << 12, r2: 1 << 13,
};

export const ROOT = "/sdcard";

// ---------------------------------------------------------------- framing --
// [magic u16 = 0x4F54][flags u8][module u8][type u8]{[correlation u16]}[len u32][payload][crc32 u32]
const MAGIC0 = 0x54, MAGIC1 = 0x4f;
const VERSION = 1, FLAG_REPLY = 0x01, FLAG_CORRELATION = 0x02;
export const MAX_PAYLOAD = 4096;

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xedb88320 : c >>> 1;
    t[i] = c >>> 0;
  }
  return t;
})();

export function crc32(bytes, end = bytes.length) {
  let c = 0xffffffff;
  for (let i = 0; i < end; i++) c = CRC_TABLE[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
  return ~c >>> 0;
}

export function buildFrame(module, type, payload = new Uint8Array(0), correlation = null) {
  if (payload.length > MAX_PAYLOAD) throw new Error(`payload too large (${payload.length} > ${MAX_PAYLOAD})`);
  const hdr = 5 + (correlation !== null ? 2 : 0) + 4;
  const out = new Uint8Array(hdr + payload.length + 4);
  const v = new DataView(out.buffer);
  out[0] = MAGIC0; out[1] = MAGIC1;
  out[2] = (VERSION << 4) | (correlation !== null ? FLAG_CORRELATION : 0);
  out[3] = module & 0xff; out[4] = type & 0xff;
  if (correlation !== null) v.setUint16(5, correlation & 0xffff, true);
  v.setUint32(hdr - 4, payload.length, true);
  out.set(payload, hdr);
  v.setUint32(hdr + payload.length, crc32(out, hdr + payload.length), true);
  return out;
}

/** Incremental, resynchronizing frame parser. */
export class Parser {
  constructor() { this.buf = new Uint8Array(0); }
  reset() { this.buf = new Uint8Array(0); }
  feed(data) {
    const joined = new Uint8Array(this.buf.length + data.length);
    joined.set(this.buf); joined.set(data, this.buf.length);
    let b = joined, frames = [];
    for (;;) {
      let i = 0;
      while (i + 1 < b.length && !(b[i] === MAGIC0 && b[i + 1] === MAGIC1)) i++;
      if (i + 1 >= b.length) { this.buf = b.slice(b.length ? b.length - 1 : 0); return frames; }
      b = b.subarray(i);
      if (b.length < 5) break;
      const flags = b[2];
      const hdr = 5 + (flags & FLAG_CORRELATION ? 2 : 0) + 4;
      if (b.length < hdr) break;
      const v = new DataView(b.buffer, b.byteOffset, b.length);
      const corr = flags & FLAG_CORRELATION ? v.getUint16(5, true) : null;
      const len = v.getUint32(hdr - 4, true);
      if (len > MAX_PAYLOAD) { b = b.subarray(2); continue; } // not a frame: resync past this magic
      const total = hdr + len + 4;
      if (b.length < total) break;
      if (crc32(b, hdr + len) !== v.getUint32(hdr + len, true)) { b = b.subarray(2); continue; }
      frames.push({ flags, module: b[3], type: b[4], correlation: corr, payload: b.slice(hdr, hdr + len),
                    isReply: !!(flags & FLAG_REPLY) });
      b = b.subarray(total);
    }
    this.buf = b.slice();
    return frames;
  }
}

export class LinkError extends Error {}

// -------------------------------------------------------------- transport --
/** The vendor interface of one USB device. */
export class UsbLink {
  constructor(device) {
    this.device = device;
    this.iface = null; this.epIn = null; this.epOut = null;
    this.parser = new Parser();
    this.waiters = new Map(); // correlation -> {module, resolve, reject, timer, types}
    this.corr = 1;
    this.reading = false;
    this.onclose = null;
    this.sendQueue = Promise.resolve(); // frames go out whole, in order
  }

  get serial() { return this.device.serialNumber || ""; }
  get product() { return this.device.productName || "USB device"; }
  get known() { return DEVICES[this.serial] || { name: this.product, site: "" }; }

  /** Ask for a device (the browser's chooser: needs a click). */
  static async request() {
    const device = await navigator.usb.requestDevice({ filters: [{ vendorId: VID, productId: PID }] });
    return new UsbLink(device);
  }

  /** Devices this page may already open (no chooser), the emulators first. */
  static async permitted() {
    if (!navigator.usb) return [];
    const devs = await navigator.usb.getDevices();
    return devs.filter((d) => d.vendorId === VID).sort((a, b) => (DEVICES[b.serialNumber] ? 1 : 0) - (DEVICES[a.serialNumber] ? 1 : 0));
  }

  async open() {
    const d = this.device;
    await d.open();
    if (d.configuration === null) await d.selectConfiguration(1);
    let found = null;
    for (const itf of d.configuration.interfaces) {
      for (const alt of itf.alternates) {
        if (alt.interfaceClass !== 0xff) continue;
        const inEp = alt.endpoints.find((e) => e.type === "bulk" && e.direction === "in");
        const outEp = alt.endpoints.find((e) => e.type === "bulk" && e.direction === "out");
        if (inEp && outEp) { found = { itf: itf.interfaceNumber, alt: alt.alternateSetting, inEp, outEp }; break; }
      }
      if (found) break;
    }
    if (!found) throw new LinkError("the device has no vendor interface");
    await d.claimInterface(found.itf);
    if (found.alt !== 0) await d.selectAlternateInterface(found.itf, found.alt);
    this.iface = found.itf; this.epIn = found.inEp.endpointNumber; this.epOut = found.outEp.endpointNumber;
    this.readLoop();
    return this;
  }

  async close() {
    this.reading = false;
    for (const w of this.waiters.values()) { clearTimeout(w.timer); w.reject(new LinkError("disconnected")); }
    this.waiters.clear();
    try { if (this.iface !== null) await this.device.releaseInterface(this.iface); } catch (_) { /* gone */ }
    try { await this.device.close(); } catch (_) { /* gone */ }
    this.iface = null;
  }

  async readLoop() {
    this.reading = true;
    while (this.reading) {
      let r;
      try {
        r = await this.device.transferIn(this.epIn, 16384);
      } catch (e) {
        if (this.reading) { this.reading = false; this.failAll(e); if (this.onclose) this.onclose(e); }
        return;
      }
      if (r.status === "stall") { try { await this.device.clearHalt("in", this.epIn); } catch (_) {} continue; }
      if (!r.data || !r.data.byteLength) continue;
      for (const f of this.parser.feed(new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength))) {
        if (!f.isReply) continue;
        const w = f.correlation !== null ? this.waiters.get(f.correlation) : null;
        if (w && w.module === f.module && (!w.types || w.types.includes(f.type))) {
          this.waiters.delete(f.correlation); clearTimeout(w.timer); w.resolve(f);
        }
      }
    }
  }

  failAll(e) {
    for (const w of this.waiters.values()) { clearTimeout(w.timer); w.reject(new LinkError(String(e.message || e))); }
    this.waiters.clear();
  }

  async send(bytes) {
    let sent = 0;
    while (sent < bytes.length) {
      const out = await this.device.transferOut(this.epOut, bytes.subarray(sent));
      if (out.status !== "ok") throw new LinkError("USB write: " + out.status);
      if (!out.bytesWritten) throw new LinkError("USB write made no progress");
      sent += out.bytesWritten;
    }
  }

  /** A request and its reply (matched by module and correlation id). The
   *  device answers in order, so several can be in flight (see readChunks). */
  request(module, type, payload = new Uint8Array(0), { timeoutMs = 15000, types = null } = {}) {
    const corr = this.corr = (this.corr % 0xffff) + 1;
    const reply = new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.waiters.delete(corr);
        reject(new LinkError(`no reply from module 0x${module.toString(16)} to request 0x${type.toString(16)}`));
      }, timeoutMs);
      this.waiters.set(corr, { module, resolve, reject, timer, types });
    });
    const frame = buildFrame(module, type, payload, corr);
    const sent = this.sendQueue.then(() => this.send(frame));
    this.sendQueue = sent.catch(() => {
      const w = this.waiters.get(corr);
      if (w) { clearTimeout(w.timer); this.waiters.delete(corr); }
    });
    // (both observed at once: a timeout or a lost link while the frame is
    // still queued rejects now, and never as an unhandled rejection)
    return Promise.all([sent, reply]).then(([, f]) => f);
  }

  /** The dispatcher's module list: {name, firmware, modules: [{id, name, protocol, version, ...}]}. */
  async discover() {
    const f = await this.request(MODULE.DISCOVERY, 0x00, undefined, { timeoutMs: 3000 });
    return parseDiscovery(f.payload);
  }
}

export function parseDiscovery(p) {
  let i = 0;
  const need = (n) => { if (i + n > p.length) throw new Error("truncated discovery reply"); };
  const u8 = () => { need(1); return p[i++]; };
  const u16 = () => { need(2); const v = p[i] | (p[i + 1] << 8); i += 2; return v; };
  const str = () => { const n = u8(); need(n); const s = new TextDecoder().decode(p.subarray(i, i + n)); i += n; return s; };
  const version = u8(); u8(); // version, reserved
  const out = { version, name: str(), firmware: str(), modules: [] };
  const n = u8();
  for (let k = 0; k < n; k++) {
    const m = { id: u8(), name: str(), app: str(), description: str(), protocol: "", version: 0 };
    if (version >= 2) { m.protocol = str(); m.version = u16(); }
    out.modules.push(m);
  }
  return out;
}

// ---------------------------------------------------------- emu.control --
const enc = new TextEncoder(), dec = new TextDecoder();
const u32le = (v) => { const b = new Uint8Array(4); new DataView(b.buffer).setUint32(0, v, true); return b; };
const u16le = (v) => { const b = new Uint8Array(2); new DataView(b.buffer).setUint16(0, v, true); return b; };
const cat = (...parts) => {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
};

export class ControlError extends Error {}

/** The emulator's control module (0x20) and the espp system service (7). */
export class Control {
  constructor(link) {
    this.link = link;
    this.ops = Promise.resolve(); // see exclusive()
    this.mainOps = Promise.resolve(); // see onMain()
  }

  /** Requests that need the device's main loop run one at a time: a screenshot
   *  holds the game (the main loop) until its last chunk is read, so a save,
   *  load, quit or launch sent meanwhile would stall the capture. */
  onMain(fn) {
    const run = this.mainOps.then(() => fn());
    this.mainOps = run.catch(() => {});
    return run;
  }

  // Operations made of several requests that share the device's state (a file
  // written in parts, metadata.csv read, changed and written back) run one at
  // a time: two at once would interleave their writes or lose an edit.
  exclusive(fn) {
    const run = this.ops.then(() => fn());
    this.ops = run.catch(() => {});
    return run;
  }

  async call(type, payload = new Uint8Array(0), { timeoutMs = 15000, raw = false } = {}) {
    if (typeof payload === "string") payload = enc.encode(payload);
    const f = await this.link.request(MODULE.CONTROL, type, payload, { timeoutMs });
    if (!f.payload.length) throw new ControlError("empty reply");
    const body = f.payload.subarray(1);
    if (f.payload[0] !== 0) {
      let msg = dec.decode(body);
      try { msg = JSON.parse(msg).error || msg; } catch (_) { /* plain text */ }
      throw new ControlError(msg);
    }
    if (raw) return body;
    return body.length ? JSON.parse(dec.decode(body)) : {};
  }

  remote(path) {
    // (the device takes any path: keep this page's on the card)
    if (path && path.split("/").includes("..")) throw new ControlError(`no '..' in a path: ${path}`);
    if (!path || path === "/" || path === ".") return ROOT;
    if (path.startsWith(ROOT)) return path;
    return ROOT + "/" + path.replace(/^\/+/, "");
  }

  status() { return this.call(T.STATUS); }

  async roms() {
    let out = [], offset = 0;
    for (;;) {
      const r = await this.call(T.LIST_ROMS, String(offset));
      out = out.concat(r.roms);
      offset = r.next;
      if (offset >= r.total || !r.roms.length) return out;
    }
  }

  launch(match) { return this.onMain(() => this.call(T.LAUNCH, match, { timeoutMs: 30000 })); }
  quit() { return this.onMain(() => this.call(T.QUIT, undefined, { timeoutMs: 30000 })); }
  buttons(mask, holdMs = 0) { return this.call(T.BUTTONS, cat(u16le(mask), u16le(holdMs)), { timeoutMs: holdMs + 5000 }); }
  save(slot) { return this.onMain(() => this.call(T.SAVE, new Uint8Array([slot]), { timeoutMs: 60000 })); }
  load(slot) { return this.onMain(() => this.call(T.LOAD, new Uint8Array([slot]), { timeoutMs: 60000 })); }
  usbDrive(host) { return this.onMain(() => this.call(T.USB_DRIVE, new Uint8Array([host ? 1 : 0]), { timeoutMs: 20000 })); }
  reloadRoms() { return this.onMain(() => this.call(T.RELOAD_ROMS)); }
  enterUpdater() { return this.call(T.ENTER_UPDATER); }
  consoleFilter(key) { return this.call(T.CONSOLE_FILTER, key || "all"); }
  settings(change) { return this.call(T.SETTINGS, change || ""); }

  /** `size` bytes read in 4000-byte chunks, `depth` requests in flight
   *  (the device answers in order; a short chunk is an error: the size is known). */
  async readChunks(size, request, progress, depth = 4) {
    const out = new Uint8Array(size);
    const CH = 4000;
    let next = 0, done = 0;
    const one = async () => {
      while (next < size) {
        const off = next, n = Math.min(CH, size - off);
        next += n;
        const chunk = await request(off, n);
        if (chunk.length !== n) throw new ControlError(`short read: ${off + chunk.length} of ${size} B`);
        out.set(chunk, off); done += n;
        if (progress) progress(done, size);
      }
    };
    await Promise.all(Array.from({ length: Math.min(depth, Math.ceil(size / CH)) }, one));
    return out;
  }

  /** The running game's frame, or the launcher's screen: {width, height, format, source, data}. */
  frame(progress) { return this.onMain(() => this.frameNow(progress)); }
  async frameNow(progress) {
    const info = await this.call(T.SCREENSHOT, undefined, { timeoutMs: 15000 });
    // (the last chunk read lets the game go on: it must be the last one asked for)
    const last = Math.max(0, info.size - 4000 > 0 ? info.size - ((info.size % 4000) || 4000) : 0);
    const head = await this.readChunks(last, (off, n) => this.call(T.SCREENSHOT_READ, cat(u32le(off), u16le(n)), { raw: true }), progress);
    const tail = await this.call(T.SCREENSHOT_READ, cat(u32le(last), u16le(info.size - last)), { raw: true });
    if (tail.length !== info.size - last) throw new ControlError(`short read at the end of the frame`);
    const data = new Uint8Array(info.size);
    data.set(head); data.set(tail, last);
    if (progress) progress(info.size, info.size);
    return { ...info, data };
  }

  // files on the SD card
  async ls(path) {
    const p = this.remote(path);
    let r = await this.call(T.FILE_LIST, p);
    let out = r.entries;
    while (r.truncated && r.next != null && r.entries.length) {
      r = await this.call(T.FILE_LIST, p + "\0" + r.next);
      out = out.concat(r.entries);
    }
    return out;
  }
  async stat(path) {
    try { return await this.call(T.FILE_STAT, this.remote(path)); } catch (e) {
      if (String(e.message).includes("does not exist")) return null;
      throw e;
    }
  }
  mkdir(path) { return this.call(T.MKDIR, this.remote(path)); }
  rm(path) { return this.exclusive(() => this.call(T.FILE_DELETE, this.remote(path), { timeoutMs: 120000 })); }

  // (queued too: a read alongside a write of the same file could see part of it)
  readFile(path, progress) { return this.exclusive(() => this.readFileNow(path, progress)); }
  async readFileNow(path, progress) {
    const p = enc.encode(this.remote(path));
    const st = await this.call(T.FILE_STAT, p);
    return this.readChunks(st.size, (off, n) => this.call(T.FILE_READ, cat(u32le(off), u16le(n), p), { raw: true }), progress);
  }

  writeFile(path, data, progress) { return this.exclusive(() => this.writeFileNow(path, data, progress)); }
  async writeFileNow(path, data, progress) {
    const p = enc.encode(this.remote(path));
    // path + data at most 3584 bytes a frame, as emulink: under the 4096-byte
    // payload (MAX_PAYLOAD) with room to spare
    const chunk = 3584 - p.length;
    if (chunk < 256) throw new ControlError("path too long");
    let off = 0;
    do {
      const part = data.subarray(off, off + chunk);
      await this.call(T.FILE_WRITE, cat(u32le(off), u16le(p.length), p, part), { timeoutMs: 30000 });
      off += part.length;
      if (progress) progress(off, data.length);
    } while (off < data.length);
    await this.call(T.FILE_CLOSE);
  }

  /** Add (or remove) a metadata.csv line; returns the number of games after the reload. */
  addRom(romPath, name, boxart = "", remove = false) {
    return this.exclusive(() => this.addRomNow(romPath, name, boxart, remove));
  }
  async addRomNow(romPath, name, boxart, remove) {
    const rel = romPath.startsWith(ROOT + "/") ? romPath.slice(ROOT.length + 1) : romPath;
    for (const [field, value] of [["ROM path", rel], ["boxart path", boxart], ["name", name]]) {
      if (/[\r\n]/.test(value) || (field !== "name" && value.includes(",")))
        throw new ControlError(`the ${field} cannot hold ${value.includes(",") ? "a comma" : "a line break"}`);
    }
    const meta = "metadata.csv";
    // only a missing file starts an empty list: a failed read must not rewrite it
    const text = (await this.stat(meta)) ? dec.decode(await this.readFileNow(meta)) : "";
    const lines = text.split(/\r?\n/).filter((l) => l.trim() && l.split(",")[0].trim() !== rel);
    if (!remove) lines.push(`${rel}, ${boxart}, ${name}`);
    await this.writeFileNow(meta, enc.encode(lines.join("\n") + "\n"));
    return (await this.reloadRoms()).total;
  }

  // the espp system service
  async info() {
    const f = await this.link.request(MODULE.SYSTEM, 0x01, undefined, { timeoutMs: 5000, types: [0x81, 0x84] });
    if (f.type !== 0x81) throw new ControlError("GET_INFO failed");
    const p = f.payload, out = {};
    const strs = { 1: "chip", 5: "idf", 6: "project", 7: "firmware", 8: "build_date", 9: "build_time",
                   11: "running_partition", 12: "boot_partition" };
    const le = (v) => v.reduceRight((n, b) => n * 256 + b, 0);
    for (let i = 0; i + 2 <= p.length;) {
      const tag = p[i], n = p[i + 1], v = p.subarray(i + 2, i + 2 + n);
      i += 2 + n;
      if (strs[tag]) out[strs[tag]] = dec.decode(v);
      else if (tag === 2) out.chip_revision = le(v);
      else if (tag === 3) out.cores = v[0];
      else if (tag === 10) out.elf_sha256 = [...v].map((b) => b.toString(16).padStart(2, "0")).join("");
      else if (tag === 13) out.ota_state = { 0: "new", 1: "pending verify", 2: "valid", 3: "invalid", 4: "aborted", 0xfe: "undefined", 0xff: "n/a" }[v[0]] ?? v[0];
      else if (tag === 14) out.reset_reason = RESET_REASONS[v[0]] ?? v[0];
      else if (tag === 15) out.uptime_ms = le(v);
      else if (tag === 16) out.mac = [...v].map((b) => b.toString(16).padStart(2, "0")).join(":");
      else if (tag >= 17 && tag <= 21) out[{ 17: "flash_size", 18: "psram_size", 19: "cpu_mhz", 20: "free_heap", 21: "min_free_heap" }[tag]] = le(v);
    }
    return out;
  }

  reboot(bootloader = false, delayMs = 250) {
    return this.link.request(MODULE.SYSTEM, bootloader ? 0x03 : 0x02, u16le(delayMs), { timeoutMs: 5000, types: [0x83, 0x84] });
  }
}

export const RESET_REASONS = {
  0: "unknown", 1: "power-on", 2: "external", 3: "software", 4: "panic", 5: "interrupt WDT", 6: "task WDT",
  7: "other WDT", 8: "deep sleep", 9: "brownout", 10: "SDIO", 11: "USB", 12: "JTAG", 13: "eFuse",
  14: "power glitch", 15: "CPU lockup",
};

/** RGB565 pixels (rows packed) into an ImageData. */
export function rgb565ToImageData(data, width, height, bigEndian = false) {
  const img = new ImageData(width, height);
  const px = img.data;
  for (let i = 0, j = 0; i + 1 < data.length && j < px.length; i += 2, j += 4) {
    const v = bigEndian ? (data[i] << 8) | data[i + 1] : data[i] | (data[i + 1] << 8);
    const r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, b = v & 0x1f;
    px[j] = (r << 3) | (r >> 2); px[j + 1] = (g << 2) | (g >> 4); px[j + 2] = (b << 3) | (b >> 2); px[j + 3] = 255;
  }
  return img;
}
