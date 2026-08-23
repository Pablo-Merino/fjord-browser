# Fjord

## Mission

Build a fast, keyboard-first Wayland browser whose native GPUI chrome belongs
in Omarchy and Hyprland while WPE WebKit owns standards-based web content.

## Audience

The first audience is one technical Omarchy user evaluating Fjord as a
secondary browser. Broader distribution starts only after the architecture and
core compatibility prove reliable.

## Scope

The current scope is the technical kill gates followed by the personal MVP
alpha defined in `docs/`. GPUI owns browser UI, WPE WebKit owns web content, and
the application targets Linux, Wayland, and Hyprland.

## Out of Scope

Do not add Chromium, permanent CPU frame copies, a custom rendering engine,
Chrome extensions, DRM guarantees, default-browser registration, public
packaging, or speculative plugin systems. `docs/improvements.md` defines when
deferred work may enter scope.

## Languages

- **Client-facing**: English
- **End-user-facing**: English

## Reference Projects

- [**Zed**](https://github.com/zed-industries/zed): mirror GPUI ownership,
  actions, Wayland platform, and test patterns from the exact pinned revision.
- [**Cog**](https://github.com/Igalia/cog): consult WPE lifecycle, input, and
  platform integration patterns without copying its UI architecture.

## Sources Of Truth

- `docs/kill-gates.md` defines go/no-go technical acceptance and comes first.
- `docs/mvp-alpha.md` defines the approved personal alpha.
- `docs/improvements.md` owns deferred work and promotion triggers.
- `docs/PRD.md` is background context; focused documents supersede conflicts.

## Development

- Use `./scripts/dev.sh` so builds run in the pinned Arch environment.
- Run `./scripts/dev.sh verify` before reporting Rust changes complete.
- Prefer headless Wayland checks and repository-local screenshots; defer laptop
  tests until a gate specifically requires i915, Hyprland, fcitx, or portals.
- Keep routine captures in `artifacts/screenshots/runs/` and commit only reviewed
  baselines from `artifacts/screenshots/baselines/`.

## Working Agreements

- Make the smallest complete change and avoid speculative abstractions.
- Keep WPE and GObject values inside `crates/webkit`.
- Keep browser state and GPUI chrome inside `crates/app` until a proven boundary
  requires another crate.
- Pin GPUI, Rust, Arch packages, and WPE versions; review upgrades separately.
- Never weaken sandboxing, TLS, input validation, or redaction to make a test
  pass.
- Stop and reassess if zero-copy composition requires a broad GPUI fork.
- Update durable documentation when behavior, architecture, commands, or scope
  changes.
