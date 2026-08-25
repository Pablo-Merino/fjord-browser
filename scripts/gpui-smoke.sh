#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARTIFACT_DIR="$ROOT/artifacts/reports/runs/gate2"
REPORT="$ARTIFACT_DIR/gpui-smoke.txt"

mkdir -p "$ARTIFACT_DIR"

cargo run -p fjord-app --locked 2>&1 | tee "$REPORT"
