#!/usr/bin/env python3
"""Auto-place silkscreen reference designators neatly next to their parts.

Every visible reference label is placed immediately adjacent to its
footprint (gap capped at ~1.2 mm) in the best-scoring slot:
  - prefers above/below the part, then the sides, sliding along the edge
    to fit between neighbours,
  - avoids pads, part bodies (pad-union extents), silk/fab artwork,
    bare-copper membrane fingers, board edges and the other labels,
  - treats (tented) vias as a soft cost rather than a hard obstacle,
  - shrinks the text before moving it further away.
Afterwards, labels of parts that form rows/columns are snapped into
alignment so clusters of passives read as tidy rows.

Collision checks use the REAL text bounding box reported by KiCad.

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
CLEAR = 0.15  # clearance around hard obstacles, mm
EDGE_CLEAR = 0.4  # clearance to Edge.Cuts, mm
BODY_AREA_CAP = 200.0  # ignore pad-union "bodies" bigger than this, mm^2
GAPS = (0.12, 0.35, 0.6, 0.9, 1.2)  # distance from body edge (hard cap)
SLIDES = (0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.0, -2.0)
SIDE_COST = {"N": 0.0, "S": 0.1, "E": 0.5, "W": 0.5}
SIZE_COST = 0.9  # per size step down
VIA_COST = 0.35  # per via overlapped (tented vias: cosmetic only)
SLIDE_COST = 0.15  # per mm of lateral slide


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
    """Hard obstacles per side, via boxes, edge segments, footprint extents."""
    front_obs, back_obs, via_obs, edge_segs = [], [], [], []
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
            via_obs.append(bbox_mm(t.GetBoundingBox()))

    return front_obs, back_obs, via_obs, edge_segs, extents


def main(path):
    board = pcbnew.LoadBoard(path)
    front_obs, back_obs, via_obs, edge_segs, extents = collect(board)

    def blocked(rect, obstacles, clear=CLEAR):
        for ob in obstacles:
            if rects_overlap(rect, ob, clear):
                return True
        for seg in edge_segs:
            if seg_rect_hit(*seg, rect, EDGE_CLEAR):
                return True
        return False

    def via_hits(rect):
        return sum(1 for vb in via_obs if rects_overlap(rect, vb, 0.05))

    def area_of(ref):
        x1, y1, x2, y2 = extents[ref]
        return (x2 - x1) * (y2 - y1)

    fps = sorted(
        (f for f in board.GetFootprints() if f.Reference().IsVisible()),
        key=lambda f: -area_of(f.GetReference()),
    )

    placements = {}  # ref name -> dict for the alignment pass
    unplaced = []
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

        # measure the real bbox once per (size, rotation) at the origin;
        # the anchor is NOT necessarily the bbox centre (justification),
        # so keep the full offset box, not just half-extents
        boxes = {}
        for size in SIZES:
            ref.SetTextSize(pcbnew.VECTOR2I(int(size * MM), int(size * MM)))
            ref.SetTextThickness(int(TEXT_THICKNESS * MM))
            for rot in (0, 90):
                ref.SetTextAngleDegrees(rot - fp.GetOrientationDegrees())
                ref.SetPosition(pcbnew.VECTOR2I(0, 0))
                boxes[(size, rot)] = bbox_mm(ref.GetBoundingBox())

        def slot(size, rot, scx, scy):
            """Anchor position + rect so the bbox centre lands at (scx, scy)."""
            b = boxes[(size, rot)]
            ax = scx - (b[0] + b[2]) / 2
            ay = scy - (b[1] + b[3]) / 2
            return ax, ay, (b[0] + ax, b[1] + ay, b[2] + ax, b[3] + ay)

        def halves_of(size, rot):
            b = boxes[(size, rot)]
            return (b[2] - b[0]) / 2, (b[3] - b[1]) / 2

        best = None  # (score, ax, ay, rot, size, side, rect)
        for step, size in enumerate(SIZES):
            for gap in GAPS:
                for slide in SLIDES:
                    for side in ("N", "S", "E", "W"):
                        rot = 0 if side in ("N", "S") else 90
                        hw, hh = halves_of(size, rot)
                        if side == "N":
                            scx, scy = cx + slide, fy1 - gap - hh
                        elif side == "S":
                            scx, scy = cx + slide, fy2 + gap + hh
                        elif side == "W":
                            scx, scy = fx1 - gap - hw, cy + slide
                        else:
                            scx, scy = fx2 + gap + hw, cy + slide
                        ax, ay, rect = slot(size, rot, scx, scy)
                        score = (
                            gap
                            + SIDE_COST[side]
                            + SIZE_COST * step
                            + SLIDE_COST * abs(slide)
                            + VIA_COST * via_hits(rect)
                        )
                        if best is not None and score >= best[0]:
                            continue
                        if blocked(rect, obstacles):
                            continue
                        best = (score, ax, ay, rot, size, side, rect)

        if best is None:
            # stragglers (e.g. pullups inside the membrane button fields):
            # allow a slightly larger reach and then a relaxed clearance,
            # still choosing the nearest possible slot
            size = SIZES[-1]
            for clear in (CLEAR, 0.05):
                for gap in GAPS + (1.6, 2.2, 3.0):
                    for slide in SLIDES + (2.6, -2.6, 3.2, -3.2):
                        for side in ("N", "S", "E", "W"):
                            rot = 0 if side in ("N", "S") else 90
                            hw, hh = halves_of(size, rot)
                            if side == "N":
                                scx, scy = cx + slide, fy1 - gap - hh
                            elif side == "S":
                                scx, scy = cx + slide, fy2 + gap + hh
                            elif side == "W":
                                scx, scy = fx1 - gap - hw, cy + slide
                            else:
                                scx, scy = fx2 + gap + hw, cy + slide
                            ax, ay, rect = slot(size, rot, scx, scy)
                            if best is None and not blocked(
                                rect, obstacles, clear
                            ):
                                best = (99, ax, ay, rot, size, side, rect)
                if best is not None:
                    break

        if best is None:
            # restore the original text state and keep it as an obstacle
            ref.SetPosition(orig[0])
            ref.SetTextAngleDegrees(orig[1])
            ref.SetTextSize(orig[2])
            ref.SetTextThickness(orig[3])
            unplaced.append(fp.GetReference())
            obstacles.append(bbox_mm(ref.GetBoundingBox()))
            continue

        _, ax, ay, rot, size, side, _rect = best
        ref.SetTextSize(pcbnew.VECTOR2I(int(size * MM), int(size * MM)))
        ref.SetTextThickness(int(TEXT_THICKNESS * MM))
        ref.SetTextAngleDegrees(rot - fp.GetOrientationDegrees())
        ref.SetPosition(pcbnew.VECTOR2I(int(ax * MM), int(ay * MM)))
        rect = bbox_mm(ref.GetBoundingBox())
        obstacles.append(rect)
        placements[fp.GetReference()] = {
            "fp": fp,
            "side": side,
            "size": size,
            "rot": rot,
            "rect": rect,
            "obs": obstacles,
        }

    align(placements)

    pcbnew.SaveBoard(path, board)
    small = sum(1 for p in placements.values() if p["size"] != SIZES[0])
    print(
        f"{path}: placed {len(placements)} labels ({small} shrunk), "
        f"unplaced: {unplaced if unplaced else 'none'}"
    )


def align(placements):
    """Snap N/S labels of row-mates to a common y (and E/W mates to x)."""
    items = list(placements.values())

    def snap(axis):
        # axis 0: E/W labels share x; axis 1: N/S labels share y
        sides = ("E", "W") if axis == 0 else ("N", "S")
        pool = [p for p in items if p["side"] in sides]
        used = set()
        for i, p in enumerate(pool):
            if i in used:
                continue
            group = [p]
            pc = p["rect"]
            for j in range(i + 1, len(pool)):
                q = pool[j]
                if j in used or q["side"] != p["side"] or q["size"] != p["size"]:
                    continue
                # same row/column: near-equal coordinate on the snap axis
                if abs(
                    (q["rect"][axis] + q["rect"][axis + 2]) / 2
                    - (pc[axis] + pc[axis + 2]) / 2
                ) < 0.7:
                    group.append(q)
                    used.add(j)
            if len(group) < 2:
                continue
            coords = sorted(
                (g["rect"][axis] + g["rect"][axis + 2]) / 2 for g in group
            )
            target = coords[len(coords) // 2]
            for g in group:
                r = g["fp"].Reference()
                pos = r.GetPosition()
                cur = [pos.x / MM, pos.y / MM]
                delta = target - (g["rect"][axis] + g["rect"][axis + 2]) / 2
                if abs(delta) < 0.01:
                    continue
                new = list(cur)
                new[axis] += delta
                moved_rect = tuple(
                    v + (delta if k % 2 == axis else 0)
                    for k, v in enumerate(g["rect"])
                )
                # keep the snap only if it stays clear of everything else
                others = [ob for ob in g["obs"] if ob is not g["rect"]]
                if any(rects_overlap(moved_rect, ob, CLEAR) for ob in others):
                    continue
                r.SetPosition(
                    pcbnew.VECTOR2I(int(new[0] * MM), int(new[1] * MM))
                )
                g["obs"].remove(g["rect"])
                g["rect"] = moved_rect
                g["obs"].append(moved_rect)

    snap(1)
    snap(0)


def verify(path):
    """Report visible labels overlapping pads/artwork/other labels."""
    board = pcbnew.LoadBoard(path)
    front_obs, back_obs, _via_obs, _edges, _ext = collect(board)
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
