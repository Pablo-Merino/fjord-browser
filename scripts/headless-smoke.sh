#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
RUNTIME_DIR=
SWAY_LOG="$ROOT/artifacts/screenshots/runs/sway.log"
SCREENSHOT="$ROOT/artifacts/screenshots/runs/headless-smoke.png"
VULKAN_INFO="$ROOT/artifacts/screenshots/runs/vulkaninfo.txt"

cleanup() {
    if [[ -n "${SWAY_PID:-}" ]]; then
        kill "$SWAY_PID" 2>/dev/null || true
        wait "$SWAY_PID" 2>/dev/null || true
    fi
    [[ -n "$RUNTIME_DIR" ]] && rm -rf "$RUNTIME_DIR"
}
trap cleanup EXIT

RUNTIME_DIR=$(mktemp -d)
mkdir -p "$(dirname -- "$SCREENSHOT")"
chmod 700 "$RUNTIME_DIR"

export XDG_RUNTIME_DIR="$RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_HEADLESS_OUTPUTS=1
export WLR_LIBINPUT_NO_DEVICES=1
export WLR_RENDERER=pixman

sway --config "$ROOT/infra/sway-headless.conf" >"$SWAY_LOG" 2>&1 &
SWAY_PID=$!

for _ in $(seq 1 50); do
    sockets=("$RUNTIME_DIR"/wayland-*)
    [[ -S "${sockets[0]}" ]] && break
    sleep 0.1
done

if [[ ! -S "${sockets[0]}" ]]; then
    echo "headless Sway failed to create a Wayland socket" >&2
    exit 1
fi

WAYLAND_DISPLAY=$(basename -- "${sockets[0]}")
export WAYLAND_DISPLAY
vulkaninfo --summary >"$VULKAN_INFO"
grep -q "llvmpipe" "$VULKAN_INFO"
grim "$SCREENSHOT"
test -s "$SCREENSHOT"
printf 'Captured %s\n' "$SCREENSHOT"
