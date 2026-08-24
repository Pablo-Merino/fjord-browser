# Fjord Progress Log

Append only. Add one entry after every meaningful change. Include what changed,
checks run, saved evidence, and the next blocker or task. Keep raw logs in
artifacts.

## 2026-08-23: Development Setup

**Changed:** Added the Arch build container, Rust workspace, headless Wayland
check, and screenshot setup.

**Checks:** `dev.sh image`, `dev.sh verify`, `dev.sh headless-smoke`, and
ShellCheck passed.

**Evidence:** `artifacts/screenshots/runs/headless-smoke.png` and
`artifacts/screenshots/runs/vulkaninfo.txt`.

**Next:** Prove that WPE WebKit works safely on its own.

## 2026-08-23: WPE Lifecycle Harness

**Changed:** Added a headless WPE test for local HTML, JavaScript, HTTPS,
rendered frames, sandbox checks, and webpage-process termination.

**Checks:** `dev.sh verify`, `dev.sh wpe-smoke`,
`dev.sh wpe-smoke-network`, `dev.sh wpe-stress`, and
`dev.sh headless-smoke` passed.

**Evidence:** `artifacts/screenshots/runs/gate1/`.

**Current result:** WPE works with shared-memory frames in the headless
container. The sandbox check passes.

**Next:** Add an in-process repeated-view leak check. Then test GPU frame
sharing on the i915 laptop before calling Gate 1 complete.

## 2026-08-24: Documentation Rewrite

**Changed:** Rewrote the public documentation in plain English. The PRD now
matches the active personal-alpha scope. The agent guide now requires simple
language and progress-log updates.

**Checks:** Markdown links, whitespace checks, and a documentation review
passed.

**Evidence:** Documentation files in this repository.

**Next:** Add the in-process repeated-view leak check for Gate 1.
