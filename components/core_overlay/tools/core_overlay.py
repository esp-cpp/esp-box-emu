#!/usr/bin/env python3
"""The game cores' code overlay (see ../overlay.ld and ../include/core_overlay.h).

Every core's code is linked at one instruction-bus address and the core a game
needs is copied into PSRAM there when it launches, so the code of the cores
that are not running is in flash only (the rest of the firmware is copied into
PSRAM at boot, CONFIG_SPIRAM_FETCH_INSTRUCTIONS). Three steps:

  core_overlay.py rename <objcopy> <archive> <core> [--keep=REGEX ...]
      (POST_BUILD of a core's library, core_overlay_archive() in
      ../project_include.cmake) renames the archive's code (.literal*,
      .text*) to .core_ovl.<core>.* and its constructor tables (.ctors*) to
      .core_ovl_ctors_<core>*, which run on the core's first load instead of
      at boot. What stays resident (normal .flash.text):
      - COMDAT groups (inline functions, template instances): the linker
        keeps one copy for the whole firmware, which may be this archive's
        while other code uses it;
      - code whose name (after .text. / .literal. and unlikely. / hot. /
        startup. / exit.) matches a --keep regex: code the rest of the
        firmware calls although it is in a core's archive (e.g. Dark Forces'
        firmware-wide free() wrapper).
      Running it again does nothing (the renamed sections match nothing).

  core_overlay.py size <objcopy> <out.h> <archive>...
      writes CORE_OVL_STORE_SIZE: what the flash store (src/store.c, in
      .flash.rodata) needs for every renamed archive's overlay code, an upper
      bound of the linked size (the link drops unused functions).

  core_overlay.py image <objcopy> <app.elf> <app.map> [--report]
      (POST_BUILD of the app, before the .bin is made) copies each linked,
      relocated member .core_ovl.<core> into the flash store behind a header
      (magic, then per core in CORES order its offset in the store and its
      size), marks the members NOBITS so elf2image leaves the instruction-bus
      sections out of the image, and checks that nothing resident reaches
      into the overlay, because only one core's code is there at a time (or
      none):
      - no boot-time constructor points into the overlay;
      - every resident word that is the address of an overlay function (on
        Xtensa every reference from resident code is one: a literal for
        l32r, as calls cannot reach from 0x42... to 0x438...) is in the carts
        (main.cpp, which load the core first), the loader, or the same core's
        archive: its data (jump tables, vtables), its internal-RAM code, or
        resident code of its own that is local or that only its own archive
        references (the linker's cross-reference table).
      A finding names the resident code and the overlay function it reaches;
      the usual fix is a --keep regex for that function.
"""
import bisect
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from array import array

# the order of the store header's entries (src/core_overlay.c has the same)
CORES = ("nes", "gbc", "sms", "msx", "genesis", "doom", "darkforces", "pico8")
MAGIC = 0x4C564F43  # "COVL"
CART_FILES = ("libmain.a(main.cpp.obj)",)
LOADER_ARCHIVE = "libcore_overlay.a("


def tool(objcopy, name):
    """another binutils program next to objcopy (xtensa-esp32s3-elf-objcopy -> ...-readelf)"""
    path = re.sub(r"objcopy(\.exe)?$", name + r"\1", objcopy)
    return path if os.path.exists(path) or shutil.which(path) else name


def align(value, to):
    return (value + to - 1) // to * to if to > 1 else value


SECTION = re.compile(r"^\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([A-Za-z]*)\s+\d+\s+\d+\s+(\d+)\s*$")


def archive_sections(objcopy, archive):
    """(member, name, size, flags, alignment) of every section in the archive"""
    out = subprocess.run([tool(objcopy, "readelf"), "-S", "-W", archive], check=True, capture_output=True,
                         text=True).stdout
    member = None
    for line in out.splitlines():
        if line.startswith("File: "):
            member = line[6:].strip()
            continue
        m = SECTION.match(line)
        if m:
            yield member, m.group(1), int(m.group(3), 16), m.group(4), int(m.group(5))


CODE = re.compile(r"^\.(literal|text)(\..*)?$")
CTORS = re.compile(r"^\.ctors(\..*)?$")


def function_of(name):
    """the function a code section holds (.text.foo / .literal.unlikely.foo -> foo), or ''"""
    parts = name.split(".", 2)
    fn = parts[2] if len(parts) == 3 else ""
    for sub in ("unlikely.", "hot.", "startup.", "exit."):
        if fn.startswith(sub):
            return fn[len(sub):]
    return fn


def rename(objcopy, archive, core, keeps):
    keep = [re.compile(k) for k in keeps]
    grouped, names = set(), set()
    for _, name, _, flags, _ in archive_sections(objcopy, archive):
        names.add(name)
        if "G" in flags:
            grouped.add(name)
    renames = {}
    for name in names:
        if name in grouped:
            continue  # COMDAT: may be the copy the whole firmware uses
        if CODE.match(name):
            fn = function_of(name)
            if fn and any(k.search(fn) for k in keep):
                continue
            renames[name] = f".core_ovl.{core}{name}"
        elif CTORS.match(name):
            renames[name] = f".core_ovl_ctors_{core}{name[len('.ctors'):]}"
    if not renames:
        return
    # the options in a response file: a big core has tens of thousands of sections
    with tempfile.NamedTemporaryFile("w", suffix=".rsp", delete=False) as rsp:
        for old in sorted(renames):
            rsp.write(f"--rename-section {old}={renames[old]}\n")
    try:
        subprocess.run([objcopy, f"@{rsp.name}", archive], check=True)
    finally:
        os.unlink(rsp.name)
    ctors = sum(1 for n in renames.values() if n.startswith(".core_ovl_ctors_"))
    print(f"core_overlay: moved {len(renames) - ctors} code sections of {os.path.basename(archive)} into the "
          f"{core} overlay ({ctors} constructor tables); {len(grouped & {n for n in names if CODE.match(n)})} "
          "COMDAT code sections stay resident")


def overlay_size(objcopy, archive):
    """the core and the bytes its renamed sections take (each at its alignment)"""
    core, total = None, 0
    for _, name, size, _, al in archive_sections(objcopy, archive):
        m = re.match(r"^\.core_ovl(?:\.|_ctors_)([a-z0-9]+)", name)
        if m and size:
            core = m.group(1)
            total = align(total, max(al, 1)) + size
    return core, total


def header_size():
    return align(8 + 8 * len(CORES), 16)


def size(objcopy, out, archives):
    total = header_size()
    parts = []
    for a in archives:
        core, n = overlay_size(objcopy, a)
        if core:
            total += align(n, 16)
            parts.append(f"{core} {n // 1024} KB")
    text = ("// generated by components/core_overlay/tools/core_overlay.py size: the flash store\n"
            f"// for the cores' code overlay ({', '.join(parts)}, before the link drops unused code)\n"
            f"#define CORE_OVL_STORE_SIZE {total}u\n")
    if not os.path.exists(out) or open(out).read() != text:
        open(out, "w").write(text)
    print(f"core_overlay: the flash store takes {total // 1024} KB ({', '.join(parts)})")


SHT_PROGBITS, SHT_NOBITS = 1, 8
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 0x1, 0x2, 0x4


def image(objcopy, elf_path, map_path, report):
    elf = bytearray(open(elf_path, "rb").read())
    (shoff,) = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x2E)
    secs = [list(struct.unpack_from("<10I", elf, shoff + i * shentsize)) for i in range(shnum)]

    def cstr(off):
        return bytes(elf[off: elf.index(b"\0", off)]).decode()

    names = [cstr(secs[shstrndx][4] + s[0]) for s in secs]
    symtab = secs[names.index(".symtab")]
    strtab_off = secs[symtab[6]][4]
    symbols = {}  # name -> (value, size, shndx, bind)
    funcs = []  # (value, size, name, bind, shndx)
    for off in range(symtab[4], symtab[4] + symtab[5], 16):
        st_name, value, sz, info, _, shndx = struct.unpack_from("<IIIBBH", elf, off)
        name = cstr(strtab_off + st_name)
        symbols.setdefault(name, (value, sz, shndx, info >> 4))
        if info & 0xF == 2 and sz:  # STT_FUNC
            funcs.append((value, sz, name, info >> 4, shndx))
    funcs.sort()

    def sym(name):
        if name not in symbols:
            sys.exit(f"core_overlay: {name} is not linked (is components/core_overlay/overlay.ld in the link?)")
        return symbols[name]

    ovl_start, ovl_end = sym("_core_ovl_start")[0], sym("_core_ovl_end")[0]
    in_ovl = lambda a: ovl_start <= a < ovl_end
    store_addr, store_size, store_shndx, _ = sym("core_ovl_store")
    store_sec = secs[store_shndx]
    store_off = store_sec[4] + store_addr - store_sec[3]

    # 1. the store: header, then each member at 16-byte alignment
    member_index = {f".core_ovl.{c}": i for i, c in enumerate(CORES)}
    (magic,) = struct.unpack_from("<I", elf, store_off)
    members = [(i, names[i]) for i in range(len(secs)) if names[i] in member_index]
    if magic != MAGIC:
        entries = [(0, 0)] * len(CORES)
        at = header_size()
        for i, name in members:
            s = secs[i]
            if s[1] != SHT_PROGBITS:
                sys.exit(f"core_overlay: {name} is NOBITS but the store is empty (relink)")
            if at + s[5] > store_size:
                sys.exit(f"core_overlay: the overlay code ({at + s[5]} B so far) does not fit the {store_size} B "
                         "flash store (src/store.c; the size step under-counted)")
            elf[store_off + at: store_off + at + s[5]] = elf[s[4]: s[4] + s[5]]
            entries[member_index[name]] = (at, s[5])
            at = align(at + s[5], 16)
        hdr = struct.pack("<II", MAGIC, len(CORES)) + b"".join(struct.pack("<II", *e) for e in entries)
        elf[store_off: store_off + len(hdr)] = hdr
        for i, _ in members:
            struct.pack_into("<I", elf, shoff + i * shentsize + 4, SHT_NOBITS)
            secs[i][1] = SHT_NOBITS
        open(elf_path, "wb").write(elf)
        sizes = ", ".join(f"{n[len('.core_ovl.'):]} {secs[i][5] // 1024} KB" for i, n in members)
        print(f"core_overlay: {sum(secs[i][5] for i, _ in members) // 1024} KB of core code stored in flash "
              f"({at // 1024} of the store's {store_size // 1024} KB; {sizes}); overlay at {ovl_start:#x}, "
              f"largest {(ovl_end - ovl_start) // 1024} KB")

    member_of_shndx = {i: n[len(".core_ovl."):] for i, n in members}
    member_size = {n[len(".core_ovl."):]: secs[i][5] for i, n in members}
    ovl_funcs = [f for f in funcs if in_ovl(f[0])]
    ovl_targets = {}  # function start -> [(core, name)]
    for v, sz, n, b, shndx in ovl_funcs:
        ovl_targets.setdefault(v, []).append((member_of_shndx.get(shndx, "?"), n))

    # the map: input sections (start, size, file, section), and the
    # cross-reference table (demangled symbol -> the files that reference it)
    inputs, refs = [], {}
    lines = open(map_path, errors="replace").read().split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("Linker script and memory map"))
    pending, cref = None, len(lines)
    row = re.compile(r"^ (\S+)?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (\S.*)$")
    for k in range(start, len(lines)):
        line = lines[k]
        if line.startswith("Cross Reference Table"):
            cref = k
            break
        m = row.match(line)
        if m:
            sec = m.group(1) or pending
            addr, sz = int(m.group(2), 16), int(m.group(3), 16)
            if sz and addr and not in_ovl(addr):
                inputs.append((addr, sz, m.group(4).strip(), sec or ""))
            pending = None
        elif line.startswith(" ") and not line.startswith("  ") and len(line.split()) == 1:
            pending = line.strip()
        else:
            pending = None
    # a symbol line ends with the file that defines it, unless the (demangled)
    # name is too long: then the file is on the next, indented line
    current = None
    file_at_end = re.compile(r"^(.*\S)\s+(\S+\.(?:a\(\S+\)|obj|o))$")
    for line in lines[cref + 3:]:
        if not line.strip():
            continue
        if line[0] != " ":
            m = file_at_end.match(line.rstrip())
            current = m.group(1).strip() if m else line.strip()
            refs[current] = []  # (the first file defines it)
        elif current is not None:
            refs[current].append(line.strip())
    inputs.sort()
    input_starts = [x[0] for x in inputs]

    def attribute(addr):
        j = bisect.bisect_right(input_starts, addr) - 1
        if j >= 0 and inputs[j][0] <= addr < inputs[j][0] + inputs[j][1]:
            return inputs[j][2], inputs[j][3]
        return "?", "?"

    def core_of(file):
        for c in CORES:
            if f"/lib{c}.a(" in file or file.startswith(f"lib{c}.a("):
                return c
        return None

    demangled = {}

    def demangle(todo):
        todo = [n for n in todo if n not in demangled]
        if not todo:
            return
        out = subprocess.run([tool(objcopy, "c++filt")], input="\n".join(todo), capture_output=True,
                             text=True).stdout.split("\n")
        for n, d in zip(todo, out):
            demangled[n] = d or n

    local = {n for v, sz, n, b, s in funcs if b == 0}
    problems, allowed = [], []

    def describe(target):
        return " / ".join(f"{c}:{n}" for c, n in ovl_targets.get(target, [])[:3])

    # 2a. boot-time constructors and init functions
    for lo, hi in (("__preinit_array_start", "__init_array_end"),):
        a, b = sym(lo)[0], sym(hi)[0]
        for i, s in enumerate(secs):
            if s[1] != SHT_NOBITS and s[3] <= a < s[3] + s[5]:
                for (w,) in struct.iter_unpack("<I", elf[s[4] + a - s[3]: s[4] + b - s[3]]):
                    if in_ovl(w):
                        problems.append(f"boot constructor {w:#x} ({describe(w)}) is in the overlay")
                break

    # 2b. resident words that are the address of an overlay function
    hits = []  # (address, target, in executable memory)
    for i, s in enumerate(secs):
        if s[1] in (0, SHT_NOBITS) or not s[2] & SHF_ALLOC or in_ovl(s[3]) or i in member_of_shndx:
            continue
        executable = bool(s[2] & SHF_EXECINSTR)
        data = bytes(elf[s[4]: s[4] + s[5]])
        lo, hi = s[3], s[3] + s[5]
        if lo <= store_addr < hi:  # (the store holds the overlay code itself)
            data = data[: store_addr - lo] + bytes(store_size) + data[store_addr - lo + store_size:]
        skew = (-lo) & 3
        words = array("I")
        words.frombytes(data[skew: skew + ((len(data) - skew) & ~3)])
        for o, w in enumerate(words):
            if w in ovl_targets:
                hits.append((lo + skew + 4 * o, w, executable))
    by_site = {}
    for at, target, executable in hits:
        file, sec = attribute(at)
        if executable and "literal" not in sec:
            continue  # instructions (code reaches out only through its literals)
        by_site.setdefault((file, sec), []).append((at, target))
    demangle([function_of(sec) for (file, sec) in by_site if function_of(sec)])
    for (file, sec), targets in sorted(by_site.items()):
        at, target = targets[0]
        core = core_of(file)
        what = (f"{sec} [{file}] @{at:#x} -> {describe(target)}"
                + (f" and {len(targets) - 1} more" if len(targets) > 1 else ""))
        if file.endswith(CART_FILES) or LOADER_ARCHIVE in file:
            ok = True
        elif core is None:
            ok = False
        elif any(t >= ovl_start + member_size.get(core, 0) for _, t in targets):
            # (every core's code starts at the overlay's start, so another core
            # may have a function at the same address, and a core's tables may
            # point into its functions, computed gotos: only a target past this
            # core's code is surely another core's)
            ok, what = False, what + "; another core's code"
        elif not CODE.match(sec):
            ok = True  # the core's own data, or its internal-RAM code
        else:
            fn = function_of(sec)
            if not fn:
                ok, what = False, what + "; resident code of unknown function"
            elif fn in local:
                ok = True  # only its own object calls it
            else:
                # (the map's names are mangled; a constructor's / destructor's
                # COMDAT group is named C5 / D5 after the C1 C2 / D1 D2 it holds)
                variants = [fn] + [re.sub(r"([CD])5E", rf"\g<1>{k}E", fn, count=1) for k in "12"]
                users = next((refs[v] for v in variants if v in refs), refs.get(demangled.get(fn, fn)))
                ok = users is not None and all(core_of(u) == core for u in users)
                if not ok:
                    others = sorted({u for u in (users or ["?"]) if core_of(u) != core})
                    what += f"; used by {', '.join(others[:3])}"
        (allowed if ok else problems).append(what)

    if report:
        for a in allowed:
            print("core_overlay: allowed", a)
    if problems:
        for p in sorted(set(problems)):
            print("core_overlay: ERROR", p)
        sys.exit(f"core_overlay: {len(set(problems))} references into the core overlay from resident code or "
                 "data that may run while another core (or none) is loaded (see the script's docstring)")
    print(f"core_overlay: checked: {len(allowed)} resident places reach into the overlay, all the carts', the "
          "loader's or the cores' own")


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd, args = argv[1], argv[2:]
    if cmd == "rename":
        objcopy, archive, core = args[:3]
        if core not in CORES:
            sys.exit(f"core_overlay: unknown core {core} (CORES: {', '.join(CORES)})")
        rename(objcopy, archive, core, [a.split("=", 1)[1] for a in args[3:] if a.startswith("--keep=")])
    elif cmd == "size":
        size(args[0], args[1], args[2:])
    elif cmd == "image":
        image(args[0], args[1], args[2], "--report" in args[3:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
