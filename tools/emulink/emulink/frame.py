"""The espp stream_frame v2 codec (see espp/components/stream_frame).

[magic u16 = 0x4F54]["flags u8"][module u8][type u8]{[correlation u16]}[len u32][payload][crc32 u32]
little-endian; flags bit0 = reply, bit1 = correlation present, bits 4..7 = version (1).
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from typing import List, Optional

MAGIC = 0x4F54
MAGIC_BYTES = struct.pack("<H", MAGIC)
VERSION = 1
FLAG_REPLY = 0x01
FLAG_CORRELATION = 0x02
MAX_PAYLOAD = 4096


@dataclass
class Frame:
    flags: int
    module: int
    type: int
    payload: bytes
    correlation: Optional[int] = None

    @property
    def is_reply(self) -> bool:
        return bool(self.flags & FLAG_REPLY)


def build(module: int, type_: int, payload: bytes = b"", reply: bool = False,
          correlation: Optional[int] = None) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large ({len(payload)} > {MAX_PAYLOAD})")
    flags = (VERSION << 4) | (FLAG_REPLY if reply else 0) | (FLAG_CORRELATION if correlation is not None else 0)
    out = bytearray(MAGIC_BYTES)
    out += bytes([flags, module & 0xFF, type_ & 0xFF])
    if correlation is not None:
        out += struct.pack("<H", correlation & 0xFFFF)
    out += struct.pack("<I", len(payload))
    out += payload
    out += struct.pack("<I", zlib.crc32(bytes(out)) & 0xFFFFFFFF)
    return bytes(out)


class Parser:
    """Incremental, resynchronizing frame parser."""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> List[Frame]:
        self._buf += data
        frames: List[Frame] = []
        while True:
            i = self._buf.find(MAGIC_BYTES)
            if i < 0:
                # keep a trailing half magic
                del self._buf[:-1]
                return frames
            if i:
                del self._buf[:i]
            if len(self._buf) < 5:
                return frames
            flags = self._buf[2]
            hdr = 5 + (2 if flags & FLAG_CORRELATION else 0) + 4
            if len(self._buf) < hdr:
                return frames
            corr = struct.unpack_from("<H", self._buf, 5)[0] if flags & FLAG_CORRELATION else None
            length = struct.unpack_from("<I", self._buf, hdr - 4)[0]
            if length > MAX_PAYLOAD:
                del self._buf[:2]  # not a frame: resync past this magic
                continue
            total = hdr + length + 4
            if len(self._buf) < total:
                return frames
            crc = struct.unpack_from("<I", self._buf, hdr + length)[0]
            if zlib.crc32(bytes(self._buf[: hdr + length])) & 0xFFFFFFFF != crc:
                del self._buf[:2]
                continue
            frames.append(Frame(flags, self._buf[3], self._buf[4], bytes(self._buf[hdr: hdr + length]), corr))
            del self._buf[:total]
