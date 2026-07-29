#!/usr/bin/env python3
"""Auto-place silkscreen reference designators so they are readable.

For every visible reference label, finds the nearest clear spot around its
footprint - avoiding component bodies (incl. silk graphics), pads, vias,
board-edge segments and the other labels - and normalises the text to an
upright 0/90 degree orientation on the footprint's silscreen side.

Run with KiCad's bundled python:
  /Applications/KiCad/KiCad.app/.../python3 scripts/tidy-silkscreen.py <board.kicad_pcb>

After running, re-serialize through the container build + canonicalize
pipeline before committing (see .github/workflows/atopile.yml).
"""

import sys

import pcbnew

MM = 1e6

# label body sizes to try, largest first
SIZES = [1.0, 0.8, 0.7, 0.6]
TEXT_THICKNESS = 0.15
CLEAR = 0.15  # clearance around obstacles, mm
EDGE_CLEAR = 0.4  # clearance to Edge.Cuts, mm


def text_bbox(x, y, w, h):
    return (x - w / 2, y - h / 2, x + w / 2, y + h / 2)


def label_dims(text, size):
    # rough stroke-font advance: ~0.85 * size per glyph
    w = 0.85 * size * max(len(text), 1) + 0.2
    h = size + 0.2
    return w, h


def rects_overlap(a, b, margin=0.0):
    return not (
        a[2] + margin <= b[0]
        or b[2] + margin <= a[0]
        or a[3] + margin <= b[1]
        or b[3] + margin <= a[1]
    )


def seg_rect_hit(x1, y1, x2, y2, rect, margin):
    # sample the segment; cheap and good enough for edge segments
    import math

    steps = max(2, int(math.hypot(x2 - x1, y2 - y1) / 0.3))
    rx1, ry1, rx2, ry2 = rect
    for i in range(steps + 1):
        t = i / steps
        px, py = x1 + t * (x2 - x1), y1 + t * (y2 - y1)
        if rx1 - margin <= px <= rx2 + margin and ry1 - margin <= py <= ry2 + margin:
            return True
    return False


def main(path):
    board = pcbnew.LoadBoard(path)

    front_obs = []  # rectangles (mm) on front side
    back_obs = []
    edge_segs = []  # (x1,y1,x2,y2)

    def bbox_mm(bb):
        return (
            bb.GetLeft() / MM,
            bb.GetTop() / MM,
            bb.GetRight() / MM,
            bb.GetBottom() / MM,
        )

    FRONT_LAYERS = {pcbnew.F_SilkS, pcbnew.F_Fab, pcbnew.F_Cu}
    BACK_LAYERS = {pcbnew.B_SilkS, pcbnew.B_Fab, pcbnew.B_Cu}

    for fp in board.GetFootprints():
        # pads: side-aware, THT blocks both sides
        for pad in fp.Pads():
            pb = bbox_mm(pad.GetBoundingBox())
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
        # graphics: silk/fab outlines and bare-copper artwork, item by item
        # (whole-footprint bboxes blanket the board for the outline/membrane
        # assemblies, so obstacles are collected at item granularity)
        for g in fp.GraphicalItems():
            lay = g.GetLayer()
            if lay == pcbnew.Edge_Cuts and isinstance(g, pcbnew.PCB_SHAPE):
                s, e = g.GetStart(), g.GetEnd()
                edge_segs.append((s.x / MM, s.y / MM, e.x / MM, e.y / MM))
            elif isinstance(g, pcbnew.PCB_SHAPE):
                gb = bbox_mm(g.GetBoundingBox())
                if lay in FRONT_LAYERS:
                    front_obs.append(gb)
                elif lay in BACK_LAYERS:
                    back_obs.append(gb)

    for d in board.GetDrawings():
        if d.GetLayer() == pcbnew.Edge_Cuts and isinstance(d, pcbnew.PCB_SHAPE):
            s, e = d.GetStart(), d.GetEnd()
            edge_segs.append((s.x / MM, s.y / MM, e.x / MM, e.y / MM))

    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            vb = bbox_mm(t.GetBoundingBox())
            front_obs.append(vb)
            back_obs.append(vb)

    def collides(rect, obstacles):
        for ob in obstacles:
            if rects_overlap(rect, ob, CLEAR):
                return True
        for seg in edge_segs:
            if seg_rect_hit(*seg, rect, EDGE_CLEAR):
                return True
        return False

    def pad_extent(f):
        """Bounding box of the footprint's pads (its practical body)."""
        import itertools

        boxes = [bbox_mm(p.GetBoundingBox()) for p in f.Pads()]
        gboxes = [
            bbox_mm(g.GetBoundingBox())
            for g in f.GraphicalItems()
            if isinstance(g, pcbnew.PCB_SHAPE)
            and g.GetLayer() in (pcbnew.F_Fab, pcbnew.B_Fab)
        ]
        boxes = boxes + gboxes
        if not boxes:
            bb = f.GetBoundingBox(False)
            return bbox_mm(bb)
        return (
            min(b[0] for b in boxes),
            min(b[1] for b in boxes),
            max(b[2] for b in boxes),
            max(b[3] for b in boxes),
        )

    # place labels, largest parts first so big parts grab their spot
    def area(f):
        x1, y1, x2, y2 = pad_extent(f)
        return (x2 - x1) * (y2 - y1)

    fps = sorted(board.GetFootprints(), key=lambda f: -area(f))

    moved, shrunk, unplaced = 0, 0, []
    for fp in fps:
        ref = fp.Reference()
        if not ref.IsVisible():
            continue
        is_front = fp.GetLayer() == pcbnew.F_Cu
        obstacles = front_obs if is_front else back_obs

        fx1, fy1, fx2, fy2 = pad_extent(fp)
        cx, cy = (fx1 + fx2) / 2, (fy1 + fy2) / 2
        text = fp.GetReference()

        placed = False
        for size in SIZES:
            w0, h0 = label_dims(text, size)
            cands = []
            # rings of offsets around the body: N, S, W, E then corners
            for gap in (0.25, 0.55, 0.9, 1.3, 1.8, 2.4, 3.1, 4.0, 5.0):
                cands += [
                    (cx, fy1 - gap - h0 / 2, 0),
                    (cx, fy2 + gap + h0 / 2, 0),
                    (fx1 - gap - h0 / 2, cy, 90),
                    (fx2 + gap + h0 / 2, cy, 90),
                    (fx1 - gap - w0 / 2, fy1 - gap - h0 / 2, 0),
                    (fx2 + gap + w0 / 2, fy1 - gap - h0 / 2, 0),
                    (fx1 - gap - w0 / 2, fy2 + gap + h0 / 2, 0),
                    (fx2 + gap + w0 / 2, fy2 + gap + h0 / 2, 0),
                ]
            for (px, py, rot) in cands:
                if rot == 0:
                    rect = text_bbox(px, py, w0, h0)
                else:
                    rect = text_bbox(px, py, h0, w0)
                if not collides(rect, obstacles):
                    ref.SetPosition(
                        pcbnew.VECTOR2I(int(px * MM), int(py * MM))
                    )
                    # keep text upright regardless of footprint rotation
                    ref.SetTextAngleDegrees(rot - fp.GetOrientationDegrees())
                    ref.SetTextSize(
                        pcbnew.VECTOR2I(int(size * MM), int(size * MM))
                    )
                    ref.SetTextThickness(int(TEXT_THICKNESS * MM))
                    obstacles.append(rect)
                    moved += 1
                    if size != SIZES[0]:
                        shrunk += 1
                    placed = True
                    break
            if placed:
                break
        if not placed:
            unplaced.append(text)
            # leave where it is but still register as obstacle
            tb = bbox_mm(ref.GetBoundingBox())
            obstacles.append(tb)

    pcbnew.SaveBoard(path, board)
    print(
        f"{path}: placed {moved} labels ({shrunk} shrunk), "
        f"unplaced: {unplaced if unplaced else 'none'}"
    )


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
