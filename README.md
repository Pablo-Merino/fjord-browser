# Fjord

Fjord is an experimental browser for Omarchy and Hyprland. It aims to feel like
a native Wayland application: quick to open, easy to use from the keyboard, and
small in the ways that matter.

Fjord uses GPUI for its own interface and WPE WebKit to display websites.

## Status

Fjord is not ready for everyday browsing yet. It is currently proving that its
two main parts can safely share a window without copying web frames through the
CPU. Until that works, Fjord is a technical experiment rather than a browser.

The first usable version will be a secondary browser for one Omarchy user.
Chrome stays installed and remains the default browser during this stage.

## What The First Alpha Includes

- Vertical tabs on the left, with expanded and icon-only modes
- Back, Forward, Reload, Stop, address, security, and menu controls
- Keyboard shortcuts and a command palette
- Sessions, history, site storage, permissions, downloads, uploads, and media
- Website notifications, camera, microphone, and desktop screen sharing
- A look based on the active Omarchy theme

Fjord will not initially include bookmarks, private browsing, saved passwords,
extensions, DRM, printing, multi-window browsing, or default-browser support.

## Development Transparency

Fjord is designed and implemented with agentic AI under human direction and
review. Generated work must pass the same checks and design review as any other
change. Fjord does not use AI at runtime.

## Documentation

Read these in order:

- [Product brief](docs/PRD.md)
- [Technical gates](docs/kill-gates.md)
- [Personal alpha](docs/mvp-alpha.md)
- [Future improvements](docs/improvements.md)
- [Progress log](docs/progress.md)

## Development

Fjord builds in a pinned Arch Linux container so it matches the target system.

```sh
./scripts/dev.sh image
./scripts/dev.sh verify
./scripts/dev.sh headless-smoke
```

The WPE checks are separate:

```sh
./scripts/dev.sh wpe-smoke
./scripts/dev.sh wpe-smoke-network
./scripts/dev.sh wpe-stress
```

Routine screenshots are saved under `artifacts/screenshots/runs/`. Reports are
saved under `artifacts/reports/runs/`. They are not committed. Reviewed
screenshots may go in `artifacts/screenshots/baselines/`.

## License

Fjord is licensed under the [Apache License 2.0](LICENSE). It is an independent
project, not an official Omarchy component.
