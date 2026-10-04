#!/usr/bin/env python3
"""Canonicalize KiCad board files for reproducible diffs.

atopile 0.15.x writes KiCad group member lists in nondeterministic order
(python set iteration), so every `ato build` permutes them and `--frozen`
/ git diffs report phantom changes. This sorts each group's member list
in place. Run after `ato build`; CI uses it before `git diff --exit-code`.

Pure text transform - no atopile/kicad dependencies.

Usage: canonicalize-boards.py <board.kicad_pcb> [...]
       canonicalize-boards.py --all   (all boards under elec/layout/)
"""

import glob
import re
import sys
from pathlib import Path


def canonicalize(path: Path) -> bool:
    s = path.read_text()

    def sort_members(m: re.Match) -> str:
        body = m.group(2)
        uuids = re.findall(r'"[0-9a-fA-F-]{36}"', body)
        if not uuids:
            return m.group(0)
        # rebuild with sorted uuids, two per line, fixed formatting
        indent = "\t\t\t"
        su = sorted(uuids)
        lines = [indent + " ".join(su[i:i + 2]) for i in range(0, len(su), 2)]
        return "(members\n" + "\n".join(lines) + "\n\t\t)"

    s2 = re.sub(r'\(members((?:\s+"[0-9a-fA-F-]{36}")+)(\s*\))',
                lambda m: sort_members(re.match(r'(\(members)((?:\s+"[0-9a-fA-F-]{36}")+)(\s*\))', m.group(0))),
                s)
    if s2 != s:
        path.write_text(s2)
        return True
    return False


def main() -> None:
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    if args == ["--all"]:
        root = Path(__file__).resolve().parent.parent
        args = glob.glob(str(root / "elec/layout/*/*.kicad_pcb"))
    for p in map(Path, args):
        changed = canonicalize(p)
        print(f"{p.name}: {'canonicalized' if changed else 'already canonical'}")


if __name__ == "__main__":
    main()
