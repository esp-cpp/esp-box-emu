#!/usr/bin/env python3
"""Capture the box's serial console to a file for tools/perf_report.py.

    python tools/serial_capture.py PORT [--seconds N] [--out FILE] [--reset] [--until REGEX]

Opens the port plainly (no explicit DTR/RTS -- toggling them resets the ESP32-S3
through the USB-Serial-JTAG controller), reconnects if the port drops (the
console disappears for a moment when the box resets or exposes USB mass
storage), and tees everything to stdout. --reset pulses the chip reset once
after the first connect so the capture starts from boot. --until stops the
capture as soon as a line matches the regex (e.g. 'phase=quit').

Only one reader may be attached to the port at a time: stop this before flashing
or starting idf.py monitor.
"""
import argparse
import re
import sys
import time
from datetime import datetime

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--seconds", type=float, default=120.0, help="capture duration (default 120)")
    ap.add_argument("--out", help="output file (default perf_<timestamp>.log)")
    ap.add_argument("--reset", action="store_true", help="pulse the chip reset after connecting")
    ap.add_argument("--until", help="stop once a line matches this regex")
    ap.add_argument("--quiet", action="store_true", help="do not echo to stdout")
    args = ap.parse_args()

    out = args.out or datetime.now().strftime("perf_%Y%m%d_%H%M%S.log")
    until = re.compile(args.until) if args.until else None
    end = time.time() + args.seconds
    reset = args.reset
    partial = b""

    with open(out, "ab") as f:
        while time.time() < end:
            try:
                s = serial.Serial(args.port, 115200, timeout=0.5)
            except Exception:
                time.sleep(0.5)
                continue
            try:
                if reset:
                    reset = False
                    s.dtr = False
                    s.rts = True
                    time.sleep(0.1)
                    s.rts = False
                    time.sleep(0.1)
                while time.time() < end:
                    d = s.read(4096)
                    if not d:
                        continue
                    f.write(d)
                    f.flush()
                    if not args.quiet:
                        sys.stdout.buffer.write(d)
                        sys.stdout.buffer.flush()
                    if until:
                        partial += d
                        *lines, partial = partial.split(b"\n")
                        if any(until.search(l.decode("utf-8", "replace")) for l in lines):
                            print(f"\n[serial_capture] matched --until, stopping", file=sys.stderr)
                            return 0
            except Exception as e:
                f.write(f"\n[serial_capture] port dropped: {e}\n".encode())
                f.flush()
                time.sleep(0.5)
            finally:
                try:
                    s.close()
                except Exception:
                    pass
    print(f"\n[serial_capture] wrote {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
