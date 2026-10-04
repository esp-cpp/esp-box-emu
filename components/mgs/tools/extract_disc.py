"""Pull the game's files out of a Metal Gear Solid disc image.

The disc is a raw 2352-byte-per-sector .bin (Mode 2 Form 1), so the 2048 bytes
of user data start 24 bytes into each sector. Reads the ISO9660 directory and
writes MGS/* out to a directory the port can point at.

STAGE.DIR is 71 MB and will not fit the board's 8 MB FAT partition, so it can
also be split per stage: its first sector is a table of (8-char name, start
sector) and each stage runs until the next one starts.

    python extract_disc.py <disc.bin> <outdir> [--stages s07a,brf,init]

With --stages, only those stage blocks are written (as MGS/STAGE/<name>.bin)
plus a rebuilt STAGE.DIR describing just them -- small enough to flash.
"""
import os
import struct
import sys

SECTOR = 2352


def sniff_data_offset(fh):
    """Mode 1 keeps user data at +16, Mode 2 Form 1 at +24."""
    fh.seek(16 * SECTOR)
    raw = fh.read(SECTOR)
    at = raw.find(b"CD001")
    if at < 0:
        raise SystemExit("not an ISO9660 image")
    return at - 1


class Disc(object):
    def __init__(self, path):
        self.fh = open(path, "rb")
        self.off = sniff_data_offset(self.fh)

    def sector(self, n):
        self.fh.seek(n * SECTOR + self.off)
        return self.fh.read(2048)

    def read(self, lba, nbytes):
        out = bytearray()
        n = 0
        while len(out) < nbytes:
            out += self.sector(lba + n)
            n += 1
        return bytes(out[:nbytes])

    def listdir(self, lba, length):
        buf = b"".join(self.sector(lba + i)
                       for i in range((length + 2047) // 2048))
        out, i = [], 0
        while i < len(buf):
            rec_len = buf[i]
            if rec_len == 0:
                i = (i // 2048 + 1) * 2048
                if i >= len(buf):
                    break
                continue
            rec = buf[i:i + rec_len]
            name = rec[33:33 + rec[32]].decode("ascii", "replace")
            if name not in ("\x00", "\x01"):
                out.append((name.split(";")[0],
                            int.from_bytes(rec[2:6], "little"),
                            int.from_bytes(rec[10:14], "little"),
                            bool(rec[25] & 2)))
            i += rec_len
        return out


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    disc = Disc(sys.argv[1])
    outdir = sys.argv[2]
    want = None
    for a in sys.argv[3:]:
        if a.startswith("--stages="):
            want = a.split("=", 1)[1].split(",")

    pvd = disc.sector(16)
    root_lba = int.from_bytes(pvd[156 + 2:156 + 6], "little")
    root_len = int.from_bytes(pvd[156 + 10:156 + 14], "little")

    mgs = [e for e in disc.listdir(root_lba, root_len) if e[0] == "MGS"]
    if not mgs:
        raise SystemExit("no MGS directory on this disc")
    entries = disc.listdir(mgs[0][1], mgs[0][2])

    os.makedirs(os.path.join(outdir, "MGS"), exist_ok=True)
    for name, lba, size, isdir in entries:
        if isdir:
            continue
        if name == "STAGE.DIR" and want is not None:
            continue          # handled below
        dst = os.path.join(outdir, "MGS", name)
        print("  %-16s %12d bytes" % (name, size))
        with open(dst, "wb") as f:
            f.write(disc.read(lba, size))

    stage = [e for e in entries if e[0] == "STAGE.DIR"]
    if not stage:
        return
    lba, size = stage[0][1], stage[0][2]
    head = disc.sector(lba)
    table_size = struct.unpack("<i", head[0:4])[0]
    n = table_size // 12
    table = []
    for i in range(n):
        nm, off = struct.unpack("<8si", head[4 + i * 12:16 + i * 12])
        table.append((nm.decode("ascii", "replace").rstrip("\0"), off))
    print("\nSTAGE.DIR: %d stages, %d bytes total" % (n, size))

    if want is None:
        return

    os.makedirs(os.path.join(outdir, "MGS", "STAGE"), exist_ok=True)
    total = 0
    for i, (nm, off) in enumerate(table):
        if nm not in want:
            continue
        end = table[i + 1][1] if i + 1 < len(table) else (size // 2048)
        nsec = end - off
        blob = disc.read(lba + off, nsec * 2048)
        dst = os.path.join(outdir, "MGS", "STAGE", nm + ".bin")
        with open(dst, "wb") as f:
            f.write(blob)
        total += len(blob)
        print("  stage %-8s %8d bytes" % (nm, len(blob)))
    print("selected stages total: %d bytes (%.1f MB)" % (total, total / 1048576.0))


if __name__ == "__main__":
    main()
