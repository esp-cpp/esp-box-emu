#!/usr/bin/env python3
"""Auto-place silkscreen reference designators so they are readable.

For every visible reference label, finds the nearest clear spot around its
footprint and moves the label there, avoiding:
  - pads (both sides for THT) and part bodies (pad-union extents),
  - silk / fab artwork and bare-copper membrane fingers (item by item),
  - vias, board-edge segments, and the other labels.
Text is normalised to an upright 0/90 orientation and consistent sizes.

Collision checks use the REAL text bounding box reported by KiCad (the text
is temporarily placed at each candidate), not an estimate.

Run with KiCad's bundled python:
  .../python3 scripts/tidy-silkscreen.py <board.kicad_pcb> [...]
  .../python3 scripts/tidy-silkscreen.py --verify <board.kicad_pcb> [...]

After running, re-serialize through the container build + canonicalize
pipeline before committing (see .github/workflows/atopile.yml).
"""

import math
import sys

import pcbnew

MM = 1e6

SIZES = [1.0, 0.8, 0.7, 0.6]  # label sizes to try, largest first
TEXT_THICKNESS = 0.15
CLEAR = 0.2  # clearance around obstacles, mm
EDGE_CLEAR = 0.4  # clearance to Edge.Cuts, mm
BODY_AREA_CAP = 200.0  # ignore pad-union "bodies" bigger than this, mm^2
GAPS = (0.15, 0.4, 0.7, 1.0, 1.4, 1.9, 2.5, 3.2, 4.0, 5.0, 6.2, 7.5)


def bbox_mm(bb):
    return (
        bb.GetLeft() / MM,
        bb.GetTop() / MM,
        bb.GetRight() / MM,
        bb.GetBottom() / MM,
    )


def rects_overlap(a, b, margin=0.0):
    return not (
        a[2] + margin <= b[0]
        or b[2] + margin <= a[0]
        or a[3] + margin <= b[1]
        or b[3] + margin <= a[1]
    )


def seg_rect_hit(x1, y1, x2, y2, rect, margin):
    steps = max(2, int(math.hypot(x2 - x1, y2 - y1) / 0.3))
    rx1, ry1, rx2, ry2 = rect
    for i in range(steps + 1):
        t = i / steps
        px, py = x1 + t * (x2 - x1), y1 + t * (y2 - y1)
        if rx1 - margin <= px <= rx2 + margin and ry1 - margin <= py <= ry2 + margin:
            return True
    return False


def union(boxes):
    return (
        min(b[0] for b in boxes),
        min(b[1] for b in boxes),
        max(b[2] for b in boxes),
        max(b[3] for b in boxes),
    )


def collect(board):
    """Obstacle rectangles per side + edge segments + per-footprint extents."""
    front_obs, back_obs, edge_segs = [], [], []
    extents = {}

    FRONT_LAYERS = {pcbnew.F_SilkS, pcbnew.F_Fab, pcbnew.F_Cu}
    BACK_LAYERS = {pcbnew.B_SilkS, pcbnew.B_Fab, pcbnew.B_Cu}

    for fp in board.GetFootprints():
        pad_boxes = []
        for pad in fp.Pads():
            pb = bbox_mm(pad.GetBoundingBox())
            pad_boxes.append(pb)
            if pad.GetAttribute() in (
                pcbnew.PAD_ATTRIB_PTH,
                pcbnew.PAD_ATTRIB_NPTH,
            ):
                front_obs.append(pb)
                back_obs.append(pb)
            elif pad.IsOnLayer(pcbnew.F_Cu):
                front_obs.append(pb)
            elif pad.IsOnLayer(pcbnew.B_Cu):
                back_obs.append(pb)

        gfx_boxes = []
        for g in fp.GraphicalItems():
            lay = g.GetLayer()
            if lay == pcbnew.Edge_Cuts and isinstance(g, pcbnew.PCB_SHAPE):
                s, e = g.GetStart(), g.GetEnd()
                edge_segs.append((s.x / MM, s.y / MM, e.x / MM, e.y / MM))
            elif isinstance(g, pcbnew.PCB_SHAPE):
                gb = bbox_mm(g.GetBoundingBox())
                # giant outline polylines (membrane blobs) blanket real
                # estate: only their strokes matter, so cap by bbox area
                if (gb[2] - gb[0]) * (gb[3] - gb[1]) > BODY_AREA_CAP:
                    continue
                if lay in FRONT_LAYERS:
                    front_obs.append(gb)
                    gfx_boxes.append(gb)
                elif lay in BACK_LAYERS:
                    back_obs.append(gb)
                    gfx_boxes.append(gb)

        # part body: pad-union extent (covers the package between the pads);
        # skip board-sized unions (outline / membrane assemblies)
        if pad_boxes:
            u = union(pad_boxes)
            if (u[2] - u[0]) * (u[3] - u[1]) <= BODY_AREA_CAP:
                (front_obs if fp.GetLayer() == pcbnew.F_Cu else back_obs).append(u)

        # ring base for this footprint's own label: body + own artwork
        boxes = pad_boxes + gfx_boxes
        boxes = [
            b for b in boxes if (b[2] - b[0]) * (b[3] - b[1]) <= BODY_AREA_CAP
        ] or pad_boxes
        extents[fp.GetReference()] = (
            union(boxes) if boxes else bbox_mm(fp.GetBoundingBox(False))
        )

    for d in board.GetDrawings():
        lay = d.GetLayer()
        if lay == pcbnew.Edge_Cuts and isinstance(d, pcbnew.PCB_SHAPE):
            s, e = d.GetStart(), d.GetEnd()
            edge_segs.append((s.x / MM, s.y / MM, e.x / MM, e.y / MM))
        elif isinstance(d, pcbnew.PCB_SHAPE):
            db = bbox_mm(d.GetBoundingBox())
            if lay in (pcbnew.F_SilkS, pcbnew.F_Cu):
                front_obs.append(db)
            elif lay in (pcbnew.B_SilkS, pcbnew.B_Cu):
                back_obs.append(db)

    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            vb = bbox_mm(t.GetBoundingBox())
            front_obs.append(vb)
            back_obs.append(vb)

    return front_obs, back_obs, edge_segs, extents


def main(path):
    board = pcbnew.LoadBoard(path)
    front_obs, back_obs, edge_segs, extents = collect(board)

    def collides(rect, obstacles):
        for ob in obstacles:
            if rects_overlap(rect, ob, CLEAR):
                return True
        for seg in edge_segs:
            if seg_rect_hit(*seg, rect, EDGE_CLEAR):
                return True
        return False

    def area_of(ref):
        x1, y1, x2, y2 = extents[ref]
        return (x2 - x1) * (y2 - y1)

    fps = sorted(
        (f for f in board.GetFootprints() if f.Reference().IsVisible()),
        key=lambda f: -area_of(f.GetReference()),
    )

    moved, shrunk, unplaced = 0, 0, []
    for fp in fps:
        ref = fp.Reference()
        obstacles = front_obs if fp.GetLayer() == pcbnew.F_Cu else back_obs
        fx1, fy1, fx2, fy2 = extents[fp.GetReference()]
        cx, cy = (fx1 + fx2) / 2, (fy1 + fy2) / 2

        orig = (
            ref.GetPosition(),
            ref.GetTextAngleDegrees(),
            ref.GetTextSize(),
            ref.GetTextThickness(),
        )

        placed = False
        for size in SIZES:
            ref.SetTextSize(pcbnew.VECTOR2I(int(size * MM), int(size * MM)))
            ref.SetTextThickness(int(TEXT_THICKNESS * MM))
            # measure the real half-extent for this size at 0 and 90 degrees
            half = {}
            for rot in (0, 90):
                ref.SetTextAngleDegrees(rot - fp.GetOrientationDegrees())
                ref.SetPosition(pcbnew.VECTOR2I(0, 0))
                b = bbox_mm(ref.GetBoundingBox())
                half[rot] = ((b[2] - b[0]) / 2, (b[3] - b[1]) / 2)

            hw0, hh0 = half[0]
            hw9, hh9 = half[90]
            cands = []
            for gap in GAPS:
                cands += [
                    (cx, fy1 - gap - hh0, 0),
                    (cx, fy2 + gap + hh0, 0),
                    (fx1 - gap - hw9, cy, 90),
                    (fx2 + gap + hw9, cy, 90),
                    (fx1 - gap - hw0, fy1 - gap - hh0, 0),
                    (fx2 + gap + hw0, fy1 - gap - hh0, 0),
                    (fx1 - gap - hw0, fy2 + gap + hh0, 0),
                    (fx2 + gap + hw0, fy2 + gap + hh0, 0),
                ]
            for (px, py, rot) in cands:
                ref.SetTextAngleDegrees(rot - fp.GetOrientationDegrees())
                ref.SetPosition(pcbnew.VECTOR2I(int(px * MM), int(py * MM)))
                rect = bbox_mm(ref.GetBoundingBox())
                if not collides(rect, obstacles):
                    obstacles.append(rect)
                    moved += 1
                    if size != SIZES[0]:
                        shrunk += 1
                    placed = True
                    break
            if placed:
                break

        if not placed:
            # last resort: smallest size, half clearance
            size = SIZES[-1]
            ref.SetTextSize(pcbnew.VECTOR2I(int(size * MM), int(size * MM)))
            ref.SetTextThickness(int(TEXT_THICKNESS * MM))
            for (px, py, rot) in cands:
                ref.SetTextAngleDegrees(rot - fp.GetOrientationDegrees())
                ref.SetPosition(pcbnew.VECTOR2I(int(px * MM), int(py * MM)))
                rect = bbox_mm(ref.GetBoundingBox())
                hit = any(rects_overlap(rect, ob, CLEAR / 2) for ob in obstacles)
                hit = hit or any(
                    seg_rect_hit(*seg, rect, EDGE_CLEAR) for seg in edge_segs
                )
                if not hit:
                    obstacles.append(rect)
                    moved += 1
                    shrunk += 1
                    placed = True
                    break

        if not placed:
            # restore and register current bbox as an obstacle
            ref.SetPosition(orig[0])
            ref.SetTextAngleDegrees(orig[1])
            ref.SetTextSize(orig[2])
            ref.SetTextThickness(orig[3])
            unplaced.append(fp.GetReference())
            obstacles.append(bbox_mm(ref.GetBoundingBox()))

    pcbnew.SaveBoard(path, board)
    print(
        f"{path}: placed {moved} labels ({shrunk} shrunk), "
        f"unplaced: {unplaced if unplaced else 'none'}"
    )


def verify(path):
    """Report visible labels overlapping pads/artwork/other labels."""
    board = pcbnew.LoadBoard(path)
    front_obs, back_obs, _, _ = collect(board)
    labels = []
    for fp in board.GetFootprints():
        ref = fp.Reference()
        if not ref.IsVisible():
            continue
        labels.append(
            (
                fp.GetReference(),
                bbox_mm(ref.GetBoundingBox()),
                fp.GetLayer() == pcbnew.F_Cu,
            )
        )
    bad = []
    for i, (name, rect, front) in enumerate(labels):
        for ob in front_obs if front else back_obs:
            if rects_overlap(rect, ob, 0.0):
                bad.append((name, "copper/artwork"))
                break
        for j in range(i + 1, len(labels)):
            n2, r2, f2 = labels[j]
            if f2 == front and rects_overlap(rect, r2, 0.0):
                bad.append((f"{name}/{n2}", "label-label"))
    print(f"{path}: verify -> {bad if bad else 'clean'}")
    return bad


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--verify"]
    for p in args:
        (verify if "--verify" in sys.argv else main)(p)
