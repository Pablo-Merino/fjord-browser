#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARTIFACT_DIR="$ROOT/artifacts/reports/runs/gate1"
REPORT="$ARTIFACT_DIR/wpe-stress.txt"

mkdir -p "$ARTIFACT_DIR"
: >"$REPORT"

# Two startup views establish the descriptor baseline. Five more views must
# create and destroy cleanly in the same process without growing it.
cargo run -p fjord-webkit --bin wpe-smoke --locked -- --stress 2>&1 | tee "$REPORT"
