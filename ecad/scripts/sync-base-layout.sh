#!/usr/bin/env bash
# Pull the shared box-emu-base layout (group "box") and each variant's
# connector layout (group "connector") into the two top-level boards.
# Run after editing elec/layout/box-emu-base/box-emu-base.kicad_pcb (or the
# connector boards). Close the parent boards in KiCad first.
set -euo pipefail
cd "$(dirname "$0")/.."

for board in box-emu box-3-emu; do
    echo "== $board"
    ato kicad-ipc layout-sync --legacy \
        --board "elec/layout/$board/$board.kicad_pcb" \
        --include-group box \
        --include-group connector
done
