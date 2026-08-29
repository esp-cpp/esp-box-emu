#!/usr/bin/env python3
"""Offline stand-in for the atopile components API.

atopile's hosted picker service (components.atopileapi.com) was taken down as
part of the 0.16 platform migration, and (as of 2026-07) no public replacement
endpoint exists for the 0.15.x CLI. Every part in this project is already
pinned (vendored parts + LCSC ids saved in the layout files by
--keep-picked-parts), so builds don't actually need anything from the hosted
service. This server answers the picker's API calls from data already in the
repo:

  - LCSC id / manufacturer / part number / package / datasheet / description
    come from the footprint properties saved in elec/layout/*/*.kicad_pcb
  - anything extra falls back to build/cache/parts/easyeda/<CID>/<CID>.json

`attributes` are returned empty: Component.attach() only aliases attributes
present in the response, so an empty dict simply leaves the design constraints
as-is (footprints/symbols still come from the local easyeda cache or the —
still-alive — easyeda API).

Usage (one-shot build):
    scripts/ato-build-offline.sh -b box-emu-base

or manually:
    python3 scripts/parts-cache-server.py --port 8471 &
    ATO_SERVICES_COMPONENTS_URL=http://127.0.0.1:8471 ato build ...

Remove once atopile publishes a CLI that talks to the new app.atopile.io
services.
"""

import argparse
import glob
import json
import os
import re
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PARTS: dict[int, dict] = {}


def load_from_layouts() -> None:
    for pcb in glob.glob(os.path.join(ROOT, "elec/layout/*/*.kicad_pcb")):
        s = open(pcb, encoding="utf-8", errors="ignore").read()
        for m in re.finditer(r'\(footprint "([^"]+)"\n(.*?)\n\t\)', s, re.S):
            body = m.group(2)

            def prop(name: str) -> str:
                pm = re.search(r'\(property "%s" "([^"]*)"' % name, body)
                return pm.group(1) if pm else ""

            lcsc = prop("LCSC")
            if not re.fullmatch(r"C\d+", lcsc):
                continue
            lcsc_n = int(lcsc[1:])
            if lcsc_n in PARTS:
                continue
            PARTS[lcsc_n] = {
                "lcsc": lcsc_n,
                "manufacturer_name": prop("Manufacturer"),
                "part_number": prop("Partnumber") or prop("Value"),
                "package": prop("package"),
                "datasheet_url": prop("Datasheet"),
                "description": prop("Description"),
                "is_basic": 0,
                "is_preferred": 0,
                "stock": 100000,
                "price": [{"qFrom": 1, "qTo": None, "price": 0.01}],
                "attributes": {},
            }


def load_from_easyeda_cache() -> None:
    for j in glob.glob(os.path.join(ROOT, "build/cache/parts/easyeda/*/C*.json")):
        cid = os.path.basename(j)
        if not re.fullmatch(r"C\d+\.json", cid):
            continue
        lcsc_n = int(cid[1:-5])
        entry = PARTS.setdefault(
            lcsc_n,
            {
                "lcsc": lcsc_n,
                "manufacturer_name": "",
                "part_number": "",
                "package": "",
                "datasheet_url": "",
                "description": "",
                "is_basic": 0,
                "is_preferred": 0,
                "stock": 100000,
                "price": [{"qFrom": 1, "qTo": None, "price": 0.01}],
                "attributes": {},
            },
        )
        try:
            d = json.load(open(j))
        except Exception:
            continue
        head = d.get("dataStr", {}).get("head", {}) if isinstance(d, dict) else {}
        cpara = head.get("c_para", {}) if isinstance(head, dict) else {}
        entry["manufacturer_name"] = entry["manufacturer_name"] or cpara.get(
            "Manufacturer", ""
        )
        entry["part_number"] = entry["part_number"] or cpara.get(
            "Manufacturer Part", ""
        )
        entry["package"] = entry["package"] or cpara.get("package", "")



# Nominal values for the passives pinned in this design, keyed by LCSC
# number: (endpoint, package, SI value). The offline server answers
# parametric picker queries only from this table, so a new value/package
# combination must be added here (the server logs unanswered queries).
PASSIVE_VALUES = {
    25744: ("resistors", "R0402", 10e3),      # 0402WGF1002TCE
    26083: ("resistors", "R0402", 1e6),       # 0402WGF1004TCE
    4109: ("resistors", "R0402", 2e3),        # 0402WGF2001TCE
    25905: ("resistors", "R0402", 5.1e3),     # 0402WGF5101TCE
    327323: ("resistors", "R0402", 91e3),     # RC0402FR-0791KL
    2909386: ("resistors", "R0402", 820e3),   # FRC0402F8203TS
    1525: ("capacitors", "C0402", 100e-9),    # CL05B104KO5NNNC
    52923: ("capacitors", "C0402", 1e-6),     # CL05A105KA5NQNC
    368809: ("capacitors", "C0402", 4.7e-6),  # CL05A475KP5NRNC
    15850: ("capacitors", "C0805", 10e-6),    # CL21A106KAYNNNE
    45783: ("capacitors", "C0805", 22e-6),    # CL21A226MAQNNNE
}


def match_parametric(q: dict) -> list[dict] | None:
    """Answer a parametric /v0/query entry from PASSIVE_VALUES, or None."""
    endpoint = q.get("endpoint")
    if endpoint not in ("resistors", "capacitors"):
        return None
    try:
        packages = {
            e["name"] for e in q["package"]["data"]["elements"]
        }
    except (KeyError, TypeError):
        packages = set()
    key = "resistance" if endpoint == "resistors" else "capacitance"
    try:
        intervals = [
            (iv["data"]["min"], iv["data"]["max"])
            for iv in q[key]["data"]["intervals"]["data"]["intervals"]
        ]
    except (KeyError, TypeError):
        return None
    out = []
    for lcsc_n, (kind, pkg, value) in PASSIVE_VALUES.items():
        if kind != endpoint or (packages and pkg not in packages):
            continue
        if not any(lo <= value <= hi for lo, hi in intervals):
            continue
        part = PARTS.get(lcsc_n)
        if part:
            out.append(part)
    return out


class Handler(BaseHTTPRequestHandler):
    def _send(self, code: int, payload: dict) -> None:
        data = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):  # quiet
        sys.stderr.write("parts-cache-server: %s\n" % (fmt % args))

    def _components_for(self, lcsc_n: int) -> list[dict]:
        part = PARTS.get(lcsc_n)
        return [part] if part else []

    def do_GET(self):
        m = re.fullmatch(r"/v0/component/lcsc/(\d+)", self.path)
        if m:
            comps = self._components_for(int(m.group(1)))
            if comps:
                return self._send(200, {"components": comps})
            return self._send(404, {"detail": f"C{m.group(1)} not in local cache"})
        m = re.fullmatch(r"/v0/component/mfr/([^/]+)/(.+)", self.path)
        if m:
            for p in PARTS.values():
                if p["part_number"] == m.group(2):
                    return self._send(200, {"components": [p]})
            return self._send(404, {"detail": "not in local cache"})
        return self._send(404, {"detail": "unknown route"})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length) or b"{}")
        if self.path == "/v0/query":
            results = []
            for q in body.get("queries", []):
                if "lcsc" in q:
                    results.append({"components": self._components_for(int(q["lcsc"]))})
                elif "part_number" in q:
                    comps = [
                        p
                        for p in PARTS.values()
                        if p["part_number"] == q["part_number"]
                    ]
                    results.append({"components": comps})
                else:
                    comps = match_parametric(q)
                    if comps is None or not comps:
                        sys.stderr.write(
                            "unanswered parametric query: %s\n" % json.dumps(q)
                        )
                    results.append({"components": comps or []})
            return self._send(200, {"results": results})
        return self._send(404, {"detail": "unknown route"})


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8471)
    ap.add_argument(
        "--host",
        default="127.0.0.1",
        help="bind address (use 0.0.0.0 in CI so the atopile docker container "
        "can reach the server via the bridge gateway)",
    )
    args = ap.parse_args()
    load_from_layouts()
    load_from_easyeda_cache()
    print(
        f"parts-cache-server: {len(PARTS)} parts loaded, "
        f"listening on {args.host}:{args.port}"
    )
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
