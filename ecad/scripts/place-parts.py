#!/usr/bin/env python3
"""Scripted footprint placement for atopile-generated KiCad boards.

Positions are keyed by the footprint's stable `atopile_address` property
(e.g. "usb_c.conn", "dpad.up_button.btn"), so they survive designator
renumbering and rebuilds. Intended flow: mechanical positions come from
CAD/STEP, get written into a positions JSON once, and are re-applied to
the board whenever the MCAD changes or the board is regenerated.

Usage:
  place-parts.py dump  <board.kicad_pcb>                    > positions.json
  place-parts.py apply <board.kicad_pcb> <positions.json>

positions.json:
{
  "transform": {"dx": 0, "dy": 0, "flip_y": false},   # optional, applied to
                                                      # every entry (use for
                                                      # CAD->KiCad origin/axis
                                                      # conversion; KiCad y is
                                                      # down)
  "parts": {
    "usb_c.conn":          {"x": 0.0, "y": 55.0, "r": 0,   "side": "front"},
    "uSD.card":            {"x": 20.0, "y": 40.0, "r": 90, "side": "back"},
    "dpad.up_button.btn":  {"x": -25.0, "y": 20.0, "r": 0, "side": "front"}
  }
}

Entries not present in the board are reported and skipped; footprints not
listed in the file are left untouched. Run with the python interpreter that
has atopile installed (the script re-execs itself via the `ato` entrypoint's
interpreter if faebryk isn't importable). After applying, run
`ato build -b <build>` and pull the group into the parent boards.
"""

import json
import shutil
import sys
from pathlib import Path


def _reexec_with_atopile_python() -> None:
    """Re-exec using the interpreter that runs `ato` (has faebryk installed)."""
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

from faebryk.exporters.pcb.kicad.transformer import PCB_Transformer  # noqa: E402
from faebryk.libs.kicad.fileformats import Property, kicad  # noqa: E402


def load_board(path: Path):
    return kicad.loads(kicad.pcb.PcbFile, path)


def fp_address(fp) -> str | None:
    return Property.try_get_property(fp.propertys, "atopile_address")


def fp_reference(fp) -> str | None:
    return Property.try_get_property(fp.propertys, "Reference")


def cmd_dump(board_path: Path) -> None:
    pcb_file = load_board(board_path)
    parts: dict[str, dict] = {}
    for fp in pcb_file.kicad_pcb.footprints:
        key = fp_address(fp) or fp_reference(fp)
        if not key:
            continue
        side = "back" if fp.layer.startswith("B.") else "front"
        parts[key] = {
            "x": round(fp.at.x, 4),
            "y": round(fp.at.y, 4),
            "r": round(fp.at.r or 0, 2),
            "side": side,
        }
    out = {
        "transform": {"dx": 0, "dy": 0, "flip_y": False},
        "parts": dict(sorted(parts.items())),
    }
    json.dump(out, sys.stdout, indent=2)
    print()


def cmd_apply(board_path: Path, positions_path: Path) -> None:
    spec = json.loads(positions_path.read_text())
    tf = spec.get("transform", {})
    dx, dy = tf.get("dx", 0), tf.get("dy", 0)
    flip_y = tf.get("flip_y", False)

    pcb_file = load_board(board_path)
    by_addr = {}
    for fp in pcb_file.kicad_pcb.footprints:
        for key in (fp_address(fp), fp_reference(fp)):
            if key:
                by_addr.setdefault(key, fp)

    moved, missing = 0, []
    for key, pos in spec.get("parts", {}).items():
        fp = by_addr.get(key)
        if fp is None:
            missing.append(key)
            continue
        x = pos["x"] + dx
        y = (-pos["y"] if flip_y else pos["y"]) + dy
        r = pos.get("r", 0)
        side = pos.get("side", "back" if fp.layer.startswith("B.") else "front")
        layer = "B.Cu" if side == "back" else "F.Cu"
        coord = kicad.pcb.Xyr(x=x, y=y, r=r)
        PCB_Transformer.move_fp(fp, coord, layer)
        moved += 1

    kicad.dumps(pcb_file, board_path)
    print(f"placed {moved} footprints in {board_path}")
    if missing:
        print("not found in board (skipped):")
        for key in missing:
            print(f"  - {key}")


def main() -> None:
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == "dump":
        cmd_dump(Path(args[1]))
    elif len(args) == 3 and args[0] == "apply":
        cmd_apply(Path(args[1]), Path(args[2]))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
