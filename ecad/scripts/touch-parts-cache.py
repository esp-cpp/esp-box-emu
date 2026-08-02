#!/usr/bin/env python3
"""Freshen the EasyEDA part-cache timestamps.

atopile re-queries the EasyEDA API for any cached part whose
`atopile_queried_at` is older than one day (PartLifecycle.EasyEDA_API.
DELTA_REFRESH) and raises a hard PickError when the API is unreachable,
even though the CAD data is sitting in the cache. Bumping the timestamps
makes `shall_refresh()` return False so builds run fully offline.

Run before `ato build` whenever the design adds new (unpicked) component
instances; harmless otherwise.
"""

import glob
import json
import os
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main() -> None:
    now = time.time()
    touched = 0
    for path in glob.glob(
        os.path.join(ROOT, "build/cache/parts/easyeda/*/C*.json")
    ):
        with open(path) as f:
            data = json.load(f)
        if data.get("atopile_queried_at") == now:
            continue
        data["atopile_queried_at"] = now
        with open(path, "w") as f:
            json.dump(data, f, indent=4)
        touched += 1
    print(f"touch-parts-cache: {touched} cached parts freshened")


if __name__ == "__main__":
    main()
