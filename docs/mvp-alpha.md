# Fjord Personal MVP Alpha

## Status And Authority

This document defines the first user-facing Fjord release. It incorporates the
confirmed product interview and supersedes the draft [PRD](PRD.md) where they
conflict. The [technical kill gates](kill-gates.md) are prerequisites, not
optional MVP tasks.

## Objective

Deliver a fast, keyboard-first secondary browser for one Omarchy user. Fjord
must support real work, Google, AI, calls, screen sharing, and ordinary media
workflows while Chrome remains installed as the default and compatibility
fallback.

The alpha is not a public release and does not claim Chrome feature parity.

## Identity

| Field | Value |
| --- | --- |
| Development codename | Fjord |
| Binary | `fjord` |
| Wayland application ID | `io.github.PabloMerino.Fjord` |
| License | Apache-2.0 |
| Interface language | English |
| Target | Omarchy, Hyprland, Wayland |
| Delivery | Development binary |
| Default browser | No |

Fjord remains a codename until public naming and trademark checks are complete.
The application ID is provisional and will change with the final public name;
Hyprland rules and portal grants may need migration at that point.

## Product Principles

- GPUI owns all browser chrome.
- WPE WebKit owns web content and its supported process isolation.
- Interaction latency takes priority over speculative memory optimization.
- Browser state has one owner and explicit transitions.
- Website compatibility fixes go upstream first.
- Site-specific behavior is limited to explicit debug overrides.
- Errors never expose private browsing data through logs or diagnostics.

## Minimal Repository Shape

```text
crates/
├── app/       # GPUI process, browser model, views, actions, persistence
└── webkit/    # WPE thread, FFI, frames, input, engine events
```

Do not create a crate per feature. Add a headless core crate only after browser
state has a second consumer or a clear independent test boundary.

## Runtime Boundary

```text
GPUI application thread
├── Browser model
├── Vertical tab sidebar
├── Toolbar and overlays
├── Commands and focus
└── External-frame element
             │
             │ bounded commands and events
             ▼
Dedicated GLib/WPE thread
├── Persistent WPE profile
├── One WPE view per activated tab
├── Navigation and page state
├── Input-method context
└── DMA-BUF frame production
             │
             ▼
GPUI/Blade external-texture primitive
```

WPE and GObject values stay on the WPE thread. GPUI receives browser-facing
events and owned frame handles. A tab restored after startup creates its WPE
view on first activation and then keeps that view alive.

## Window Model

- Fjord owns one main browser window.
- Hyprland owns placement, borders, moving, resizing, and workspace policy.
- The window is undecorated and restores logical size only.
- A required authentication flow may open one constrained transient popup.
- A second Fjord process shows a warning and exits.
- Closing the final tab exits Fjord.
- Quitting with multiple tabs always requires confirmation.
- Quitting with active downloads separately confirms their cancellation.

## Vertical Tabs

The sidebar occupies the full left edge of the window.

| Mode | Behavior |
| --- | --- |
| Expanded | Approximately 240 px; favicon, title, loading/crash state, audio state, and close action |
| Collapsed | Approximately 44 px; favicon or fallback icon, state markers, and title tooltip |

The default is expanded. One global TOML setting persists the mode:

```toml
[tabs]
sidebar = "expanded" # "expanded" or "collapsed"
```

The sidebar includes:

- a scrollable tab list that keeps the active tab visible;
- a New Tab button at the bottom;
- the expanded/collapsed toggle below the New Tab button;
- a close button on the active or hovered expanded row;
- an audio indicator that toggles per-tab mute;
- stable keyboard focus and accessibility labels in both modes.

Initial tab behavior:

- New, close, activate, duplicate current URL, and reopen closed tab
- Middle-click closes a tab
- Middle-click or `Ctrl+click` on a link opens a background tab
- `Ctrl+Shift+click` opens and activates a tab
- Closing the active tab returns to the previously active tab
- `Ctrl+Tab` follows sidebar order
- `Ctrl+1` through `Ctrl+8` select indexed tabs
- `Ctrl+9` selects the final tab
- Background tabs remain alive after first activation
- A bounded recently closed stack persists across restarts
- Reordering is deferred

## Toolbar

The toolbar sits above webpage content and begins to the right of the sidebar.
It contains:

- Back and Forward buttons with honest disabled states;
- one Reload button that becomes Stop while the active tab loads;
- a full-URL address field;
- a security and site-information control;
- one overflow menu;
- a thin loading-progress line below the toolbar.

The toolbar remains visible in webpage and browser fullscreen. The sidebar
hides during fullscreen and restores its previous mode afterward.

## Address And Navigation

- New tabs open `fjord://newtab` and focus the address field.
- Google is the default configurable search engine.
- Text that parses as a URL navigates directly; other text becomes a search.
- Suggestions use open tabs and local history only.
- Fjord sends no remote search-suggestion requests before submission.
- Page zoom persists per origin; `Ctrl+0` clears that origin's override.
- Explicit user-selected or command-line `file://` URLs may open.
- Websites cannot navigate freely into local files.
- External schemes such as `mailto:` prompt and may remember an origin/scheme
  decision.

## Commands And Keybindings

Visible controls, keyboard shortcuts, menus, accessibility actions, and the
command palette invoke the same typed commands.

| Command | Default binding |
| --- | --- |
| Focus location | `Ctrl+L` |
| Find in page | `Ctrl+F` |
| Reload | `Ctrl+R` |
| Stop loading | `Escape` while loading |
| Back | `Alt+Left` |
| Forward | `Alt+Right` |
| Zoom in | `Ctrl++` |
| Zoom out | `Ctrl+-` |
| Reset zoom | `Ctrl+0` |
| New tab | `Ctrl+T` |
| Close tab | `Ctrl+W` |
| Reopen tab | `Ctrl+Shift+T` |
| Next tab | `Ctrl+Tab` |
| Previous tab | `Ctrl+Shift+Tab` |
| Open command palette | `Ctrl+K` |

TOML may override bindings after restart. Super bindings are not defaults and
hot reload is deferred.

The centered command palette searches commands, open tabs, and history. The
overflow menu includes a Keybindings item that opens a generated guide showing
effective defaults and overrides.

## Internal HTML Pages

Status pages use a locked-down local `fjord://` scheme:

- `fjord://newtab`
- `fjord://error`
- `fjord://crash`
- `fjord://keybindings`

Pages use bundled HTML and CSS with no JavaScript. They share the semantic theme
tokens and JetBrains Mono typography used by native chrome. Browser actions are
internal links intercepted by Fjord. The custom scheme cannot access network
resources, profile cookies, or ordinary website storage.

New Tab is visually blank because the address field owns initial focus. Error
and crash pages include focused recovery actions. If WebKit itself cannot
render, Fjord shows a minimal native GPUI fallback or startup error.

Settings, download management, site information, permissions, and destructive
data controls remain native GPUI surfaces. History browsing opens the command
palette in history mode.

## Native Web Surfaces

Fjord presents WPE browser-owned requests with GPUI-native surfaces:

- webpage context menus;
- select/option menus;
- JavaScript alerts, confirms, and prompts;
- HTTP authentication;
- permission requests;
- external-protocol confirmation;
- TLS warnings;
- file and download dialogs.

These surfaces preserve WPE semantics and return focus to the correct webpage
element when dismissed.

## Persistence

WPE owns cookies, cache, and website storage in a private persistent profile.
Fjord stores browser-owned metadata in SQLite:

- session windows and tabs;
- active-tab and recent-use order;
- history;
- recently closed tabs;
- permission decisions;
- per-origin zoom;
- per-site user-agent overrides;
- external-protocol decisions;
- completed-download records.

Startup restores the previous tab set automatically. It loads the active tab
first and leaves restored background tabs dormant until activation.

History and download records remain until explicitly cleared. Clearing the
download list never deletes downloaded files. Separate controls clear history,
download records, permissions, and WPE cookies/cache/site storage. A Clear All
action composes those explicit operations.

Data directories use private filesystem permissions. Application-level
database encryption is out of scope because Fjord stores no passwords.

Invalid TOML never prevents startup or overwrites the file. Fjord uses defaults
and reports the parse location through a native warning.

## Downloads And Files

- Downloads use the XDG Downloads directory and collision-safe names.
- The toolbar panel shows progress, cancel, retry, Open, and Reveal actions.
- Files never open automatically.
- Quitting confirms cancellation while transfers remain active.
- File uploads use the desktop portal where applicable.
- Wayland file drops into webpages are supported and validated.
- Webpage clipboard operations support plain text, HTML, and images.

## Permissions And Desktop Integration

Camera, microphone, location, notifications, and clipboard requests default to
deny until a native prompt receives a decision. Explicit decisions persist per
origin and remain editable in site information or settings.

- Screen sharing uses `xdg-desktop-portal` and the Hyprland portal backend.
- Allowed website notifications use the desktop notification service.
- Activating a notification focuses Fjord and the originating tab.
- Media autoplay follows WPE WebKit defaults.
- Tab audio state is visible and mutable from the sidebar.

## Security

Real-account testing starts only after this security gate passes:

- WPE's supported process sandbox and isolation are enabled and verified.
- A crashed tab does not terminate the GPUI browser shell.
- TLS verification is active.
- Profile and browser-data directories are private.
- Logs redact full URLs, titles, headers, cookies, form data, clipboard data,
  and local paths by default.
- Permission prompts default to deny.
- External schemes require confirmation.
- Local-file access follows the explicit-open policy.

Invalid TLS shows `fjord://error`. An advanced bypass applies only to that
visit and is never remembered. Clicking the address security indicator opens a
native site-information panel with connection state, certificate summary,
permissions, stored data controls, and bypass state.

Fjord stores no passwords and offers no autofill. Normal website sessions may
persist through WPE cookies. WPE privacy and tracking-prevention defaults remain
enabled.

## Debugging

The overflow menu exposes a debug section with:

- Open Web Inspector;
- per-site persistent user-agent override and reset;
- optional performance overlay;
- explicit URL logging for the current debug run.

The performance overlay reports GPUI frame time, WPE frame delivery, stale or
dropped buffers, active views, and process memory. It is disabled by default.
Diagnostics stay local and bounded; Fjord uploads no telemetry or crash reports.

## Appearance

- Read the active Omarchy theme at startup, initially Vantablack on the target.
- Map theme values into shared semantic tokens.
- Fall back to a built-in opaque dark theme.
- Use JetBrains Mono for browser chrome.
- Use a small bundled SVG set for browser controls.
- Use opaque surfaces without backdrop blur.
- Animate only sidebar mode and transient overlays briefly.
- Honor reduced motion and stop requesting frames when motion settles.
- Provide visible focus, semantic roles, labels, non-color state cues, and
  sufficient contrast.

Screen-reader runtime validation is deferred, but keyboard operation and
semantic accessibility metadata block the alpha.

## Performance Policy

Interaction latency is the primary performance goal. Fjord measures but does
not hard-gate:

- startup to usable GPUI chrome;
- command-palette opening;
- warm tab switching;
- new-tab creation;
- GPUI frame time;
- WPE frame delivery and stale-frame count;
- idle CPU and GPU activity;
- process and per-tab memory.

Startup is progressive. GPUI opens a usable window and focusable address field
before WPE profile and restored-tab initialization completes. Do not prewarm a
blank WPE view unless new-tab measurements justify it. Do not suspend active
background views unless memory measurements justify the added lifecycle state.

## Validation Workflow

Fast iteration stays on the headless host:

1. Build inside the pinned Arch Linux container.
2. Run pure, persistence, service, and GPUI tests.
3. Launch under a disposable headless Wayland compositor.
4. Exercise deterministic local web fixtures with synthetic input.
5. Capture screenshots and performance traces in the repository.

Use software rendering when container NVIDIA setup becomes an infrastructure
distraction. The laptop is required only after the headless kill gates pass,
for i915 DMA-BUF, Hyprland focus, fcitx, cross-application clipboard, portals,
media devices, and real-account compatibility.

Routine screenshots go to `artifacts/screenshots/runs/` and remain ignored.
Selected milestone and regression baselines go to
`artifacts/screenshots/baselines/` and may be committed.

The personal alpha validates only the laptop's current display. Broader scale
and monitor combinations are later compatibility work.

## Required Compatibility Suite

Core workflows must pass on:

| Suite | Required behavior |
| --- | --- |
| Work | GitHub, Linear, AWS Console, and Slack web login, navigation, editing, uploads, and downloads |
| Google | Account login, Gmail, Calendar, Drive, and Docs |
| AI | ChatGPT streaming, rich input, clipboard, and uploads |
| Calls | Google Meet camera, microphone, notifications, and portal screen sharing |
| Media | YouTube playback, seek, audio, mute, and fullscreen with toolbar |

Noncritical visual differences and Chromium-only capabilities may be
documented. Broken core flows block the personal alpha. Site-specific fixes go
upstream first; persistent per-site user-agent overrides are the only initial
compatibility escape hatch.

## Alpha Acceptance

The personal alpha is ready when:

- all [technical kill gates](kill-gates.md) pass;
- vertical tabs and toolbar behavior match this document;
- session, history, permissions, cookies, and site storage survive restart;
- crash recovery preserves the tab and shows `fjord://crash` with Reload;
- downloads, uploads, file drops, clipboard formats, media, notifications,
  camera, microphone, and portal screen sharing work;
- the real-account security gate passes;
- every required compatibility suite completes its core workflows;
- keyboard-only operation and semantic accessibility checks pass;
- local diagnostics remain bounded and redacted;
- Chrome remains unchanged as the system default;
- known limitations are recorded in [improvements](improvements.md).

Implementation pauses after the kill-gate review build. Continue into this MVP
only after explicit approval.
