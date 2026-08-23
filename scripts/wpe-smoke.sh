#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARTIFACT_DIR="$ROOT/artifacts/screenshots/runs/gate1"
REPORT="$ARTIFACT_DIR/wpe-smoke.txt"

mkdir -p "$ARTIFACT_DIR"

# WebKit's bwrap sandbox needs namespaces Docker's standard profile blocks.
# The outer container is privileged only for this Gate 1 verification command.
cargo run -p fjord-webkit --bin wpe-smoke --locked -- "$@" 2>&1 | tee "$REPORT"
