# Fjord Improvements

## Purpose

This document tracks work deliberately excluded from the personal MVP alpha.
An item moves into active scope only when its trigger occurs. The list avoids
designing future systems before real browser usage supplies evidence.

This document supersedes the draft [PRD](PRD.md) where they conflict.

## Product And Distribution

| Improvement | Promotion trigger |
| --- | --- |
| Public product name and branding | The personal alpha proves the architecture and naming checks begin |
| Arch package and desktop entry | The development binary survives sustained personal use without profile or upgrade breakage |
| Default-browser registration | Fjord reliably handles external URLs and the user explicitly chooses to trial it as default |
| Public Omarchy beta | Core compatibility passes on more than one Omarchy machine and hardware profile |
| Broader Wayland distribution support | Contributors provide reproducible runtime evidence outside Omarchy |
| Signed releases and provenance | Fjord distributes binaries beyond the developer's machines |
| Update mechanism | A distribution channel exists that is not already owned by a package manager |

## Browser Features

| Improvement | Promotion trigger |
| --- | --- |
| Bookmarks | History and palette search are stable and saved resources become a repeated need |
| Private browsing | The persistent profile is reliable and beta privacy requirements are defined |
| Password manager integration | Manual external password entry becomes a material daily-use problem |
| Passkeys and WebAuthn product UI | A required workflow cannot use WPE's existing capability without browser integration |
| Content and ad blocking | Compatibility is stable enough to distinguish filter effects from engine bugs |
| WebExtensions | Specific indispensable extensions are identified and WPE support is mature enough to evaluate |
| DRM and Widevine | A supported legal distribution and integration path exists |
| Printing and PDF output | Printing becomes a repeated workflow that Chrome fallback no longer covers adequately |
| Bookmarks import | Bookmarks enter MVP scope and a real source format must be migrated |
| Profile import | Security, ownership, and migration semantics are defined for an actual source browser |
| Multiple persistent profiles | One profile cannot safely satisfy demonstrated work/personal separation needs |

## Tabs And Windows

| Improvement | Promotion trigger |
| --- | --- |
| Tab reordering | Stable tab identity, persistence, pointer capture, and keyboard movement are proven |
| Pinned tabs | Repeated daily tabs need behavior distinct from restored ordinary tabs |
| Tab groups | The vertical list becomes difficult to navigate in measured personal use |
| Browser workspaces | Stable tabs and sessions exist and separate tab collections solve a repeated workflow |
| Full multi-window support | The single main window and transient popup lifecycle are reliable |
| Window/session workspace mapping | Both Fjord workspaces and Hyprland IPC are independently useful and stable |
| Split views | One-view-per-tab composition and focus routing are robust enough for simultaneous active views |
| Tab previews | Switching or palette search lacks enough context without a visual preview |

## Performance

| Improvement | Promotion trigger |
| --- | --- |
| Prewarmed blank WebView | Measured new-tab creation is perceptibly slow |
| Background-tab suspension | Measured memory or power use is unacceptable with WebKit throttling alone |
| Persistent frame cache | Warm tab switching misses its measured target because the renderer lacks a current frame |
| GPU-specific optimizations | Profiles identify a driver-specific bottleneck after correctness is established |
| Hard performance budgets | Baselines are stable enough that CI variance is understood |

## Configuration And Desktop Integration

| Improvement | Promotion trigger |
| --- | --- |
| Configuration hot reload | Restart-applied TOML changes become a frequent development or user problem |
| Browser-owned themes | Omarchy inheritance cannot express a requested visual configuration |
| Theme-change hot reload | Runtime Omarchy theme switching becomes a regular workflow |
| Configurable sidebar side and width | Runtime use shows fixed left-side compact widths are inadequate |
| Hyprland IPC | A specific browser workflow requires compositor state or commands |
| Browser/Hyprland workspace mapping | Browser workspaces ship and users request explicit compositor coupling |
| Native URL-handler IPC | Fjord is prepared for default-browser or desktop URL-handler trials |

## Platform And Accessibility

| Improvement | Promotion trigger |
| --- | --- |
| Orca screen-reader validation | The personal alpha prepares for users beyond the initial target |
| Physical multi-monitor matrix | Fjord enters beta or a real monitor issue is reported |
| Fractional scaling and HiDPI matrix | Fjord enters beta or a real scale-factor issue is reported |
| Alternative GPU validation | Fjord targets machines beyond the tested i915 system |
| X11 support | A concrete target user cannot run Wayland and the maintenance cost is accepted |
| Other operating systems | The Wayland product is stable and a supported GPUI/WebKit architecture is proposed |

## Extensibility And Advanced Tools

| Improvement | Promotion trigger |
| --- | --- |
| User scripts or styles | Repeated workflows need customization that commands cannot provide |
| External command or Unix-socket API | A concrete automation client and security model exist |
| D-Bus API | Desktop integration needs a stable cross-process contract |
| Rust, WASM, Lua, or JavaScript plugins | Commands and external automation cannot satisfy proven extension use cases |
| Advanced developer tools | The WPE inspector is insufficient for a repeated debugging workflow |
| AI commands | Core browser actions and safe page/selection context boundaries are stable |

## Explicitly Rejected Until Triggered

- A custom rendering engine or WebKit fork
- Chromium compatibility layers or application-level website workaround soup
- Permanent CPU frame copies
- A framework around GPUI version differences
- A crate or interface per speculative feature
- Browser-owned updater work while package managers can own updates
- Cloud sync without a defined account, encryption, conflict, and recovery model
