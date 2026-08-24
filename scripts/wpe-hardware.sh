#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARTIFACT_DIR="$ROOT/artifacts/reports/runs/gate1"
REPORT="$ARTIFACT_DIR/wpe-hardware.txt"

mkdir -p "$ARTIFACT_DIR"

# This runs only on the i915 laptop with /dev/dri passed into the container.
cargo run -p fjord-webkit --bin wpe-smoke --locked -- --hardware 2>&1 | tee "$REPORT"
