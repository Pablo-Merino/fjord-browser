# PRD: Native GPUI + WebKit Browser

**Codename:** Fjord
**Status:** Draft
**Target:** Linux / Wayland / Hyprland
**Primary stack:** Rust + GPUI + WPE WebKit
**Initial environment:** Omarchy / Hyprland

## 1. Product Vision

Build a **native, keyboard-first Linux browser** whose interface feels like it belongs inside Hyprland/Omarchy rather than being a Chromium application dressed to match it.

The browser separates the **browser UI** from the **web engine**:

```text
GPUI                         WPE WebKit
─────────────────────        ─────────────────────
Window chrome                HTML/CSS
Tabs                         JavaScript
Command palette              DOM
Navigation                   Networking
History                      Cookies/storage
Downloads                    Page rendering
Settings
Keyboard interaction
        │                         │
        └────────────┬────────────┘
                     │
                  Wayland
                     │
                  Hyprland
```

GPUI owns the application experience. WebKit exists primarily to render and interact with the web.

The long-term goal is **not another general-purpose Chrome clone**. It is a browser designed around the workflows and interaction patterns of modern keyboard-driven Linux desktops.

---

## 2. Product Principles

### Native First

No Electron, React, HTML, or CSS for browser chrome.

All browser UI should be implemented in Rust using GPUI.

### Wayland First

Wayland is the primary display protocol. Hyprland is the primary development target.

X11 compatibility is explicitly not required initially.

### Keyboard First

Nearly every browser operation should be possible without touching the mouse.

Mouse interaction remains fully supported for webpages and conventional UI interactions.

### WebKit, Not Browser-Engine Development

We are building a browser, not a rendering engine.

Do not fork WebKit unless an extraordinary technical requirement eventually appears.

### Minimal Chrome

The browser should maximize webpage space and avoid permanent UI that does not provide continual value.

### Fast

Launching a browser window, opening tabs, and interacting with browser chrome should feel closer to opening a native terminal/editor than launching Chrome.

---

## 3. Target User

Initially:

> Technical Linux users running Wayland compositors, particularly Hyprland/Omarchy, who prefer keyboard-driven native applications.

Typical environment:

```text
Hyprland
Omarchy / Arch
Kitty / Ghostty
Neovim / Zed
Waybar
wofi / fuzzel
tmux
```

These users value:

- keyboard navigation
- low latency
- native Wayland behavior
- minimal interfaces
- configuration
- composability
- tiling window managers
- predictable shortcuts
- avoiding Electron where practical

The initial product does **not** need to appeal to mainstream Chrome users.

---

## 4. Technical Architecture

### Application

```text
Rust
 │
 ├── GPUI
 │    ├── window management
 │    ├── rendering
 │    ├── browser chrome
 │    ├── commands
 │    ├── keyboard input
 │    └── application state
 │
 ├── Browser Core
 │    ├── tabs
 │    ├── navigation
 │    ├── history
 │    ├── bookmarks
 │    ├── sessions
 │    └── configuration
 │
 └── WPE WebKit
      ├── rendering
      ├── JavaScript
      ├── DOM
      ├── HTTP
      ├── cookies
      ├── storage
      └── web APIs
```

### Architectural Boundary

The architecture should enforce:

```text
Browser functionality
        ↓
     Rust Core
        ↓
 ┌──────┴───────┐
GPUI           WebKit
```

GPUI components should not directly become responsible for WebKit state management.

Everything WebKit-specific should live behind an abstraction layer.

This keeps the integration replaceable and testable and prevents WebKit-specific types from spreading throughout the application.

---

## 5. Browser Data Model

Conceptually:

```rust
struct Browser {
    windows: Vec<BrowserWindow>,
    history: HistoryStore,
    bookmarks: BookmarkStore,
    sessions: SessionStore,
}

struct BrowserWindow {
    id: WindowId,
    tabs: Vec<Tab>,
    active_tab: TabId,
}

struct Tab {
    id: TabId,
    webview: WebView,
    url: Url,
    title: String,
    loading: bool,
}
```

The actual implementation should evolve based on GPUI's ownership and state model.

---

# Development Roadmap

## v0.1: WebKit Spike

### Objective

Answer one question:

> **Can GPUI and WPE WebKit coexist cleanly inside one native Wayland application?**

Do not build a browser yet.

### UI

```text
╭────────────────────────────────────────────╮
│ https://example.com                       │
├────────────────────────────────────────────┤
│                                            │
│                                            │
│              WebKit View                   │
│                                            │
│                                            │
╰────────────────────────────────────────────╯
```

### Requirements

GPUI window containing:

- basic address bar
- WebKit rendering surface

WebKit must support:

- loading URLs
- JavaScript
- keyboard input
- mouse input
- scrolling
- clipboard
- text selection
- links
- navigation

### Critical Tests

Verify:

- Hyprland compatibility
- resizing
- maximize/fullscreen
- fractional scaling
- HiDPI
- multiple monitors
- focus switching
- keyboard focus
- mouse focus
- clipboard
- IME/input methods
- GPU acceleration

### Exit Criteria

No major architectural hack is necessary to host WebKit.

**This is a kill gate.**

If WPE integration requires unreasonable compositor gymnastics, investigate WebKitGTK or alternative embedding architectures before proceeding.

---

## v0.2: Minimal Browser

Turn the technical experiment into something capable of basic daily browsing.

### Navigation

Implement:

- address bar
- URL navigation
- search queries
- back
- forward
- reload
- stop
- HTTPS indicator

Input behavior:

```text
github.com
↓
https://github.com

gpui webkit integration
↓
search engine query
```

Search engine should initially be configurable.

### Tabs

Support:

- new tab
- close tab
- switch tab
- reopen closed tab
- duplicate tab

Example:

```text
╭────────────────────────────────────────────╮
│ 󰈹 GitHub     󰈹 Docs     󰈹 Hacker News  + │
├────────────────────────────────────────────┤
│ github.com                                 │
├────────────────────────────────────────────┤
│                                            │
│                webpage                     │
│                                            │
╰────────────────────────────────────────────╯
```

Tab UI can initially remain simple.

### Shortcuts

| Shortcut | Action |
| --- | --- |
| `Ctrl+L` | Address bar |
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab |
| `Ctrl+Shift+T` | Restore tab |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `Alt+Left` | Back |
| `Alt+Right` | Forward |
| `Ctrl+R` | Reload |
| `Ctrl+K` | Command palette |

Standard shortcuts should work even if more opinionated bindings are added later.

---

## v0.3: Native Browser UX

This is where the browser begins developing its identity.

### Vertical Tab Sidebar

Primary navigation becomes:

```text
╭──────────────┬─────────────────────────────╮
│              │                             │
│ 󰈹 GitHub     │                             │
│              │                             │
│ 󰈹 Docs       │          webpage            │
│              │                             │
│ 󰈹 Reddit     │                             │
│              │                             │
│──────────────│                             │
│ + New Tab    │                             │
╰──────────────┴─────────────────────────────╯
```

Sidebar should be collapsible.

Collapsed:

```text
╭────┬───────────────────────────────────────╮
│ 󰈹  │                                       │
│ 󰈹  │                                       │
│ 󰈹  │               webpage                 │
│    │                                       │
│ +  │                                       │
╰────┴───────────────────────────────────────╯
```

### Command Palette

Inspired by Zed rather than conventional browser menus.

```text
╭─────────────────────────────────────────╮
│ > tab                                   │
├─────────────────────────────────────────┤
│ New Tab                                 │
│ Close Tab                               │
│ Duplicate Tab                           │
│ Move Tab to Workspace                   │
│ Reopen Closed Tab                       │
╰─────────────────────────────────────────╯
```

Every important browser action should be exposed as a command.

Commands should have stable IDs:

```text
browser.new_tab
browser.close_tab
browser.reload

tab.next
tab.previous
tab.pin

history.show

workspace.create
workspace.next
```

Keyboard shortcuts map onto commands rather than directly invoking implementation functions.

This makes configuration substantially easier later.

---

## v0.4: Daily Driver

Add the features necessary to use the browser seriously.

### History

Persistent local history.

Searchable through the command palette:

```text
> hist github gpui

GitHub - zed-industries/zed
github.com/zed-industries/zed

GPUI Documentation
docs.rs/gpui
```

SQLite is a good initial storage backend.

### Downloads

Native GPUI download manager.

```text
Downloads

ubuntu.iso
████████████████░░░░  78%
1.8 GB / 2.3 GB

zed-linux.tar.gz
✓ Complete
```

Support:

- progress
- cancel
- retry
- open
- reveal in file manager

### Bookmarks

Bookmarks should behave more like searchable resources than a conventional bookmarks toolbar.

Accessible primarily through:

- command palette
- address bar search
- sidebar

### Session Restore

On startup:

```text
previous windows
    ↓
workspaces
    ↓
tabs
    ↓
URLs
```

Crashes should not destroy the previous browsing session.

### Permissions

Native permission UI for:

- microphone
- camera
- location
- notifications
- clipboard

---

## v0.5: Workspaces

Introduce browser-level workspaces.

Example:

```text
WORK
 ├ GitHub
 ├ Linear
 ├ Slack
 └ AWS

PERSONAL
 ├ Reddit
 ├ YouTube
 └ Hacker News

RESEARCH
 ├ WebKit docs
 ├ GPUI source
 └ GitHub issues
```

Switching workspace swaps the active tab collection.

Potential shortcuts:

```text
Super+1
Super+2
Super+3
```

These must remain configurable because Hyprland heavily uses `SUPER`.

### Persistent Tabs

Allow certain tabs to survive normal cleanup:

```text
● GitHub
● Linear
● ChatGPT

  Documentation
  Search results
```

This gives the browser some Arc-like concepts without reproducing Arc's interface.

---

## v0.6: Omarchy / Hyprland Integration

The application should start behaving like part of the desktop.

### Native Visual System

Configuration should support things such as:

```toml
[ui]
font = "JetBrainsMono Nerd Font"
radius = 8
border_width = 1
animations = true

[window]
decorations = false
```

Ideally theme values can eventually be inherited from the surrounding environment.

### Hyprland IPC

Optional Hyprland integration could expose:

- current workspace
- monitor
- window state
- special workspaces
- compositor commands

This could enable deeper browser/desktop interactions.

For example:

```text
browser workspace
        ↕
Hyprland workspace
```

A browser workspace could optionally map onto a Hyprland workspace.

---

## v0.7: Configuration

Provide a human-editable configuration file.

```text
~/.config/<browser>/config.toml
```

Example:

```toml
search_engine = "kagi"

[bindings]
"ctrl-l" = "browser.location"
"ctrl-k" = "command_palette.open"
"alt-j" = "tab.next"
"alt-k" = "tab.previous"

[privacy]
block_third_party_cookies = true

[ui]
vertical_tabs = true
animations = true
```

Changes should eventually hot reload.

---

## v0.8: Extensibility

Do **not** immediately attempt Chrome extension compatibility.

Instead, expose browser functionality through the command architecture first.

Potential extension mechanisms can later include:

- Rust plugins
- WASM
- Lua
- JavaScript
- external commands
- D-Bus
- Unix sockets

The precise model should be decided after actual user workflows emerge.

---

# Future: Browser as a Programmable Desktop Tool

Once the fundamentals are reliable, there is a much more interesting direction than competing feature-for-feature with Chrome.

## Split Views

```text
╭────────────┬──────────────────┬──────────────╮
│ tabs       │ GitHub           │ Documentation│
│            │                  │              │
│            │                  │              │
│            │                  │              │
╰────────────┴──────────────────┴──────────────╯
```

Each pane hosts an independent WebKit view.

## Webpage Command Integration

Expose contextual browser operations:

```text
Copy page as Markdown
Open link in workspace
Copy clean URL
Search selected text
Open source
Inspect element
Save page
```

## Developer Mode

Given the target audience, developer tooling should eventually be a first-class feature.

Potential capabilities:

- WebKit inspector
- request inspection
- cookie/storage viewer
- JavaScript console
- responsive viewport
- user-agent switching
- proxy configuration
- per-site JavaScript toggles

## AI

AI should **not** become a permanent chatbot sidebar.

Instead, it should participate in the command model.

For example:

```text
> summarize page
> explain selection
> find where this page discusses WebKit
> compare current tabs
> extract table
> ask current page
```

This keeps AI contextual rather than consuming permanent screen space.

---

# Explicit Non-Goals

For the early versions:

- Chromium compatibility
- Chrome extension support
- X11
- Windows
- macOS
- mobile
- browser sync
- custom rendering engine
- WebKit fork
- Google account integration
- DRM/Widevine guarantees
- pixel-perfect Chrome compatibility

Trying to solve these early would bury the interesting product underneath browser-engine archaeology.

---

# Performance Targets

| Metric | Target |
| --- | ---: |
| Browser chrome | 60+ FPS |
| UI input latency | <16 ms |
| Idle CPU | ~0% |
| New empty tab | <50 ms perceived |
| Command palette | Effectively instant |
| Session restore | Progressive |

Webpage performance itself will largely depend on WebKit.

Memory should be measured rather than given an arbitrary target because WebKit's process model will dominate it.

---

# Repository Structure

A sensible starting point:

```text
browser/
├── crates/
│   ├── app/
│   ├── browser/
│   ├── webkit/
│   ├── ui/
│   ├── commands/
│   ├── config/
│   ├── history/
│   └── session/
│
├── assets/
├── config/
├── Cargo.toml
└── README.md
```

The critical crate is:

```text
webkit/
```

Everything WebKit-specific should live behind this boundary.

For example:

```rust
trait WebView {
    fn navigate(&mut self, url: &Url);
    fn reload(&mut self);
    fn stop(&mut self);

    fn go_back(&mut self);
    fn go_forward(&mut self);

    fn title(&self) -> Option<&str>;
    fn url(&self) -> Option<&Url>;
}
```

Do not let WebKit types spread throughout the application.

---

# Biggest Technical Risks

## 1. WPE ↔ GPUI Composition

This is the project-defining technical risk.

Solve it before doing almost anything else.

## 2. Input Routing

GPUI and WebKit need predictable ownership of:

- keyboard
- pointer
- scrolling
- focus

## 3. Wayland Surfaces

Subsurface management, resizing, and synchronization need to behave correctly across Hyprland.

## 4. IME

Text input must work properly inside websites.

Browser text input is substantially more complicated than handling ordinary keyboard events.

## 5. GPU Composition

Avoid expensive GPU → CPU → GPU copies.

The desired path is approximately:

```text
WebKit
  ↓
GPU buffer
  ↓
Wayland / compositor
```

rather than:

```text
WebKit GPU
  ↓
CPU bitmap
  ↓
GPUI GPU
```

## 6. Web Compatibility

Some websites may assume Chromium-specific behavior.

That's largely inherited from WebKit and should not turn into application-level workaround soup.

---

# First Milestone

Resist the temptation to build tabs, bookmarks, themes, or beautiful Omarchy chrome first.

The first repository milestone should contain approximately this:

```text
$ cargo run

╭──────────────────────────────────────────────╮
│ example.com                                  │
├──────────────────────────────────────────────┤
│                                              │
│             Example Domain                   │
│                                              │
│   This domain is for use in illustrative...  │
│                                              │
╰──────────────────────────────────────────────╯
```

The engineering checklist is deliberately simple:

> **GPUI window → WPE WebKit → Wayland → Hyprland**

The prototype must:

- browse the real web
- resize smoothly
- survive fractional scaling
- support keyboard input
- support mouse input
- support IME
- support clipboard operations
- remain GPU accelerated
- behave correctly under Hyprland

If those conditions are satisfied, **the architectural gamble has paid off**.

At that point, development can move from browser-engine integration work to building the actual product.
