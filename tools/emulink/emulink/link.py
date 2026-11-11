"""The vendor-interface link to an emulator (pyusb), and the device's CDC port."""

from __future__ import annotations

import itertools
import os
import time
from collections import deque
from typing import Deque, List, Optional

from . import frame as _f

#: espp::UsbDevice's default identity (pid.codes VID), which both emulators keep
#: so the stock espp tools (espp_ota, espp_coredump, the web consoles) find them.
VID = 0x1209
PID = 0x0D32
#: The USB serial numbers the emulators report.
SERIALS = ("tab5-emu", "esp-box-emu")


class LinkError(RuntimeError):
    pass


def _usb():
    try:
        import usb.core
        import usb.util
    except ImportError as e:  # pragma: no cover
        raise LinkError("pyusb is required (it is in the ESP-IDF Python env; or pip install pyusb, "
                        "and brew install libusb on macOS)") from e
    return usb.core, usb.util


def find_devices() -> List[dict]:
    """The emulators on the bus: [{serial, product, bus, address}]."""
    core, util = _usb()
    out = []
    for dev in core.find(find_all=True, idVendor=VID, idProduct=PID) or []:
        try:
            serial = util.get_string(dev, dev.iSerialNumber)
            product = util.get_string(dev, dev.iProduct)
        except Exception:
            continue
        if serial in SERIALS:
            out.append({"serial": serial, "product": product, "bus": dev.bus, "address": dev.address, "dev": dev})
    return out


class Link:
    """One open vendor interface. Also a transport for the espp host clients
    (``write(bytes, timeout_ms)`` / ``read(max_len, timeout_ms)``)."""

    def __init__(self, serial: Optional[str] = None) -> None:
        self.serial = serial or os.environ.get("EMULINK_SERIAL")
        self._dev = None
        self._itf = None
        self._ep_in = None
        self._ep_out = None
        self._parser = _f.Parser()
        self._pending: Deque[_f.Frame] = deque()
        self._corr = itertools.count(1)
        self.product = ""

    # -- lifecycle -------------------------------------------------------------
    def open(self, timeout_s: float = 0.0) -> "Link":
        core, util = _usb()
        deadline = time.monotonic() + timeout_s
        while True:
            devs = [d for d in find_devices() if self.serial in (None, d["serial"])]
            if devs:
                break
            if time.monotonic() >= deadline:
                raise LinkError("no emulator found on USB" + (f" with serial {self.serial!r}" if self.serial else ""))
            time.sleep(0.5)
        d = devs[0]
        self._dev, self.serial, self.product = d["dev"], d["serial"], d["product"]
        try:
            return self._claim(core, util)
        except core.USBError as e:
            # e.g. still enumerating after a reset: the caller may retry
            self._dev = None
            raise LinkError(f"cannot open the device's vendor interface: {e}") from e

    def _claim(self, core, util) -> "Link":
        cfg = self._dev.get_active_configuration()
        for itf in cfg:
            if itf.bInterfaceClass != 0xFF:
                continue
            eps = list(itf)
            ep_in = next((e for e in eps if util.endpoint_direction(e.bEndpointAddress) == util.ENDPOINT_IN), None)
            ep_out = next((e for e in eps if util.endpoint_direction(e.bEndpointAddress) == util.ENDPOINT_OUT), None)
            if ep_in is not None and ep_out is not None:
                self._itf, self._ep_in, self._ep_out = itf.bInterfaceNumber, ep_in, ep_out
                break
        if self._itf is None:
            raise LinkError("the device has no vendor interface")
        try:
            if self._dev.is_kernel_driver_active(self._itf):
                self._dev.detach_kernel_driver(self._itf)
        except (NotImplementedError, core.USBError):
            pass
        util.claim_interface(self._dev, self._itf)
        return self

    def close(self) -> None:
        if self._dev is not None:
            _, util = _usb()
            try:
                util.release_interface(self._dev, self._itf)
                util.dispose_resources(self._dev)
            except Exception:
                pass
        self._dev = None
        self._itf = None

    def __enter__(self) -> "Link":
        return self.open() if self._dev is None else self

    def __exit__(self, *exc) -> None:
        self.close()

    # -- raw transport (espp clients use these) --------------------------------
    def write(self, data: bytes, timeout_ms: int = 5000) -> int:
        try:
            return self._ep_out.write(data, timeout=timeout_ms)
        except Exception as e:
            raise LinkError(f"USB write failed: {e}") from e

    def read(self, max_len: int = 16384, timeout_ms: int = 1000) -> bytes:
        core, _ = _usb()
        try:
            return bytes(self._ep_in.read(max(max_len, 64), timeout=timeout_ms))
        except core.USBTimeoutError:
            return b""
        except core.USBError as e:
            if getattr(e, "errno", None) in (60, 110) or "timed out" in str(e).lower():
                return b""
            raise LinkError(f"USB read failed: {e}") from e

    # -- request / reply -------------------------------------------------------
    def request(self, module: int, type_: int, payload: bytes = b"", timeout_s: float = 10.0,
                reply_types: Optional[tuple] = None) -> _f.Frame:
        """Send a request and return its reply (matched by module and
        correlation id; frames for anything else are dropped)."""
        corr = next(self._corr) & 0xFFFF
        try:
            self.write(_f.build(module, type_, payload, correlation=corr))
        except LinkError:
            # the host reset the interface (e.g. macOS after ejecting the
            # device's drive): open it again once
            self.close()
            self.open(timeout_s=10)
            self.write(_f.build(module, type_, payload, correlation=corr))
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            while self._pending:
                fr = self._pending.popleft()
                if fr.is_reply and fr.module == module and (fr.correlation in (None, corr)) and \
                        (reply_types is None or fr.type in reply_types):
                    return fr
            data = self.read(16384, timeout_ms=200)
            if data:
                self._pending.extend(self._parser.feed(data))
        raise LinkError(f"no reply from module {module:#x} to request {type_:#x} in {timeout_s:.0f} s")


def find_console_port(serial: Optional[str] = None) -> Optional[str]:
    """The CDC console's serial port (e.g. /dev/cu.usbmodem...) of an emulator."""
    from serial.tools import list_ports

    for p in list_ports.comports():
        if p.vid == VID and p.pid == PID and (p.serial_number in SERIALS) and serial in (None, p.serial_number):
            return p.device
    return None
