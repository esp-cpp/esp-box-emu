"""The espp host clients (espp_ota, espp_coredump), found next to this repo.

They ship with the espp components: in a checkout of espp
(``$ESPP_DIR/components/<name>/python``) or in a project's
``managed_components/espp__<name>/python`` after a build.
"""

from __future__ import annotations

import importlib
import os
import sys
from pathlib import Path

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[3]  # tools/emulink/emulink/espp_tools.py -> the project


def _candidates(component: str):
    if os.environ.get("ESPP_DIR"):
        yield Path(os.environ["ESPP_DIR"]) / "components" / component / "python"
    for base in (_REPO, _REPO / "updater"):
        yield base / "managed_components" / f"espp__{component}" / "python"
    yield Path.home() / "esp-cpp" / "espp" / "components" / component / "python"


def load(package: str, component: str):
    """Import `package` (e.g. espp_ota) from wherever the espp component is."""
    try:
        return importlib.import_module(package)
    except ImportError:
        pass
    for c in _candidates(component):
        if (c / package).is_dir():
            sys.path.insert(0, str(c))
            return importlib.import_module(package)
    raise ImportError(f"{package} not found: set ESPP_DIR to an espp checkout, or build the project "
                      f"so managed_components/espp__{component} exists")
