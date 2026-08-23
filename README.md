# Fjord

Fjord is the development codename for an experimental, keyboard-first browser
for Omarchy and Hyprland. It uses GPUI for native browser chrome and WPE WebKit
for web content.

The project aims to feel like part of a modern Wayland desktop rather than a
Chromium application styled to resemble one. The first target is a personal
secondary-browser alpha with vertical tabs, native controls, persistent
sessions, and enough compatibility for real work and communication workflows.

## Status

Fjord is in planning and technical-validation stage. The first implementation
must prove that GPUI and WPE WebKit can share a Wayland application with
zero-copy GPU composition and correct input routing. Product development stops
if that architecture requires broad compositor or framework hacks.

## Planned Alpha

- Native GPUI chrome with a full-height vertical tab sidebar
- Expanded title and collapsed icon-only tab modes
- Back, forward, reload/stop, address, security, and overflow controls
- WPE WebKit 2.52.x with its supported process sandbox and isolation
- Persistent tabs, history, cookies, site storage, and permissions
- Downloads, uploads, media, WebRTC, notifications, and portal screen sharing
- Omarchy theme inheritance and configurable keyboard shortcuts
- Local HTML status pages under `fjord://`

Fjord will remain a secondary browser during the personal alpha. It will not
register itself as the default browser.

## Documentation

- [Product requirements](docs/PRD.md)
- [Technical kill gates](docs/kill-gates.md)
- [Personal MVP alpha](docs/mvp-alpha.md)
- [Deferred improvements](docs/improvements.md)

The focused planning documents supersede the draft PRD where they conflict.

## Development

Development happens on a headless Ubuntu host using a pinned Arch Linux
container. UI and behavior iterations run under a headless Wayland compositor.
Testing on the Omarchy laptop is deferred until hardware-specific DMA-BUF,
i915, Hyprland, fcitx, and portal behavior must be verified.

The application has not been scaffolded yet. Build instructions will be added
after the technical kill gates establish the final dependency and renderer
shape.

## Screenshots

Runtime captures belong in [`artifacts/screenshots/`](artifacts/screenshots/).
Routine runs are ignored; selected milestone and regression baselines may be
committed.

## License

Licensed under the [Apache License 2.0](LICENSE).

Fjord is an independent project and is not currently an official Omarchy
component.
