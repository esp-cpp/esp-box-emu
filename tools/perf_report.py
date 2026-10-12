#!/usr/bin/env python3
"""Summarise (and diff) the performance/memory lines in a serial capture.

    python tools/perf_report.py CAPTURE.log            # summary of one capture
    python tools/perf_report.py BEFORE.log AFTER.log   # side by side, with deltas

The firmware prints greppable one-liners that this script keys on:

  [mem] phase=<boot|menu|launch|quit> internal: free=.. largest=.. min_free=.. | psram: ...
  [mem] delta phase=<launch|quit> internal: free=+.. largest=+.. | psram: free=+.. largest=+..
  [<core> mem] <buffer>: <bytes> bytes -> INTERNAL|PSRAM       (per-core big-buffer placement)
  [genesis perf] fps=.. frame=..us 68k=..us vdp=..us ... | core1 snd=..us leash=..us | ...
  Statistics: / FPS: ..                                          (printed when a game quits)

A capture is split into runs at each `phase=menu` line. For each run the report
shows what the core took at launch, where its big buffers landed, the per-frame
time breakdown averaged over the periodic perf lines (the first line of a run is
dropped as warm-up), and the leak/fragmentation delta after quitting.
"""
import re
import sys
from collections import OrderedDict

KV = re.compile(r"(?<![\w.])([A-Za-z0-9_]+)=([-+]?(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?))(?:/(\d+))?")
MEM = re.compile(r"\[mem\] (delta )?phase=(\w+) (.*)")
MEM_DETAIL = re.compile(r"\[mem\] (block|region) phase=(\w+) (.*)")
MEM_TASK = re.compile(r"\[mem\] task phase=(\w+) name=(.*?) core=(-?\d+) prio=(\d+) stack_free_min=(\d+)")
PLACEMENT = re.compile(r"\[(\w+) mem\] ([^:]+): (\d+) bytes -> (INTERNAL|PSRAM)")
PERF = re.compile(r"\[(\w+) perf\] (.*)")
STATS_FPS = re.compile(r"^FPS: ([\d.]+)")


def parse_kv(text):
    """'internal: free=1 largest=2 | psram: free=3' -> {'internal.free': 1, ...}"""
    out = OrderedDict()
    section = ""
    for part in text.split("|"):
        part = part.strip()
        m = re.match(r"^(\w+)\s*:\s*(.*)$", part)
        if m and "=" not in m.group(1):
            section, part = m.group(1), m.group(2)
        else:
            section = ""
        for key, val, denom in KV.findall(part):
            name = f"{section}.{key}" if section else key
            out[name] = float(val) if "." in val else int(val, 0)
            if denom:
                out[name + "_of"] = int(denom)
    return out


def new_run(menu_kv):
    return {"mem": {"menu": menu_kv}, "delta": {}, "placement": [], "perf": [], "stats_fps": None,
            "blocks": {}, "regions": {}, "tasks": {}}


def parse(path):
    runs = []
    run = None
    boot = None
    boot_detail = new_run(None) # block/region/task lines printed at boot
    with open(path, "rb") as f:
        for raw in f:
            line = raw.decode("utf-8", "replace").strip()
            m = MEM.search(line)
            if m:
                is_delta, phase, rest = m.group(1), m.group(2), m.group(3)
                kv = parse_kv(rest)
                if phase == "boot":
                    boot = kv
                    continue
                if phase == "menu" and not is_delta:
                    run = new_run(kv)
                    runs.append(run)
                    continue
                if run is None:
                    continue
                (run["delta"] if is_delta else run["mem"])[phase] = kv
                continue
            m = MEM_TASK.search(line)
            if m:
                target = boot_detail if m.group(1) == "boot" else run
                if target is not None:
                    target["tasks"].setdefault(m.group(1), OrderedDict())[m.group(2)] = int(m.group(5))
                continue
            m = MEM_DETAIL.search(line)
            if m:
                kind, phase, rest = m.group(1), m.group(2), m.group(3)
                target = boot_detail if phase == "boot" else run
                if target is not None:
                    stack = re.search(r"stack=(.*)$", rest)
                    kv = parse_kv(rest)
                    if stack:
                        kv["stack"] = stack.group(1)
                    target[kind + "s"].setdefault(phase, []).append(kv)
                continue
            if run is None:
                continue
            m = PLACEMENT.search(line)
            if m:
                run["placement"].append((m.group(1), m.group(2).strip(), int(m.group(3)), m.group(4)))
                continue
            m = PERF.search(line)
            if m:
                kv = parse_kv(m.group(2))
                kv["_core"] = m.group(1)
                run["perf"].append(kv)
                continue
            m = STATS_FPS.search(line)
            if m:
                run["stats_fps"] = float(m.group(1))
    return (boot, boot_detail), runs


def mean(values):
    values = [v for v in values if v is not None]
    return sum(values) / len(values) if values else None


def perf_summary(perf_lines):
    """Average every numeric field over the perf lines (dropping the first as warm-up)."""
    lines = perf_lines[1:] if len(perf_lines) > 1 else perf_lines
    if not lines:
        return {}
    keys = [k for k in lines[0] if not k.startswith("_")]
    return OrderedDict((k, mean([l.get(k) for l in lines])) for k in keys)


def fmt(v):
    if v is None:
        return "-"
    if isinstance(v, float):
        return f"{v:.1f}" if abs(v) < 1000 else f"{v:,.0f}"
    return f"{v:,}"


def fmt_delta(a, b):
    if a is None or b is None:
        return ""
    d = b - a
    if isinstance(a, float) or isinstance(b, float):
        pct = f" ({100.0 * d / a:+.1f}%)" if a else ""
        return f"{d:+.1f}{pct}"
    return f"{d:+,}"


def run_rows(run):
    """Flatten one run into an ordered list of (label, value) rows."""
    rows = []
    for phase in ("menu", "launch", "quit"):
        kv = run["mem"].get(phase)
        if not kv:
            continue
        for key in ("internal.free", "internal.largest", "psram.free", "psram.largest"):
            rows.append((f"{phase} {key}", kv.get(key)))
    for phase in ("launch", "quit"):
        kv = run["delta"].get(phase)
        if not kv:
            continue
        for key in ("internal.free", "internal.largest", "psram.free", "psram.largest"):
            rows.append((f"{phase} delta {key}", kv.get(key)))
    summary = perf_summary(run["perf"])
    core = run["perf"][0]["_core"] if run["perf"] else None
    for key, val in summary.items():
        rows.append((f"{core} perf {key}", val))
    if run["perf"]:
        rows.append((f"{core} perf lines", len(run["perf"])))
    rows.append(("statistics FPS (unthrottled)", run["stats_fps"]))
    for region in run["regions"].get("launch", []):
        start = region.get("start")
        rows.append((f"launch region {start:#x} largest", region.get("largest")))
    # stack high-water marks after the full play session (quit) are what
    # right-sizing a stack needs; launch marks are a sanity check
    for name, free_min in run["tasks"].get("quit", {}).items():
        rows.append((f"quit stack_free_min {name}", free_min))
    return rows


def print_detail(detail, phase, indent="  "):
    """Blocks, regions and tasks recorded for one phase (boot or launch)."""
    blocks = detail["blocks"].get(phase, [])
    if blocks:
        print(f"{indent}{phase} internal blocks:")
        for b in sorted(blocks, key=lambda b: -b.get("size", 0)):
            owner = f"  stack={b['stack']}" if "stack" in b else ""
            print(f"{indent}  {b.get('size', 0):>8,} B at {b.get('addr', 0):#x}{owner}")
    regions = detail["regions"].get(phase, [])
    if regions:
        print(f"{indent}{phase} internal heap regions:")
        for r in regions:
            print(f"{indent}  {r.get('start', 0):#x} size={r.get('size', 0):>8,} used={r.get('used', 0):>8,}"
                  f" ({r.get('used_blocks', 0)} blocks) free={r.get('free', 0):>8,} largest={r.get('largest', 0):>8,}")
    tasks = detail["tasks"].get(phase, {})
    if tasks:
        print(f"{indent}{phase} task stack_free_min:")
        for name, free_min in sorted(tasks.items(), key=lambda kv: -kv[1]):
            print(f"{indent}  {name:<18} {free_min:>8,}")


def print_single(path, boot_info, runs):
    boot, boot_detail = boot_info
    print(f"== {path}")
    if boot:
        print(f"boot: internal free={fmt(boot.get('internal.free'))} largest={fmt(boot.get('internal.largest'))}"
              f" | psram free={fmt(boot.get('psram.free'))} largest={fmt(boot.get('psram.largest'))}")
    print_detail(boot_detail, "boot")
    if not runs:
        print("no runs found (no `[mem] phase=menu` line)")
        return
    for i, run in enumerate(runs, 1):
        print(f"\n-- run {i}")
        if run["placement"]:
            print("  placement:")
            for core, name, size, where in run["placement"]:
                print(f"    {core:<10} {name:<24} {size:>8,} B  {where}")
        print_detail(run, "launch")
        width = max(len(label) for label, _ in run_rows(run))
        for label, value in run_rows(run):
            print(f"  {label:<{width}}  {fmt(value):>14}")


def print_diff(a_path, a_runs, b_path, b_runs):
    """Compare run N of A against run N of B."""
    n = min(len(a_runs), len(b_runs))
    if n == 0:
        print("one of the captures has no runs")
        return
    for i in range(n):
        a, b = a_runs[i], b_runs[i]
        a_rows, b_rows = dict(run_rows(a)), dict(run_rows(b))
        labels = [l for l, _ in run_rows(a)] + [l for l, _ in run_rows(b) if l not in a_rows]
        width = max(len(l) for l in labels)
        print(f"\n-- run {i + 1}: A={a_path}  B={b_path}")
        print(f"  {'':<{width}}  {'A':>14} {'B':>14}  delta (B-A)")
        for label in labels:
            av, bv = a_rows.get(label), b_rows.get(label)
            print(f"  {label:<{width}}  {fmt(av):>14} {fmt(bv):>14}  {fmt_delta(av, bv)}")
        a_place = {(c, nme): w for c, nme, _, w in a["placement"]}
        b_place = {(c, nme): w for c, nme, _, w in b["placement"]}
        moved = [(k, a_place[k], b_place[k]) for k in a_place if k in b_place and a_place[k] != b_place[k]]
        if moved:
            print("  placement changes:")
            for (core, name), wa, wb in moved:
                print(f"    {core} {name}: {wa} -> {wb}")


def main(argv):
    if len(argv) not in (2, 3) or argv[1] in ("-h", "--help"):
        print(__doc__)
        return 2
    boot_a, runs_a = parse(argv[1])
    if len(argv) == 2:
        print_single(argv[1], boot_a, runs_a)
        return 0
    boot_b, runs_b = parse(argv[2])
    print_single(argv[1], boot_a, runs_a)
    print()
    print_single(argv[2], boot_b, runs_b)
    print_diff(argv[1], runs_a, argv[2], runs_b)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
