#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARTIFACT_DIR="$ROOT/artifacts/screenshots/runs/gate1"
REPORT="$ARTIFACT_DIR/wpe-stress.txt"

mkdir -p "$ARTIFACT_DIR"
: >"$REPORT"

# Each run is a fresh host process. This catches lifecycle regressions without
# claiming the in-process leak measurement required to close Gate 1.
for iteration in $(seq 1 5); do
    printf 'iteration=%s\n' "$iteration" | tee -a "$REPORT"
    cargo run -p fjord-webkit --bin wpe-smoke --locked 2>&1 | tee -a "$REPORT"
done
