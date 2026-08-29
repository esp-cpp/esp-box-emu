#!/usr/bin/env python3
"""Repair duplicate designators in a board so `ato build` can run again.

KiCad's "Update Footprints from Library..." resets footprint references to
the library defaults (U2, U5, U$3, REF**, ...), which creates duplicates
that make atopile refuse to build ("Duplicate designators found in
layout"). This blanks the Reference on every footprint involved in a
duplicate; the next `ato build -b <build>` assigns fresh unique
designators (with the correct per-part prefixes) and leaves unique
references untouched.

Usage: python3 scripts/fix-duplicate-refs.py <board.kicad_pcb> [...]
"""

import shutil
import sys
from collections import Counter
from pathlib import Path


def _reexec_with_atopile_python() -> None:
    import os

    ato = shutil.which("ato")
    if not ato:
        sys.exit("error: faebryk not importable and `ato` not on PATH")
    shebang = Path(ato).read_text(errors="ignore").splitlines()[0]
    if not shebang.startswith("#!"):
        sys.exit(f"error: can't determine atopile's python from {ato}")
    python = shebang[2:].strip()
    os.execv(python, [python, *sys.argv])


try:
    import faebryk  # noqa: F401
except ImportError:
    _reexec_with_atopile_python()

from faebryk.libs.kicad.fileformats import Property, kicad  # noqa: E402


def fix(path: Path) -> None:
    lock = path.parent / f"~{path.name}.lck"
    if lock.exists():
        sys.exit(f"{path.name}: board appears to be open in KiCad "
                 f"({lock.name} exists) - close it first")

    pcb_file = kicad.loads(kicad.pcb.PcbFile, path)
    pcb = pcb_file.kicad_pcb

    refs = Counter()
    for fp in pcb.footprints:
        r = Property.try_get_property(fp.propertys, "Reference")
        if r:
            refs[r] += 1
    dups = {r for r, n in refs.items() if n > 1}
    if not dups:
        print(f"{path.name}: no duplicate references")
        return

    blanked = 0
    for fp in pcb.footprints:
        r = Property.try_get_property(fp.propertys, "Reference")
        if r in dups:
            for p in fp.propertys:
                if p.name == "Reference":
                    p.value = ""
                    blanked += 1
    kicad.dumps(pcb_file, path)
    print(f"{path.name}: blanked {blanked} footprints with duplicated "
          f"references {sorted(dups)} - run `ato build` to renumber")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for p in sys.argv[1:]:
        fix(Path(p))
