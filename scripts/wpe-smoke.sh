#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARTIFACT_DIR="$ROOT/artifacts/reports/runs/gate1"
if [[ ${1:-} == --network ]]; then
    REPORT="$ARTIFACT_DIR/wpe-smoke-network.txt"
else
    REPORT="$ARTIFACT_DIR/wpe-smoke.txt"
fi

mkdir -p "$ARTIFACT_DIR"

# WebKit's bwrap sandbox needs namespaces Docker's standard profile blocks.
# The outer container is privileged only for this Gate 1 verification command.
cargo run -p fjord-webkit --bin wpe-smoke --locked -- "$@" 2>&1 | tee "$REPORT"
