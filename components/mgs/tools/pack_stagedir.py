"""Rebuild a STAGE.DIR containing only the stages that fit on the board.

The retail archive is 71 MB across 96 stages; the ESP32's data partition is 8.
The format is simple enough to rewrite:

    sector 0:  int32 table_size, then table_size/12 entries of
               { char name[8]; int32 start_sector; }
    sector n:  the stage blocks, each starting at its recorded sector

so a subset is just the same header with recomputed offsets followed by the
selected blocks, padded to sector boundaries.

    python pack_stagedir.py <disc.bin> <out/STAGE.DIR> s07a init brf ...
"""
import os
import struct
import sys

SECTOR_RAW = 2352
SECTOR = 2048
NAME_LEN = 8
ENTRY = 12


def data_offset(fh):
    fh.seek(16 * SECTOR_RAW)
    at = fh.read(SECTOR_RAW).find(b"CD001")
    if at < 0:
        raise SystemExit("not an ISO9660 image")
    return at - 1


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    disc_path, out_path = sys.argv[1], sys.argv[2]
    wanted = sys.argv[3:]

    fh = open(disc_path, "rb")
    off = data_offset(fh)

    def sector(n):
        fh.seek(n * SECTOR_RAW + off)
        return fh.read(SECTOR)

    # locate STAGE.DIR through the ISO directory
    pvd = sector(16)
    root_lba = int.from_bytes(pvd[158:162], "little")
    root_len = int.from_bytes(pvd[166:170], "little")

    def listdir(lba, length):
        buf = b"".join(sector(lba + i) for i in range((length + 2047) // 2048))
        out, i = [], 0
        while i < len(buf):
            n = buf[i]
            if n == 0:
                i = (i // 2048 + 1) * 2048
                if i >= len(buf):
                    break
                continue
            rec = buf[i:i + n]
            nm = rec[33:33 + rec[32]].decode("ascii", "replace").split(";")[0]
            out.append((nm, int.from_bytes(rec[2:6], "little"),
                        int.from_bytes(rec[10:14], "little")))
            i += n
        return out

    mgs = [e for e in listdir(root_lba, root_len) if e[0] == "MGS"][0]
    sd = [e for e in listdir(mgs[1], mgs[2]) if e[0] == "STAGE.DIR"][0]
    base_lba, total = sd[1], sd[2]

    head = sector(base_lba)
    table_size = struct.unpack("<i", head[0:4])[0]
    count = table_size // ENTRY
    table = []
    for i in range(count):
        nm, start = struct.unpack("<8si", head[4 + i * ENTRY:4 + (i + 1) * ENTRY])
        table.append((nm.decode("ascii", "replace").rstrip("\0"), start))

    missing = [w for w in wanted if w not in [t[0] for t in table]]
    if missing:
        raise SystemExit("not on this disc: %s" % ", ".join(missing))

    # pull each requested block; a stage runs until the next one begins
    blocks = []
    for i, (nm, start) in enumerate(table):
        if nm not in wanted:
            continue
        end = table[i + 1][1] if i + 1 < len(table) else total // SECTOR
        nsec = end - start
        data = b"".join(sector(base_lba + start + k) for k in range(nsec))
        blocks.append((nm, data))

    # keep the original ordering: the game may assume the table is sorted
    order = {nm: i for i, (nm, _) in enumerate(table)}
    blocks.sort(key=lambda b: order[b[0]])

    new_table_size = len(blocks) * ENTRY
    if new_table_size > SECTOR - 4:
        raise SystemExit("too many stages for a one-sector table")

    # the table occupies sector 0, so the first block starts at sector 1
    cursor = 1
    header = bytearray(SECTOR)
    struct.pack_into("<i", header, 0, new_table_size)
    body = bytearray()
    for i, (nm, data) in enumerate(blocks):
        struct.pack_into("<8si", header, 4 + i * ENTRY,
                         nm.encode("ascii")[:NAME_LEN].ljust(NAME_LEN, b"\0"),
                         cursor)
        body += data
        cursor += len(data) // SECTOR

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(bytes(header))
        f.write(bytes(body))

    size = SECTOR + len(body)
    print("wrote %s: %d stages, %d bytes (%.2f MB)"
          % (out_path, len(blocks), size, size / 1048576.0))
    for nm, data in blocks:
        print("   %-8s %8d bytes" % (nm, len(data)))


if __name__ == "__main__":
    main()
