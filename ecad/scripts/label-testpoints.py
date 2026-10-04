#!/usr/bin/env python3
"""Label test points on the silkscreen with what they carry.

A bare "TPn" designator means nothing to someone holding a probe, so for
every footprint whose reference starts with "TP" this hides the
designator and places a board-level silk text naming the net it exposes
(VBAT, GND, 5V, EN, ...), in the nearest free slot beside the pad.

Two phases, so the constrained designators (a 0402 has little room) are
placed first and the test-point labels — a lone pad with four open sides
— fit around them:

  label-testpoints.py --prepare <board>   # hide TP designators + drop old labels
  tidy-silkscreen.py <board>              # place every other designator
  label-testpoints.py <board>             # fit the net labels around them

Old labels are found by content and proximity (a silk text reading like
one of ours near a test point), so the pass is repeatable and does not
depend on metadata the atopile PCB-update step might not preserve.

Self-contained (no import of tidy-silkscreen) to avoid a KiCad SWIG
downcast-registry flake seen when its module is exec'd via importlib.
Run with KiCad's bundled python.
"""

import math
import sys

import pcbnew

MM = 1e6
SIZE = 0.8
THICK = 0.15
CLEAR = 0.15
EDGE_CLEAR = 0.4
NEAR_MM = 6.0
GAPS = (0.3, 0.5, 0.8, 1.1, 1.5, 2.0)
SLIDES = (0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.0, -2.0)
SIDE_COST = {"N": 0.0, "S": 0.1, "E": 0.5, "W": 0.5}
SLIDE_COST = 0.15

NET_LABELS = {
    "vbat": "VBAT",
    "gnd": "GND",
    "vcc_5v": "5V",
    "vcc_3v3": "3V3",
    "boost_en": "EN",
    "vbus": "VBUS",
    "battery_measurement": "VSNS",
}


def bbox_mm(bb):
    return (bb.GetLeft() / MM, bb.GetTop() / MM, bb.GetRight() / MM, bb.GetBottom() / MM)


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


def testpoints(board):
    return [fp for fp in board.GetFootprints() if fp.GetReference().startswith("TP")]


def label_texts(board):
    return [
        d
        for d in board.GetDrawings()
        if isinstance(d, pcbnew.PCB_TEXT)
        and d.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS)
        and d.GetText() in NET_LABELS.values()
    ]


def old_labels_near(board):
    """Our previous labels: a label-like silk text within reach of a TP."""
    tp_pos = [fp.GetPosition() for fp in testpoints(board)]
    out = []
    for d in label_texts(board):
        p = d.GetPosition()
        if any(math.hypot(p.x - q.x, p.y - q.y) / MM <= NEAR_MM for q in tp_pos):
            out.append(d)
    return out


def hide_ref(fp):
    for f in fp.GetFields():
        if f.GetName() == "Reference":
            f.SetVisible(False)
            return
    fp.Reference().SetVisible(False)


def obstacles_and_edges(board):
    """Silk / copper / body obstacles per side, plus board-edge segments."""
    front, back, edges = [], [], []
    FRONT = {pcbnew.F_SilkS, pcbnew.F_Fab, pcbnew.F_Cu}
    BACK = {pcbnew.B_SilkS, pcbnew.B_Fab, pcbnew.B_Cu}

    for fp in board.GetFootprints():
        pad_boxes = []
        for pad in fp.Pads():
            pb = bbox_mm(pad.GetBoundingBox())
            pad_boxes.append(pb)
            if pad.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH):
                front.append(pb)
                back.append(pb)
            elif pad.IsOnLayer(pcbnew.F_Cu):
                front.append(pb)
            elif pad.IsOnLayer(pcbnew.B_Cu):
                back.append(pb)
        for g in fp.GraphicalItems():
            lay = g.GetLayer()
            if lay == pcbnew.Edge_Cuts and isinstance(g, pcbnew.PCB_SHAPE):
                s, e = g.GetStart(), g.GetEnd()
                edges.append((s.x / MM, s.y / MM, e.x / MM, e.y / MM))
            elif isinstance(g, pcbnew.PCB_SHAPE):
                gb = bbox_mm(g.GetBoundingBox())
                if (gb[2] - gb[0]) * (gb[3] - gb[1]) > 200.0:
                    continue
                (front if lay in FRONT else back if lay in BACK else []).append(gb)
        # visible designator that tidy-silkscreen already placed
        ref = fp.Reference()
        if ref.IsVisible():
            (front if fp.GetLayer() == pcbnew.F_Cu else back).append(
                bbox_mm(ref.GetBoundingBox())
            )

    for d in board.GetDrawings():
        lay = d.GetLayer()
        if lay == pcbnew.Edge_Cuts and isinstance(d, pcbnew.PCB_SHAPE):
            s, e = d.GetStart(), d.GetEnd()
            edges.append((s.x / MM, s.y / MM, e.x / MM, e.y / MM))
        elif isinstance(d, (pcbnew.PCB_SHAPE, pcbnew.PCB_TEXT)):
            db = bbox_mm(d.GetBoundingBox())
            (front if lay in (pcbnew.F_SilkS, pcbnew.F_Cu) else back).append(db)

    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            (front if True else back).append(bbox_mm(t.GetBoundingBox()))
            back.append(bbox_mm(t.GetBoundingBox()))
    return front, back, edges


def prepare(path):
    board = pcbnew.LoadBoard(path)
    tps = testpoints(board)
    for fp in tps:  # hide first, before any Remove invalidates wrappers
        hide_ref(fp)
    for d in old_labels_near(board):
        board.Remove(d)
    pcbnew.SaveBoard(path, board)
    print(f"{path}: prepared {len(tps)} test points")


def label(path):
    board = pcbnew.LoadBoard(path)
    for d in old_labels_near(board):
        board.Remove(d)
    front_obs, back_obs, edges = obstacles_and_edges(board)

    def blocked(rect, obstacles, clear=CLEAR):
        return any(rects_overlap(rect, ob, clear) for ob in obstacles) or any(
            seg_rect_hit(*seg, rect, EDGE_CLEAR) for seg in edges
        )

    def search(pb, cx, cy, hw, hh, obstacles, gaps, slides, clear):
        best = None
        for gap in gaps:
            for slide in slides:
                for side in ("N", "S", "E", "W"):
                    if side == "N":
                        scx, scy = cx + slide, pb[1] - gap - hh
                    elif side == "S":
                        scx, scy = cx + slide, pb[3] + gap + hh
                    elif side == "W":
                        scx, scy = pb[0] - gap - hw, cy + slide
                    else:
                        scx, scy = pb[2] + gap + hw, cy + slide
                    rect = (scx - hw, scy - hh, scx + hw, scy + hh)
                    score = gap + SIDE_COST[side] + SLIDE_COST * abs(slide)
                    if best is not None and score >= best[0]:
                        continue
                    if blocked(rect, obstacles, clear):
                        continue
                    best = (score, scx, scy, rect)
            if best is not None and clear == CLEAR:
                break
        return best

    placed, failed = [], []
    for fp in testpoints(board):
        pads = list(fp.Pads())
        net = pads[0].GetNetname()
        text = NET_LABELS.get(net, net.upper())
        front = fp.GetLayer() == pcbnew.F_Cu
        obstacles = front_obs if front else back_obs
        pb = bbox_mm(pads[0].GetBoundingBox())
        cx, cy = (pb[0] + pb[2]) / 2, (pb[1] + pb[3]) / 2

        txt = pcbnew.PCB_TEXT(board)
        txt.SetText(text)
        txt.SetLayer(pcbnew.F_SilkS if front else pcbnew.B_SilkS)
        txt.SetTextSize(pcbnew.VECTOR2I(int(SIZE * MM), int(SIZE * MM)))
        txt.SetTextThickness(int(THICK * MM))
        txt.SetMirrored(not front)
        txt.SetPosition(pcbnew.VECTOR2I(0, 0))
        b = bbox_mm(txt.GetBoundingBox())
        hw, hh = (b[2] - b[0]) / 2, (b[3] - b[1]) / 2
        ax_off, ay_off = (b[0] + b[2]) / 2, (b[1] + b[3]) / 2

        best = search(pb, cx, cy, hw, hh, obstacles, GAPS, SLIDES, CLEAR)
        if best is None:
            # crowded test point (e.g. between two 0402s): reach further,
            # then relax the clearance, still taking the nearest slot
            wide_gaps = GAPS + (2.6, 3.2, 4.0)
            wide_slides = SLIDES + (2.6, -2.6, 3.2, -3.2)
            for clear in (CLEAR, 0.05):
                best = search(pb, cx, cy, hw, hh, obstacles, wide_gaps, wide_slides, clear)
                if best is not None:
                    break
        if best is None:
            failed.append(fp.GetReference())
            continue
        _, scx, scy, rect = best
        txt.SetPosition(pcbnew.VECTOR2I(int((scx - ax_off) * MM), int((scy - ay_off) * MM)))
        board.Add(txt)
        obstacles.append(rect)
        placed.append(f"{fp.GetReference()}={text}")

    pcbnew.SaveBoard(path, board)
    print(f"{path}: labelled {', '.join(placed)}; failed: {failed if failed else 'none'}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--prepare"]
    for p in args:
        (prepare if "--prepare" in sys.argv else label)(p)
