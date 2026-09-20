#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["cadquery-ocp>=7.7"]
# ///
"""Export ECAD footprint positions from a STEP assembly (MCAD -> positions.json).

Reads a STEP assembly exported from Fusion 360 (with the PCB present as a
component), computes each mapped occurrence's placement in the PCB's
coordinate frame, converts to KiCad's frame, and merges the result into a
positions.json consumed by place-parts.py.

Usage:
  uv run scripts/export-positions-from-step.py list <assembly.step>
      Print the occurrence tree (names, positions) - use this to fill in
      the cad_map.json "cad" fields and find the PCB occurrence name.

  uv run scripts/export-positions-from-step.py export \
      <assembly.step> <cad_map.json> <positions.json>
      Compute placements and update the matching entries in positions.json
      (other entries are left untouched; file is created if missing).

cad_map.json:
{
  "board": {
    "occurrence": "Box Electronics:1/Board:1",  // PCB occurrence (path suffix ok)
    "axes": {"x": "+Z", "y": "+X"},  // CAD directions (in the PCB occurrence
                                     // frame) of KiCad +x and +y (y is down);
                                     // any of +X -X +Y -Y +Z -Z
    "kicad_origin": [0.0, 0.0, 0.0]  // point in the PCB CAD frame that maps
                                     // to KiCad (0,0), mm
  },
  "parts": [
    {"cad": "GAMEBOY_COLOR_DPAD_SILK:U$1", "address": "dpad.membrane", "r_offset": 0},
    {"cad": "USB4110GFA:USB-C", "address": "usb_c.conn", "r_offset": 90},
    {"cad": "B_BUTTON_MEMBRANE:U$3", "address": "buttons_abxy.a_b_btns.a_btn.btn",
     "offset": [13.4, -6.2]}   // KiCad-frame mm, rotated with the part;
                               // anchors bodiless parts to a CAD neighbor
  ]
}

The board "front" is the side the viewer sees in KiCad, i.e. the -(x cross y)
direction of the configured axes. Rotation is the occurrence's rotation about
the board normal plus r_offset (calibrate r_offset once per part type against
a known-good placement). "side" overrides automatic front/back detection.
"""

import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TCollection import TCollection_AsciiString, TCollection_ExtendedString
from OCP.TDataStd import TDataStd_Name
from OCP.TDF import TDF_Label, TDF_LabelSequence
from OCP.TDocStd import TDocStd_Document
from OCP.TopLoc import TopLoc_Location
from OCP.XCAFDoc import XCAFDoc_DocumentTool

_AXES = {
    "+X": (1, 0, 0), "-X": (-1, 0, 0),
    "+Y": (0, 1, 0), "-Y": (0, -1, 0),
    "+Z": (0, 0, 1), "-Z": (0, 0, -1),
}


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


@dataclass
class Occurrence:
    path: str          # "parent/child" path of occurrence names
    depth: int
    trsf: object       # gp_Trsf, absolute placement


def _label_name(label: TDF_Label) -> str:
    attr = TDataStd_Name()
    if label.FindAttribute(TDataStd_Name.GetID_s(), attr):
        try:
            return TCollection_AsciiString(attr.Get()).ToCString()
        except Exception:
            return str(attr.Get())
    return "<unnamed>"


def load_occurrences(step_path: Path) -> list[Occurrence]:
    reader = STEPCAFControl_Reader()
    reader.SetNameMode(True)
    if reader.ReadFile(str(step_path)) != IFSelect_RetDone:
        sys.exit(f"error: failed to read {step_path}")
    doc = TDocStd_Document(TCollection_ExtendedString("doc"))
    if not reader.Transfer(doc):
        sys.exit(f"error: failed to transfer {step_path}")
    shape_tool = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())

    out: list[Occurrence] = []

    def walk(label: TDF_Label, loc: TopLoc_Location, path: str, depth: int) -> None:
        name = _label_name(label)
        # resolve references to their definitions
        ref = TDF_Label()
        target = label
        if shape_tool.GetReferredShape_s(label, ref):
            target = ref
        this_loc = loc.Multiplied(shape_tool.GetLocation_s(label))
        this_path = f"{path}/{name}" if path else name
        out.append(Occurrence(this_path, depth, this_loc.Transformation()))
        if shape_tool.IsAssembly_s(target):
            comps = TDF_LabelSequence()
            shape_tool.GetComponents_s(target, comps)
            for i in range(1, comps.Length() + 1):
                walk(comps.Value(i), this_loc, this_path, depth + 1)

    roots = TDF_LabelSequence()
    shape_tool.GetFreeShapes(roots)
    for i in range(1, roots.Length() + 1):
        walk(roots.Value(i), TopLoc_Location(), "", 0)
    return out


def _xyz(trsf) -> tuple[float, float, float]:
    t = trsf.TranslationPart()
    return (t.X(), t.Y(), t.Z())


def cmd_list(step_path: Path) -> None:
    for occ in load_occurrences(step_path):
        x, y, z = _xyz(occ.trsf)
        indent = "  " * occ.depth
        name = occ.path.rsplit("/", 1)[-1]
        print(f"{indent}{name:<50s} at ({x:9.3f}, {y:9.3f}, {z:9.3f})")
    print(
        "\n(use these names in cad_map.json; a unique path suffix like"
        ' "sub-assembly/part v1:1" is also accepted)'
    )


def _find(occurrences: list[Occurrence], name: str) -> Occurrence | None:
    exact = [o for o in occurrences if o.path == name or o.path.endswith("/" + name)]
    if len(exact) == 1:
        return exact[0]
    if len(exact) > 1:
        sys.exit(
            f"error: '{name}' is ambiguous ({len(exact)} matches):\n  "
            + "\n  ".join(o.path for o in exact)
        )
    return None


def cmd_export(step_path: Path, map_path: Path, positions_path: Path) -> None:
    spec = json.loads(map_path.read_text())
    board_cfg = spec["board"]
    axes = board_cfg.get("axes", {"x": "+X", "y": "-Y"})
    u = _AXES[axes["x"]]           # KiCad +x direction in board CAD frame
    v = _AXES[axes["y"]]           # KiCad +y (down) direction
    n = [-c for c in _cross(u, v)] # normal pointing at the viewer (front)
    origin = board_cfg.get("kicad_origin", [0.0, 0.0, 0.0])
    origin = (list(origin) + [0.0, 0.0, 0.0])[:3]

    occurrences = load_occurrences(step_path)
    board = _find(occurrences, board_cfg["occurrence"])
    if board is None:
        sys.exit(
            f"error: board occurrence '{board_cfg['occurrence']}' not found; "
            "run the `list` command to see available names"
        )
    board_inv = board.trsf.Inverted()

    if positions_path.exists():
        positions = json.loads(positions_path.read_text())
    else:
        positions = {"transform": {"dx": 0, "dy": 0, "flip_y": False}, "parts": {}}

    updated, missing = [], []
    for entry in spec.get("parts", []):
        occ = _find(occurrences, entry["cad"])
        if occ is None:
            missing.append(entry["cad"])
            continue
        rel = board_inv.Multiplied(occ.trsf)
        t = [c - o for c, o in zip(_xyz(rel), origin)]
        kx, ky = _dot(t, u), _dot(t, v)

        # part frame axes in board coordinates (rotation matrix columns)
        px = (rel.Value(1, 1), rel.Value(2, 1), rel.Value(3, 1))
        pz = (rel.Value(1, 3), rel.Value(2, 3), rel.Value(3, 3))

        # KiCad rotation is positive counterclockwise as displayed (x right,
        # y down): the part x-axis at angle r points (cos r, -sin r) in (u, v)
        kr_raw = math.degrees(math.atan2(-_dot(px, v), _dot(px, u)))
        kr = kr_raw + entry.get("r_offset", 0)

        side = entry.get("side") or ("front" if _dot(pz, n) >= 0 else "back")

        # optional offset in the anchor occurrence's frame (rotates with the
        # anchor, NOT with r_offset) - for anchoring bodiless parts to a CAD
        # neighbor such as their membrane
        offx, offy = entry.get("offset", [0.0, 0.0])
        c, sn = math.cos(math.radians(kr_raw)), math.sin(math.radians(kr_raw))
        kx += offx * c + offy * sn
        ky += -offx * sn + offy * c

        positions["parts"][entry["address"]] = {
            "x": round(kx, 4),
            "y": round(ky, 4),
            "r": round(kr % 360, 2),
            "side": side,
        }
        updated.append(f"{entry['address']:<35s} <- {entry['cad']}")

    positions["parts"] = dict(sorted(positions["parts"].items()))
    positions_path.write_text(json.dumps(positions, indent=2) + "\n")

    print(f"updated {len(updated)} entries in {positions_path}:")
    for line in updated:
        print(f"  {line}")
    if missing:
        print("not found in STEP (check names with `list`):")
        for name in missing:
            print(f"  - {name}")


def main() -> None:
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == "list":
        cmd_list(Path(args[1]))
    elif len(args) == 4 and args[0] == "export":
        cmd_export(Path(args[1]), Path(args[2]), Path(args[3]))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
