#!/usr/bin/env bash
# Build with the local components-API stand-in (see parts-cache-server.py).
# atopile's hosted picker endpoint (components.atopileapi.com) was shut down
# in the 0.16 platform migration; until a public replacement exists for the
# 0.15.x CLI, this serves the picker from data already pinned in the repo.
#
# Usage: scripts/ato-build-offline.sh [ato build args...]
#   e.g. scripts/ato-build-offline.sh -b box-emu-base
#        scripts/ato-build-offline.sh            (all targets)
#
# Env overrides (used by CI, where `ato` runs inside a docker container and
# must reach this server through the docker bridge gateway):
#   PARTS_CACHE_BIND  bind address for the server   (default 127.0.0.1)
#   PARTS_CACHE_URL   URL handed to ato             (default http://127.0.0.1:$PORT)
set -euo pipefail
cd "$(dirname "$0")/.."

PORT="${PARTS_CACHE_PORT:-8471}"
BIND="${PARTS_CACHE_BIND:-127.0.0.1}"
URL="${PARTS_CACHE_URL:-http://127.0.0.1:$PORT}"

if ! curl -sf --max-time 2 "http://127.0.0.1:$PORT/v0/component/lcsc/1525" >/dev/null 2>&1; then
    python3 scripts/parts-cache-server.py --host "$BIND" --port "$PORT" &
    SERVER_PID=$!
    trap 'kill $SERVER_PID 2>/dev/null || true' EXIT
    for _ in $(seq 1 20); do
        curl -sf --max-time 1 "http://127.0.0.1:$PORT/v0/component/lcsc/1525" >/dev/null 2>&1 && break
        sleep 0.25
    done
fi

ATO_SERVICES_COMPONENTS_URL="$URL" \
    ato build --keep-picked-parts --keep-net-names --keep-designators "$@"
