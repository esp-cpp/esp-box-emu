"""The emulator's test control (dispatcher module 0x20, "emu.control") and the
espp system service (module 7), on top of a Link."""

from __future__ import annotations

import json
import os
import struct
import sys
import time
from typing import Iterable, List, Optional

from .link import Link, LinkError

MODULE = 0x20
SYSTEM_MODULE = 7

STATUS, LIST_ROMS, LAUNCH, QUIT, BUTTONS, SAVE, LOAD, SCREENSHOT, USB_DRIVE, RELOAD_ROMS, ENTER_UPDATER = range(1, 12)
SCREENSHOT_READ = 0x0C
FILE_LIST, FILE_READ, FILE_WRITE, FILE_DELETE, MKDIR, FILE_STAT, FILE_CLOSE = range(0x10, 0x17)

#: one FILE_WRITE: the frame payload is at most 4096 bytes, of which 6 + the path are the header
CHUNK = 3584


class ControlError(RuntimeError):
    pass


class Frame:
    """A game's frame: RGB565 pixels, row by row. ``format`` is "rgb565le" /
    "rgb565be" (None: not reported by the device)."""

    def __init__(self, width: int, height: int, format: Optional[str], data: bytes) -> None:
        self.width, self.height, self.format, self.data = width, height, format, data

    def rgb(self, big_endian: bool) -> bytes:
        """The pixels as 8-bit RGB."""
        from array import array
        px = array("H")
        px.frombytes(self.data)
        if big_endian != (sys.byteorder == "big"):
            px.byteswap()
        return b"".join(map(_rgb565_table().__getitem__, px))


_RGB565 = None


def _rgb565_table():
    global _RGB565
    if _RGB565 is None:
        def entry(v):
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            return bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
        _RGB565 = [entry(v) for v in range(65536)]
    return _RGB565


class Buttons:
    """GamepadState::buttons bits."""

    A, B, X, Y, SELECT, START, UP, DOWN, LEFT, RIGHT, L, R, L2, R2 = (1 << i for i in range(14))
    NAMES = {"a": A, "b": B, "x": X, "y": Y, "select": SELECT, "start": START, "up": UP, "down": DOWN,
             "left": LEFT, "right": RIGHT, "l": L, "r": R, "l2": L2, "r2": R2}

    @classmethod
    def parse(cls, names: Iterable[str]) -> int:
        mask = 0
        for n in names:
            for part in n.replace("+", ",").split(","):
                part = part.strip().lower()
                if not part:
                    continue
                if part not in cls.NAMES:
                    raise ValueError(f"unknown button {part!r} (one of {', '.join(cls.NAMES)})")
                mask |= cls.NAMES[part]
        return mask


class Control:
    def __init__(self, link: Link) -> None:
        self.link = link

    # -- plumbing --------------------------------------------------------------
    def _call(self, type_: int, payload: bytes = b"", timeout_s: float = 15.0, raw: bool = False):
        fr = self.link.request(MODULE, type_, payload, timeout_s=timeout_s)
        if not fr.payload:
            raise ControlError("empty reply")
        status, body = fr.payload[0], fr.payload[1:]
        if status != 0:
            try:
                msg = json.loads(body.decode()).get("error", body.decode())
            except Exception:
                msg = body.decode(errors="replace")
            raise ControlError(msg)
        if raw:
            return body
        return json.loads(body.decode()) if body else {}

    @property
    def root(self) -> str:
        return "/sdcard"

    def remote(self, path: str) -> str:
        """A path on the SD card: absolute, or relative to the card's root."""
        if path in ("", "/", "."):
            return self.root
        if path.startswith(self.root):
            return path
        return f"{self.root}/{path.lstrip('/')}"

    # -- the emulator ----------------------------------------------------------
    def status(self) -> dict:
        return self._call(STATUS)

    def roms(self) -> List[dict]:
        out: List[dict] = []
        offset = 0
        while True:
            r = self._call(LIST_ROMS, str(offset).encode())
            out += r["roms"]
            offset = r["next"]
            if offset >= r["total"] or not r["roms"]:
                return out

    def launch(self, match: str, wait: bool = True, timeout_s: float = 60.0) -> str:
        rom = self._call(LAUNCH, match.encode())["rom"]
        if wait:
            self.wait_state("game", timeout_s)
        return rom

    def quit(self, wait: bool = True, timeout_s: float = 30.0) -> None:
        self._call(QUIT)
        if wait:
            self.wait_state("menu", timeout_s)

    def wait_state(self, state: str, timeout_s: float = 60.0) -> dict:
        deadline = time.monotonic() + timeout_s
        while True:
            try:
                st = self.status()
                if st["state"] == state:
                    return st
            except LinkError:
                pass  # busy (e.g. a game loading)
            if time.monotonic() > deadline:
                raise ControlError(f"not in state {state!r} after {timeout_s:.0f} s")
            time.sleep(0.25)

    def buttons(self, mask: int, hold_ms: int = 0) -> None:
        """Hold `mask` for hold_ms (then release), or until the next call (0)."""
        self._call(BUTTONS, struct.pack("<HH", mask, hold_ms), timeout_s=hold_ms / 1000 + 5)

    def press(self, names: Iterable[str], hold_ms: int = 100, gap_ms: int = 100) -> None:
        self.buttons(Buttons.parse(names), hold_ms)
        if gap_ms:
            time.sleep(gap_ms / 1000)

    def save(self, slot: int) -> None:
        self._call(SAVE, bytes([slot]), timeout_s=60)

    def load(self, slot: int) -> None:
        self._call(LOAD, bytes([slot]), timeout_s=60)

    def screenshot(self, remote_path: str = "screenshots/screenshot.bin") -> str:
        """Save the running game's frame on the SD card ([u16be w][u16be h][RGB565]); returns its path."""
        return self._call(SCREENSHOT, self.remote(remote_path).encode())["path"]

    def frame(self, progress=None) -> "Frame":
        """The running game's frame, copied straight from its frame buffer
        (firmware before protocol 2 saves it on the card and it is read from
        there)."""
        info = self._call(SCREENSHOT, timeout_s=15)
        if "path" in info:  # older firmware
            data = self.read_file(info["path"])
            if len(data) < 4:
                raise ControlError(f"the screenshot file {info['path']} is {len(data)} bytes")
            w, h = struct.unpack(">HH", data[:4])
            px = data[4:]
            if w:
                h = min(h, len(px) // (w * 2))
            return Frame(w, h, None, px[:w * h * 2])
        size = info["size"]
        out = bytearray()
        while len(out) < size:
            n = min(4000, size - len(out))
            chunk = self._call(SCREENSHOT_READ, struct.pack("<IH", len(out), n), raw=True)
            if not chunk:
                # (the size came from FILE_STAT, so this is a failed read or a
                # file that shrank, not the end of it)
                raise ControlError(f"short read: {len(out)} of {size} B")
            out += chunk
            if progress:
                progress(len(out), size)
        if len(out) != size:
            raise ControlError(f"the frame came back short: {len(out)} of {size} bytes")
        return Frame(info["width"], info["height"], info.get("format"), bytes(out))

    def usb_drive(self, host: bool) -> None:
        self._call(USB_DRIVE, bytes([1 if host else 0]), timeout_s=20)

    def reload_roms(self) -> int:
        return self._call(RELOAD_ROMS)["total"]

    def enter_updater(self) -> None:
        self._call(ENTER_UPDATER)

    # -- files on the SD card --------------------------------------------------
    def ls(self, path: str = "") -> List[dict]:
        path = self.remote(path).encode()
        r = self._call(FILE_LIST, path)
        out = r["entries"]
        # a full page says where the next starts; older firmware only says "truncated"
        while r.get("truncated") and r.get("next") is not None and r["entries"]:
            r = self._call(FILE_LIST, path + b"\0" + str(r["next"]).encode())
            out += r["entries"]
        return out

    def stat(self, path: str) -> Optional[dict]:
        try:
            return self._call(FILE_STAT, self.remote(path).encode())
        except ControlError:
            return None

    def mkdir(self, path: str) -> None:
        self._call(MKDIR, self.remote(path).encode())

    def rm(self, path: str) -> None:
        self._call(FILE_DELETE, self.remote(path).encode(), timeout_s=60)

    def read_file(self, path: str, progress=None) -> bytes:
        path = self.remote(path)
        st = self._call(FILE_STAT, path.encode())
        size = st["size"]
        out = bytearray()
        while len(out) < size:
            n = min(4000, size - len(out))
            chunk = self._call(FILE_READ, struct.pack("<IH", len(out), n) + path.encode(), raw=True)
            if not chunk:
                # (the size came from FILE_STAT, so this is a failed read or a
                # file that shrank, not the end of it)
                raise ControlError(f"short read: {len(out)} of {size} B")
            out += chunk
            if progress:
                progress(len(out), size)
        return bytes(out)

    def write_file(self, path: str, data: bytes, progress=None) -> None:
        path = self.remote(path)
        p = path.encode()
        chunk = CHUNK - len(p)
        if chunk < 256:
            raise ControlError("path too long")
        off = 0
        while True:
            part = data[off: off + chunk]
            self._call(FILE_WRITE, struct.pack("<IH", off, len(p)) + p + part, timeout_s=30)
            off += len(part)
            if progress:
                progress(off, len(data))
            if off >= len(data):
                break
        self._call(FILE_CLOSE)

    def push(self, local: str, remote: str, progress=None) -> List[str]:
        """Copy a file or a directory tree to the SD card; returns the remote files."""
        written = []
        if os.path.isdir(local):
            for root, _dirs, files in os.walk(local):
                for f in files:
                    src = os.path.join(root, f)
                    rel = os.path.relpath(src, local)
                    dst = f"{remote.rstrip('/')}/{rel}"
                    self.write_file(dst, open(src, "rb").read(), progress)
                    written.append(self.remote(dst))
        else:
            if remote.endswith("/"):
                remote += os.path.basename(local)
            self.write_file(remote, open(local, "rb").read(), progress)
            written.append(self.remote(remote))
        return written

    def add_rom(self, rom_path: str, name: str, boxart: str = "", remove: bool = False) -> int:
        """Add (or with remove=True take out) a metadata.csv line; returns the
        number of games after reloading the list."""
        rom_rel = rom_path[len(self.root) + 1:] if rom_path.startswith(self.root + "/") else rom_path
        # metadata.csv has no quoting: the paths end at a comma (the name, the
        # last field, may have them) and a line break starts another game
        for field, value in (("ROM path", rom_rel), ("boxart path", boxart), ("name", name)):
            if "\n" in value or "\r" in value or (field != "name" and "," in value):
                raise ControlError(f"the {field} cannot hold {'a comma' if ',' in value else 'a line break'}: {value!r}")
        meta = self.remote("metadata.csv")
        try:
            text = self.read_file(meta).decode()
        except ControlError:
            text = ""
        lines = [l for l in text.splitlines() if l.strip() and l.split(",")[0].strip() != rom_rel]
        if not remove:
            lines.append(f"{rom_rel}, {boxart}, {name}")
        self.write_file(meta, ("\n".join(lines) + "\n").encode())
        return self.reload_roms()

    # -- the espp system service ------------------------------------------------
    RESET_REASONS = {0: "unknown", 1: "power-on", 2: "external", 3: "software", 4: "panic", 5: "interrupt WDT",
                     6: "task WDT", 7: "other WDT", 8: "deep sleep", 9: "brownout", 10: "SDIO", 11: "USB",
                     12: "JTAG", 13: "eFuse", 14: "power glitch", 15: "CPU lockup"}

    def info(self) -> dict:
        """The espp system service's GET_INFO: chip, firmware, partitions, reset reason, uptime..."""
        fr = self.link.request(SYSTEM_MODULE, 0x01, b"", timeout_s=5, reply_types=(0x81, 0x84))
        if fr.type != 0x81:
            raise ControlError("GET_INFO failed")
        p, i, out = fr.payload, 0, {}
        strs = {1: "chip", 6: "project", 7: "firmware", 5: "idf", 8: "build_date", 9: "build_time",
                11: "running_partition", 12: "boot_partition"}
        while i + 2 <= len(p):
            tag, n = p[i], p[i + 1]
            v = p[i + 2:i + 2 + n]
            i += 2 + n
            if tag in strs:
                out[strs[tag]] = v.decode(errors="replace")
            elif tag == 2:
                out["chip_revision"] = int.from_bytes(v, "little")
            elif tag == 3:
                out["cores"] = v[0]
            elif tag == 10:
                out["elf_sha256"] = v.hex()
            elif tag == 13:
                out["ota_state"] = {0: "new", 1: "pending verify", 2: "valid", 3: "invalid", 4: "aborted",
                                    0xFE: "undefined", 0xFF: "n/a"}.get(v[0], v[0])
            elif tag == 14:
                out["reset_reason"] = self.RESET_REASONS.get(v[0], v[0])
            elif tag == 15:
                out["uptime_ms"] = int.from_bytes(v, "little")
            elif tag == 16:
                out["mac"] = ":".join(f"{b:02x}" for b in v)
            elif tag in (17, 18, 19, 20, 21):
                out[{17: "flash_size", 18: "psram_size", 19: "cpu_mhz", 20: "free_heap", 21: "min_free_heap"}[tag]] = \
                    int.from_bytes(v, "little")
        return out

    def reboot(self, bootloader: bool = False, delay_ms: int = 250) -> None:
        self.link.request(SYSTEM_MODULE, 0x03 if bootloader else 0x02, struct.pack("<H", delay_ms),
                          timeout_s=5, reply_types=(0x83, 0x84))
