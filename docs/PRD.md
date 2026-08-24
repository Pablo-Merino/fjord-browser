# Fjord Product Brief

## What Fjord Is

Fjord is an experimental browser for people who use Omarchy and Hyprland. It
uses a native Rust interface instead of web-based browser chrome. WPE WebKit
displays websites.

The goal is not to copy Chrome. The goal is a fast, keyboard-first browser that
fits a modern Wayland desktop.

## Current Status

Fjord is proving its core technical design. It is not ready for everyday use.
The project will not build broad browser features until it can share web frames
with GPUI directly and route input correctly.

The active plan lives in:

- [Technical gates](kill-gates.md)
- [Personal alpha](mvp-alpha.md)
- [Future improvements](improvements.md)

Those documents override this brief when they disagree.

## First User

The first user is one technical Omarchy user. Fjord will start as a secondary
browser. Chrome remains the default browser and fallback for unsupported sites.

## Design Principles

- Native interface: GPUI owns Fjord's controls, tabs, menus, and windows.
- Web standards: WPE WebKit owns HTML, CSS, JavaScript, networking, cookies,
  and webpage rendering.
- Wayland first: Fjord targets Hyprland and does not promise X11 support.
- Keyboard first: normal browser actions must work without a mouse.
- Minimal interface: show controls when they are useful, not by default.
- Fast interaction: opening tabs, switching tabs, and browser controls should
  feel immediate.
- Honest limits: do not hide unsupported websites behind browser-specific hacks.

## Product Shape

Fjord has two main parts:

```text
GPUI
  Fjord interface, tabs, commands, and browser state

WPE WebKit
  Websites, web storage, JavaScript, and webpage rendering
```

They communicate through a narrow boundary. WPE-specific values stay in the
`webkit` crate. Browser state and GPUI views stay in the `app` crate.

```text
crates/
├── app/
└── webkit/
```

## First Alpha

The first alpha aims to support real work and communication in a secondary
browser. It includes:

- Left-side vertical tabs, expanded by default and collapsible to icons
- A toolbar with navigation, address, security, and menu controls
- Sessions, history, website data, and permissions
- Downloads, uploads, clipboard, media, WebRTC, and screen sharing
- Native prompts for webpage requests such as permissions and file selection
- A theme based on the active Omarchy theme

The full alpha contract is in [mvp-alpha.md](mvp-alpha.md).

## Not In The Alpha

- Default-browser registration
- Bookmarks and private browsing
- Saved passwords and passkeys
- Extensions, content blocking, and DRM
- Printing
- Full multi-window support
- Browser workspaces and split views
- Public packaging and sync

Future work is listed in [improvements.md](improvements.md). It enters scope
only when there is a real reason to build it.

## Technical Decision

Fjord must share web frames with the GPU without a permanent path through CPU
memory. Direct GPUI import is preferred. A clean WPE Wayland subsurface is
allowed when a GPU's format or modifier is unsupported by GPUI. If neither path
works without a large GPUI fork or complex compositor workaround, the project
stops and reassesses the host architecture.

The exact tests and fallback options are in [kill-gates.md](kill-gates.md).
