# Fjord

## Purpose

Fjord is a keyboard-first Wayland browser for Omarchy and Hyprland. GPUI owns
the browser interface. WPE WebKit owns webpage content.

## Current Scope

Finish the technical gates before building the personal alpha. The alpha is a
secondary browser for one user, not a replacement for Chrome or a public
release.

Do not add Chromium, permanent CPU frame copies, a custom engine, extensions,
DRM, default-browser registration, packaging, or speculative plugin systems.

## Source Of Truth

Read these files in this order:

1. `docs/kill-gates.md`
2. `docs/mvp-alpha.md`
3. `docs/improvements.md`
4. `docs/PRD.md`

Use `docs/progress.md` to see what changed, what passed, and what remains.
When documents conflict, the earlier file in this list wins.

## Code Boundaries

- Keep WPE and GObject values in `crates/webkit`.
- Keep browser state and GPUI chrome in `crates/app`.
- Add a crate or abstraction only after a real second use proves it is needed.
- Pin GPUI, Rust, Arch packages, and WPE. Upgrade them separately.
- Do not weaken sandboxing, TLS checks, input validation, or log redaction to
  make a test pass.
- Follow the fallback steps in `docs/kill-gates.md` if direct GPU frame sharing
  needs more than the approved narrow GPUI renderer change.

## Development Workflow

- Use `./scripts/dev.sh` for builds and checks.
- Run `./scripts/dev.sh verify` before reporting Rust work complete.
- Prefer headless Wayland checks first.
- Use the Omarchy laptop only when a gate needs i915, Hyprland, fcitx (text
  input), desktop portals, or other real desktop behavior.
- The Gate 1 WPE commands use a privileged outer Docker container only to test
  WPE's nested Bubblewrap sandbox. Do not extend that exception elsewhere.
- Keep routine captures in `artifacts/screenshots/runs/`. Commit only reviewed
  baselines from `artifacts/screenshots/baselines/`.

## Documentation Rules

- Write README and docs for a general reader first.
- Use short sentences and plain English.
- Avoid specialist terms when a simple phrase works. Define necessary terms once.
- Keep raw diagnostics in artifacts and source comments, not public documents.
- Update the relevant planning document when scope or behavior changes.
- Add an entry to `docs/progress.md` after every meaningful change. Include what
  changed, checks run, evidence location, and the next blocker or task.

## Language

- Client-facing: English
- End-user-facing: English

## References

- [Zed](https://github.com/zed-industries/zed): GPUI patterns and the pinned
  Wayland implementation.
- [Cog](https://github.com/Igalia/cog): WPE lifecycle and input patterns.
