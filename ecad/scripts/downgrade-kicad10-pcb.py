#!/usr/bin/env python3
"""Downgrade a KiCad 10 board file (version 20260206) to KiCad 9 format
(20241229) so atopile <= 0.15.x can parse it.

KiCad 10 made two breaking changes atopile's parser chokes on:
  - `(tenting (front yes) (back yes))` nested blocks
  - net references are name-only `(net "foo")` with no numbered net table

This strips the former and reconstructs the latter. Placement and routing
geometry are untouched. Run it on any board KiCad 10 has saved, then run
`ato build -b <build>` to let atopile normalize the file fully.

Usage: python3 scripts/downgrade-kicad10-pcb.py <board.kicad_pcb> [...]
"""

import re
import sys


def downgrade(path: str) -> None:
    with open(path) as f:
        s = f.read()

    if "(version 20241229)" in s:
        print(f"{path}: already KiCad 9 format, skipping")
        return

    s = re.sub(r"\(tenting\s*(\((front|back)\s+\w+\)\s*)+\)", "", s)
    s = re.sub(r"\(version \d+\)", "(version 20241229)", s, count=1)
    s = re.sub(r'\(generator_version "[^"]*"\)', '(generator_version "9.0")', s)

    # rebuild the numbered net table if nets are name-only
    if not re.search(r"^\t\(net \d+ ", s, re.M):
        names: list[str] = []
        for n in re.findall(r'\(net\s+"([^"]*)"\)', s):
            if n not in names:
                names.append(n)
        num = {n: i + 1 for i, n in enumerate(names)}
        s = re.sub(
            r'\(net\s+"([^"]*)"\)',
            lambda m: f'(net {num[m.group(1)]} "{m.group(1)}")',
            s,
        )
        table = '\t(net 0 "")\n' + "".join(
            f'\t(net {i + 1} "{n}")\n' for i, n in enumerate(names)
        )
        idx = s.find("\n\t(footprint")
        if idx == -1:
            sys.exit(f"{path}: no footprints found; refusing to write")
        s = s[: idx + 1] + table + s[idx + 1 :]
        print(f"{path}: downgraded, {len(names)} nets renumbered")
    else:
        print(f"{path}: downgraded (net table already present)")

    with open(path, "w") as f:
        f.write(s)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for p in sys.argv[1:]:
        downgrade(p)
