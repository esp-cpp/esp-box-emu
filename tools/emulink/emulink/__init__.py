"""emulink: drive esp-box-emu / tab5-emu over their composite USB device.

The device's vendor (WebUSB) interface carries espp `stream_frame` frames
routed by module: OTA (0, esp-box-emu), core dump (4), system (7), monitor (8)
and the emulator's test control (0x20). The CDC interface carries the console
and the MSC interface is the SD card (when handed to the PC).
"""

from .link import Link, LinkError  # noqa: F401
from .control import Control, ControlError, Buttons  # noqa: F401

__version__ = "0.1.0"
