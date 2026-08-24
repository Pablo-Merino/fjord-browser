#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
ARCH_SNAPSHOT=2026-08-20
IMAGE_HASH=$(sha256sum "$ROOT/Dockerfile" "$ROOT/rust-toolchain.toml" | sha256sum | cut -c1-12)
IMAGE="fjord-dev:arch-${ARCH_SNAPSHOT}-u$(id -u)-${IMAGE_HASH}"
CARGO_VOLUME="fjord-cargo-$(id -u)"

build_image() {
    docker build \
        --build-arg "HOST_UID=$(id -u)" \
        --build-arg "HOST_GID=$(id -g)" \
        --build-arg "ARCH_SNAPSHOT=${ARCH_SNAPSHOT//-//}" \
        --tag "$IMAGE" \
        "$ROOT"
}

ensure_image() {
    if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        build_image
    fi
}

run_container() {
    local needs_wpe_sandbox=$1
    local needs_dri=$2
    shift
    shift
    ensure_image

    local tty=()
    local devices=()
    local security=(--security-opt seccomp=unconfined --security-opt apparmor=unconfined)
    if [[ -t 0 && -t 1 ]]; then
        tty=(-it)
    fi

    if [[ $needs_wpe_sandbox == true ]]; then
        security=(--privileged)
    fi
    if [[ $needs_dri == true ]]; then
        devices=(--device /dev/dri)
    fi

    docker run --rm --init "${tty[@]}" "${security[@]}" "${devices[@]}" \
        --shm-size=1g \
        --security-opt seccomp=unconfined \
        --security-opt apparmor=unconfined \
        --volume "$ROOT:/workspace" \
        --volume "$CARGO_VOLUME:/home/fjord/.cargo" \
        --workdir /workspace \
        "$IMAGE" "$@"
}

run() {
    run_container false false "$@"
}

run_wpe() {
    run_container true false "$@"
}

run_wpe_hardware() {
    run_container true true "$@"
}

command=${1:-help}
shift || true

case "$command" in
    image) build_image ;;
    shell) run bash ;;
    fmt) run cargo fmt --all -- --check ;;
    check) run cargo check --workspace --locked --all-targets ;;
    test) run cargo test --workspace --locked ;;
    clippy) run cargo clippy --workspace --locked --all-targets -- -D warnings ;;
    verify) run bash scripts/verify.sh ;;
    headless-smoke) run bash scripts/headless-smoke.sh ;;
    wpe-smoke) run_wpe bash scripts/wpe-smoke.sh ;;
    wpe-smoke-network) run_wpe bash scripts/wpe-smoke.sh --network ;;
    wpe-stress) run_wpe bash scripts/wpe-stress.sh ;;
    wpe-hardware) run_wpe_hardware bash scripts/wpe-hardware.sh ;;
    run) run "$@" ;;
    *)
        cat <<'EOF'
Usage: scripts/dev.sh <command>

Commands:
  image           Build the pinned Arch development image
  shell           Open a shell in the development image
  fmt             Check Rust formatting
  check           Check the complete workspace
  test            Run workspace tests
  clippy          Run Clippy with warnings denied
  verify          Run all static and test checks
  headless-smoke  Start headless Sway and capture a blank compositor frame
    wpe-smoke       Run the sandboxed standalone WPE lifecycle smoke test
    wpe-smoke-network  Run the WPE lifecycle smoke test against example.com
    wpe-stress      Repeat WPE view lifecycles in one process and check file descriptors
    wpe-hardware    Run the WPE lifecycle smoke test with direct DRM access
  run <command>   Run an arbitrary command in the development image
EOF
        ;;
esac
