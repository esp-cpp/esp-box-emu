"""emulink command line: python -m emulink <command> ..."""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
import time
import zlib
from typing import Optional

from . import espp_tools
from .control import Buttons, Control, ControlError
from .link import Link, LinkError, find_console_port, find_devices


def _progress(label: str):
    start = time.monotonic()

    def cb(done: int, total: int) -> None:
        if not sys.stderr.isatty():
            return
        pct = 100 * done // total if total else 100
        rate = done / max(time.monotonic() - start, 1e-3) / 1024
        sys.stderr.write(f"\r{label}: {pct:3d}% {done}/{total} bytes ({rate:.0f} KiB/s)")
        if done >= total:
            sys.stderr.write("\n")
        sys.stderr.flush()

    return cb


def _png(width: int, height: int, rgb: bytes) -> bytes:
    raw = b"".join(b"\x00" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def frame_to_png(frame, big_endian: bool) -> bytes:
    return _png(frame.width, frame.height, frame.rgb(big_endian))


def _open(args, timeout_s: float = 0.0) -> Link:
    return Link(args.serial).open(timeout_s)


def cmd_devices(args) -> int:
    for d in find_devices():
        print(f"{d['serial']:12s} {d['product']:20s} bus {d['bus']} address {d['address']}  "
              f"console {find_console_port(d['serial']) or '-'}")
    return 0


def cmd_status(args) -> int:
    with _open(args) as link:
        print(json.dumps(Control(link).status(), indent=2))
    return 0


def cmd_info(args) -> int:
    with _open(args) as link:
        print(json.dumps(Control(link).info(), indent=2))
    return 0


def cmd_roms(args) -> int:
    with _open(args) as link:
        roms = Control(link).roms()
    if args.json:
        print(json.dumps(roms, indent=2))
        return 0
    console = None
    for r in roms:
        if r["console"] != console:
            console = r["console"]
            print(f"== {console}")
        print(f"   {r['name']}  ({r['path']})")
    return 0


def cmd_launch(args) -> int:
    with _open(args) as link:
        rom = Control(link).launch(args.match, wait=not args.no_wait, timeout_s=args.timeout)
    print(rom)
    return 0


def cmd_quit(args) -> int:
    with _open(args) as link:
        Control(link).quit(wait=not args.no_wait)
    return 0


def cmd_press(args) -> int:
    with _open(args) as link:
        c = Control(link)
        for _ in range(args.repeat):
            c.press(args.buttons, hold_ms=args.hold, gap_ms=args.gap)
    return 0


def cmd_hold(args) -> int:
    with _open(args) as link:
        Control(link).buttons(Buttons.parse(args.buttons), 0)
    return 0


def cmd_save(args) -> int:
    with _open(args) as link:
        Control(link).save(args.slot)
    return 0


def cmd_load(args) -> int:
    with _open(args) as link:
        Control(link).load(args.slot)
    return 0


def cmd_screenshot(args) -> int:
    with _open(args) as link:
        frame = Control(link).frame(progress=_progress("screenshot"))
        if args.byteswap is not None:
            big_endian = args.byteswap
        else:  # every core so far renders little-endian RGB565
            big_endian = frame.format == "rgb565be"
    out = args.output or time.strftime("screenshot-%Y%m%d-%H%M%S.png")
    with open(out, "wb") as f:
        if out.endswith(".png"):
            f.write(frame_to_png(frame, big_endian))
        else:  # the device's own screenshot file format
            f.write(struct.pack(">HH", frame.width, frame.height) + frame.data)
    print(out)
    return 0


def cmd_ls(args) -> int:
    with _open(args) as link:
        for e in Control(link).ls(args.path):
            print(f"{'d' if e['dir'] else '-'} {e['size']:>10} {e['name']}")
    return 0


def cmd_push(args) -> int:
    with _open(args) as link:
        for f in Control(link).push(args.local, args.remote, _progress(os.path.basename(args.local))):
            print(f)
    return 0


def cmd_pull(args) -> int:
    with _open(args) as link:
        data = Control(link).read_file(args.remote, _progress(os.path.basename(args.remote)))
    out = args.local or os.path.basename(args.remote)
    with open(out, "wb") as f:
        f.write(data)
    print(out)
    return 0


def cmd_rm(args) -> int:
    with _open(args) as link:
        Control(link).rm(args.path)
    return 0


def cmd_mkdir(args) -> int:
    with _open(args) as link:
        Control(link).mkdir(args.path)
    return 0


def cmd_add_rom(args) -> int:
    """Copy a game (and its boxart) to the card, list it in metadata.csv, reload."""
    with _open(args) as link:
        c = Control(link)
        rom_remote = f"{args.dir.strip('/')}/{os.path.basename(args.rom)}"
        c.push(args.rom, rom_remote, _progress(os.path.basename(args.rom)))
        boxart = ""
        if args.boxart:
            boxart = f"boxart/{os.path.basename(args.boxart)}"
            c.push(args.boxart, boxart, _progress(os.path.basename(args.boxart)))
        name = args.name or os.path.splitext(os.path.basename(args.rom))[0]
        total = c.add_rom(rom_remote, name, boxart)
    print(f"{rom_remote}: listed as {name!r} ({total} games)")
    return 0


def cmd_remove_rom(args) -> int:
    with _open(args) as link:
        c = Control(link)
        total = c.add_rom(args.rom, "", remove=True)
        if args.delete:
            c.rm(args.rom)
    print(f"{total} games")
    return 0


def cmd_reload(args) -> int:
    with _open(args) as link:
        print(Control(link).reload_roms())
    return 0


def _msc_disk(product: str) -> Optional[str]:
    """macOS: the BSD disk (e.g. disk20) of the emulator's SD card drive."""
    import re
    import subprocess

    try:
        out = subprocess.run(["ioreg", "-p", "IOService", "-l", "-w0", "-r", "-n", product],
                             capture_output=True, text=True, timeout=10).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None
    m = re.search(r'"BSD Name" = "(disk\d+)"', out)
    return m.group(1) if m else None


def _msc_mount(product: str) -> Optional[str]:
    """macOS: where the emulator's SD card is mounted, once it is."""
    import plistlib
    import subprocess

    disk = _msc_disk(product)
    if not disk:
        return None
    try:
        out = subprocess.run(["diskutil", "info", "-plist", f"{disk}s1"], capture_output=True, timeout=10).stdout
        mount = plistlib.loads(out).get("MountPoint") if out else None
    except (OSError, subprocess.TimeoutExpired, plistlib.InvalidFileException):
        return None
    return mount or None


def cmd_drive(args) -> int:
    """Hand the SD card to the PC (it mounts it), or take it back: on macOS the
    volume is ejected first, which the device needs before it can have it."""
    with _open(args) as link:
        c = Control(link)
        if args.state == "on":
            c.usb_drive(True)
            if sys.platform == "darwin":
                deadline = time.monotonic() + 60
                mount = None
                while time.monotonic() < deadline and not mount:
                    time.sleep(0.5)
                    mount = _msc_mount(link.product)
                print(f"the PC has the SD card ({mount or 'not mounted yet'})")
            return 0
        if sys.platform == "darwin":
            disk = _msc_disk(link.product)
            if disk:
                import subprocess

                # Spotlight (mdsync) indexing a freshly mounted volume refuses
                # the unmount for a while: retry
                deadline = time.monotonic() + 60
                while True:
                    r = subprocess.run(["diskutil", "eject", disk], capture_output=True, text=True, timeout=60)
                    if r.returncode == 0 or time.monotonic() >= deadline:
                        break
                    time.sleep(2)
                if r.returncode != 0:
                    print(f"emulink: could not eject {disk}: {r.stderr.strip() or r.stdout.strip()}",
                          file=sys.stderr)
                    return 1
        try:
            c.usb_drive(False)
        except LinkError:
            pass  # the device may drop off the bus for a moment to take the card back
    # (re)connect and check
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        try:
            with Link(args.serial).open(5) as link:
                if not Control(link).status()["usb_drive"]:
                    print("the device has the SD card again")
                    return 0
        except (LinkError, ControlError):
            pass
        time.sleep(0.5)
    print("emulink: the device did not take the SD card back", file=sys.stderr)
    return 1


def cmd_reboot(args) -> int:
    with _open(args) as link:
        try:
            Control(link).reboot(bootloader=args.bootloader)
        except LinkError:
            pass  # the device may drop off the bus before its reply gets out
    if args.bootloader:
        # the ROM's download mode shows up as the chip's USB-Serial-JTAG port
        from serial.tools import list_ports

        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            for p in list_ports.comports():
                if p.vid == 0x303A and p.pid == 0x1001:
                    print(p.device)
                    return 0
            time.sleep(0.5)
        print("emulink: no download-mode port showed up", file=sys.stderr)
        return 1
    return 0


def cmd_log(args) -> int:
    """Follow the console; across a reboot (the port goes away and comes back)
    it reconnects, so a log can span a reset."""
    import serial

    out = open(args.output, "ab") if args.output else None
    write = out.write if out else sys.stdout.buffer.write
    end = time.monotonic() + args.duration if args.duration else None
    first_wait = time.monotonic() + args.wait
    while end is None or time.monotonic() < end:
        port = find_console_port(args.serial)
        if not port:
            if time.monotonic() > first_wait and out is None and args.duration == 0 and not args.follow:
                print("no emulator console port found", file=sys.stderr)
                return 1
            time.sleep(0.2)
            continue
        try:
            with serial.Serial(port, 115200, timeout=0.2) as s:
                while end is None or time.monotonic() < end:
                    data = s.read(4096)
                    if data:
                        write(data)
                        (out or sys.stdout.buffer).flush()
                        if args.until and args.until.encode() in data:
                            return 0
        except (serial.SerialException, OSError):
            time.sleep(0.2)  # the device went away (reset): wait for it to come back
    return 0


def cmd_coredump(args) -> int:
    cd = espp_tools.load("espp_coredump", "coredump")
    from espp_coredump.client import CoreDumpClient  # type: ignore

    with _open(args) as link:
        client = CoreDumpClient(link)
        size = client.size()
        if not size:
            print("no core dump on the device")
            return 0
        print(client.summary())
        if args.output:
            image = client.read_image()
            with open(args.output, "wb") as f:
                f.write(image)
            print(f"{args.output}: {len(image)} bytes (decode: esp-coredump info_corefile -t raw -c {args.output} "
                  f"build/<app>.elf)")
        if args.erase:
            client.erase()
            print("erased")
    return 0


def cmd_ota(args) -> int:
    """Flash a new app over USB. On the Tab5 (no second emulator slot) the
    emulator reboots into the updater app first, which takes the image."""
    espp_tools.load("espp_ota", "ota")
    from espp_ota.client import OtaClient  # type: ignore

    image = open(args.binary, "rb").read()
    link = _open(args)
    try:
        if "updater" not in link.product.lower():
            print("rebooting into the updater ...")
            Control(link).enter_updater()
            link.close()
            time.sleep(2)
            link = Link(args.serial)
            deadline = time.monotonic() + 30
            while True:
                try:
                    link.open(5)
                    if "updater" in link.product.lower():
                        break
                    link.close()
                except LinkError:
                    pass
                if time.monotonic() > deadline:
                    raise LinkError("the updater did not show up")
                time.sleep(1)
        print(f"flashing {args.binary} ({len(image)} bytes) via {link.product} ...")
        OtaClient(link, progress=_progress("ota")).flash(image, image_size=len(image))
        print("done: the device restarts into the new firmware")
    finally:
        link.close()
    if args.wait:
        link = Link(args.serial)
        last_error = None
        deadline = time.monotonic() + args.wait
        while time.monotonic() < deadline:
            try:
                link.open(5)
                if "updater" not in link.product.lower():
                    st = Control(link).status()
                    print(f"up: {st['firmware']} ({st['built']}, elf {st.get('elf_sha256', '?')}) on {st['partition']}")
                    link.close()
                    return 0
                link.close()
            except (LinkError, ControlError) as e:
                last_error = e
                link.close()
            time.sleep(1)
        print(f"the new firmware did not come up ({last_error})", file=sys.stderr)
        return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="emulink", description=__doc__)
    p.add_argument("--serial", help="USB serial number (tab5-emu / esp-box-emu); default: the first one found")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("devices", help="list the emulators on USB").set_defaults(fn=cmd_devices)
    sub.add_parser("status", help="state, running game, memory, firmware").set_defaults(fn=cmd_status)
    sub.add_parser("info", help="chip, firmware, partitions, last reset reason (system service)").set_defaults(fn=cmd_info)
    s = sub.add_parser("roms", help="the game list")
    s.add_argument("--json", action="store_true")
    s.set_defaults(fn=cmd_roms)
    s = sub.add_parser("launch", help="start a game (rom path, or part of a name / path)")
    s.add_argument("match")
    s.add_argument("--no-wait", action="store_true")
    s.add_argument("--timeout", type=float, default=60)
    s.set_defaults(fn=cmd_launch)
    s = sub.add_parser("quit", help="quit the running game")
    s.add_argument("--no-wait", action="store_true")
    s.set_defaults(fn=cmd_quit)
    s = sub.add_parser("press", help="press buttons (a b x y select start up down left right l r l2 r2; a+b = together)")
    s.add_argument("buttons", nargs="+")
    s.add_argument("--hold", type=int, default=100, help="ms each press is held")
    s.add_argument("--gap", type=int, default=100, help="ms after each press")
    s.add_argument("--repeat", type=int, default=1)
    s.set_defaults(fn=cmd_press)
    s = sub.add_parser("hold", help="hold buttons until the next press/hold (hold with none releases)")
    s.add_argument("buttons", nargs="*")
    s.set_defaults(fn=cmd_hold)
    for name, fn in (("save", cmd_save), ("load", cmd_load)):
        s = sub.add_parser(name, help=f"{name} a save state")
        s.add_argument("slot", type=int)
        s.set_defaults(fn=fn)
    s = sub.add_parser("screenshot", help="the running game's frame as a PNG")
    s.add_argument("-o", "--output")
    s.add_argument("--byteswap", action=argparse.BooleanOptionalAction, default=None,
                   help="RGB565 big-endian (default: the byte order the device reports)")
    s.set_defaults(fn=cmd_screenshot)
    s = sub.add_parser("ls", help="list a directory on the SD card")
    s.add_argument("path", nargs="?", default="")
    s.set_defaults(fn=cmd_ls)
    s = sub.add_parser("push", help="copy a file / directory to the SD card (relative paths are from its root)")
    s.add_argument("local")
    s.add_argument("remote")
    s.set_defaults(fn=cmd_push)
    s = sub.add_parser("pull", help="copy a file from the SD card")
    s.add_argument("remote")
    s.add_argument("local", nargs="?")
    s.set_defaults(fn=cmd_pull)
    s = sub.add_parser("rm", help="delete a file / directory on the SD card")
    s.add_argument("path")
    s.set_defaults(fn=cmd_rm)
    s = sub.add_parser("mkdir")
    s.add_argument("path")
    s.set_defaults(fn=cmd_mkdir)
    s = sub.add_parser("add-rom", help="copy a game to the card and list it in metadata.csv")
    s.add_argument("rom")
    s.add_argument("--dir", required=True, help="directory on the card, e.g. genesis")
    s.add_argument("--name")
    s.add_argument("--boxart", help="a JPEG, copied to boxart/")
    s.set_defaults(fn=cmd_add_rom)
    s = sub.add_parser("remove-rom", help="take a game out of metadata.csv")
    s.add_argument("rom", help="its path on the card, as in metadata.csv")
    s.add_argument("--delete", action="store_true", help="also delete the file")
    s.set_defaults(fn=cmd_remove_rom)
    sub.add_parser("reload", help="re-read metadata.csv").set_defaults(fn=cmd_reload)
    s = sub.add_parser("drive", help="hand the SD card to the PC as a USB drive (on) or take it back (off)")
    s.add_argument("state", choices=("on", "off"))
    s.set_defaults(fn=cmd_drive)
    s = sub.add_parser("reboot")
    s.add_argument("--bootloader", action="store_true", help="into the ROM download mode (for idf.py flash)")
    s.set_defaults(fn=cmd_reboot)
    s = sub.add_parser("log", help="print (or save) the console")
    s.add_argument("-o", "--output")
    s.add_argument("--duration", type=float, default=0, help="seconds (0 = until ^C)")
    s.add_argument("--until", help="stop once this text is seen")
    s.add_argument("--follow", action="store_true", help="keep waiting for the port to (re)appear")
    s.add_argument("--wait", type=float, default=10, help="seconds to wait for the port")
    s.set_defaults(fn=cmd_log)
    s = sub.add_parser("coredump", help="the last crash: summary, image, erase")
    s.add_argument("-o", "--output", help="save the raw core dump")
    s.add_argument("--erase", action="store_true")
    s.set_defaults(fn=cmd_coredump)
    s = sub.add_parser("ota", help="flash a new app (build/<app>.bin) over USB")
    s.add_argument("binary")
    s.add_argument("--wait", type=float, default=90, help="seconds to wait for it to come up (0 = do not)")
    s.set_defaults(fn=cmd_ota)
    return p


def main(argv: Optional[list] = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.fn(args)
    except (LinkError, ControlError, ValueError) as e:
        print(f"emulink: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
