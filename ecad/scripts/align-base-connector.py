#!/usr/bin/env python3
"""Align the `connector` group to the `box` (base) group in the top-level
boards so the two board-outline pieces meet exactly.

The required relative offset between the two outline footprint anchors was
calibrated by fitting both outlines' Edge.Cuts vertices against the Eagle
board outlines (all fits 0.0000 mm rms, 2026-07-10):

  outline fit:  kicad = (eagle_x + D, C - eagle_y)
    BOX_EMU_BASE vs v36 & v11:  D = -51.9400  C = 67.5399
    BOX_EMU      vs v36:        D = -52.0200  C = 75.0799
    BOX_3_EMU    vs v11:        D = -50.4579  C = 70.0000

  connector_anchor - base_anchor = (D_base - D_conn, C_base - C_conn)

Run after pulling the group layouts (sync-base-layout.sh does both).
Translates every footprint of the connector module plus all track/graphic
members of the `connector` KiCad group.
"""

import shutil
import sys
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

ECAD = Path(__file__).resolve().parent.parent

# (board, connector_anchor - base_anchor)
ALIGNMENTS = {
    "box-emu": (0.0800, -7.5400),      # base(v36) vs BOX_EMU(v36)
    "box-3-emu": (-1.4821, -2.4601),   # base(v11) vs BOX_3_EMU(v11)
}


def translate(obj, dx: float, dy: float) -> None:
    """Translate a PCB object in place."""
    for attr in ("at", "start", "end", "mid", "center"):
        pt = getattr(obj, attr, None)
        if pt is not None and hasattr(pt, "x"):
            pt.x += dx
            pt.y += dy
    # zone / poly points
    for attr in ("polygon", "filled_polygon"):
        for poly in getattr(obj, attr, None) or []:
            for pt in getattr(poly, "pts", None) and poly.pts.xys or []:
                pt.x += dx
                pt.y += dy


def _outline_verts(kicad_mod: Path) -> list[tuple[float, float]]:
    """Edge.Cuts endpoint vertices of an outline footprint, local coords."""
    import re

    s = kicad_mod.read_text()
    verts = []
    for m in re.finditer(r"\(fp_(?:line|arc)(.*?)\(layer \"Edge.Cuts\"\)", s, re.S):
        for a, b in re.findall(r"\((?:start|end) ([-\d.]+) ([-\d.]+)\)", m.group(1)):
            verts.append((float(a), float(b)))
    return verts


_BASE_VERTS = _outline_verts(
    ECAD / "elec/src/parts/BOX_EMU_BASE_OUTLINE/BOX_EMU_BASE.kicad_mod")
_CONN_VERTS = {
    "box-emu": _outline_verts(
        ECAD / "elec/src/parts/BOX_EMU_OUTLINE/BOX_EMU.kicad_mod"),
    "box-3-emu": _outline_verts(
        ECAD / "elec/src/parts/BOX_3_EMU_OUTLINE/BOX_3_EMU.kicad_mod"),
}


def _verify_closure(board_name: str, base_at, conn_at) -> None:
    """The two outline pieces must share exactly two junction vertices."""
    import math

    bpts = [(base_at.x + x, base_at.y + y) for x, y in _BASE_VERTS]
    cpts = [(conn_at.x + x, conn_at.y + y) for x, y in _CONN_VERTS[board_name]]
    hits = [
        (bp, min(math.hypot(bp[0] - cp[0], bp[1] - cp[1]) for cp in cpts))
        for bp in bpts
    ]
    joined = [h for h in hits if h[1] < 0.01]
    if len(joined) >= 2:
        worst = max(d for _, d in joined)
        print(f"{board_name}: outline closed - {len(joined)} junction "
              f"vertices coincide (worst {worst * 1000:.1f} um)")
    else:
        sys.exit(f"{board_name}: OUTLINE NOT CLOSED - junction vertices do "
                 f"not meet (nearest {min(d for _, d in hits):.3f} mm). "
                 "Check the outline footprints / calibration constants.")


def align(board_name: str, want: tuple[float, float]) -> None:
    path = ECAD / f"elec/layout/{board_name}/{board_name}.kicad_pcb"

    lock = path.parent / f"~{path.name}.lck"
    if lock.exists():
        sys.exit(
            f"{board_name}: {lock.name} exists - the board appears to be "
            "open in KiCad. Close it first (a KiCad save would silently "
            "overwrite this script's changes), or delete the stale lock."
        )

    pcb_file = kicad.loads(kicad.pcb.PcbFile, path)
    pcb = pcb_file.kicad_pcb

    def addr(fp):
        return Property.try_get_property(fp.propertys, "atopile_address")

    base = next((fp for fp in pcb.footprints if addr(fp) == "box.outline"), None)
    conn = next((fp for fp in pcb.footprints if addr(fp) == "connector.outline"), None)
    if base is None or conn is None:
        sys.exit(f"{board_name}: outline footprints not found (build first)")

    if (base.at.r or 0) != 0 or (conn.at.r or 0) != 0:
        sys.exit(f"{board_name}: outline footprints are rotated "
                 f"(base r={base.at.r}, conn r={conn.at.r}); this script "
                 "only handles unrotated groups - straighten them first")

    dx = (base.at.x + want[0]) - conn.at.x
    dy = (base.at.y + want[1]) - conn.at.y
    if abs(dx) < 1e-4 and abs(dy) < 1e-4:
        print(f"{board_name}: connector group already aligned")
        _verify_closure(board_name, base.at, conn.at)
        return

    # move all footprints of the connector module
    moved = 0
    member_uuids = set()
    for group in pcb.groups:
        if group.name == "connector":
            member_uuids.update(group.members)
    for fp in pcb.footprints:
        a = addr(fp)
        if (a and a.startswith("connector.")) or fp.uuid in member_uuids:
            translate(fp, dx, dy)
            moved += 1
    # move group-member tracks/vias/graphics (from pulled child layouts)
    for coll in (pcb.segments, pcb.vias, pcb.arcs, pcb.zones,
                 pcb.gr_lines, pcb.gr_arcs, pcb.gr_circles, pcb.gr_texts,
                 pcb.gr_rects, pcb.gr_polys):
        for obj in coll or []:
            if getattr(obj, "uuid", None) in member_uuids:
                translate(obj, dx, dy)

    kicad.dumps(pcb_file, path)
    print(f"{board_name}: moved connector group by ({dx:+.4f}, {dy:+.4f}) "
          f"({moved} footprints)")
    _verify_closure(board_name, base.at, conn.at)


def main() -> None:
    names = sys.argv[1:] or list(ALIGNMENTS)
    for name in names:
        align(name, ALIGNMENTS[name])


if __name__ == "__main__":
    main()
