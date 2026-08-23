# Fjord Technical Kill Gates

## Purpose

These gates decide whether GPUI and WPE WebKit form a viable browser
architecture on Wayland. They must pass before broad MVP implementation begins.
The project stops and reassesses the host architecture when a gate reaches its
defined failure condition.

This document supersedes the draft [PRD](PRD.md) where they conflict.

## Fixed Preconditions

| Area | Decision |
| --- | --- |
| Host UI | GPUI on Linux/Wayland |
| Web engine | Arch Linux `wpewebkit` 2.52.x package |
| GPUI baseline | Exact pinned Zed revision, initially `7733b9922665f103abda7c6a3fde6b9dfdc8eba9` |
| Toolchain | Rust toolchain matching the pinned Zed revision |
| Build host | Pinned Arch Linux Docker image on the headless Ubuntu host |
| Iteration display | Headless Wayland compositor, software rendering allowed |
| Hardware target | Omarchy laptop, Intel UHD 620, i915, Hyprland |
| GPU fallback | A small pinned GPUI fork is allowed for one Linux external-texture primitive |

The implementation must record the exact Arch image digest, package versions,
Rust toolchain, GPUI revision, and GPUI patch before evaluating a gate.

## Gate 1: Standalone WPE

### Question

Can the supported WPEPlatform API create, drive, and destroy browser views
cleanly in the target Arch environment?

### Entry Conditions

- The pinned Arch build image resolves `wpe-webkit-2.0` and WPEPlatform through
  `pkg-config`.
- The WPE process sandbox and multiprocess model are enabled.
- A deterministic local test page is available.

### Pass Criteria

- A dedicated GLib thread creates a persistent WPE profile and one web view.
- The view loads local HTML and `https://example.com` with JavaScript enabled.
- The host receives URI, title, load-state, crash, and frame events.
- SHM and DMA-BUF formats, modifiers, devices, strides, and sync capabilities
  are recorded.
- Creating and destroying repeated views does not leak processes or buffers.
- A tab-level WebProcess crash is observable without terminating the host.
- The sandbox state is verified rather than assumed from installed packages.

### Evidence

- A focused automated lifecycle test
- Redacted startup and shutdown logs
- Captured WPE and WPEPlatform versions
- A buffer-capability report

### Failure Action

Fix supported WPE setup, process ownership, and API usage before continuing. Do
not build GPUI chrome around an unproven engine lifecycle.

## Gate 2A: Headless Composition Correctness

### Question

Can GPUI and WPE coexist with correct frame ownership, geometry, and event-loop
behavior before hardware-specific zero-copy work?

### Entry Conditions

- Gate 1 passes.
- GPUI opens under the headless Wayland compositor.
- The GPUI and WPE event loops run independently and communicate through
  bounded channels.

### Pass Criteria

- GPUI displays frames from three simultaneous WPE views.
- Switching the active view never presents another tab's frame.
- A latest-frame-wins queue prevents frame accumulation.
- Superseded and displayed WPE buffers are released promptly.
- Resize storms tolerate stale-sized frames without deadlock or unbounded
  allocation.
- View creation, hiding, showing, and destruction remain deterministic.
- GPUI chrome stays responsive while pages load or render continuously.
- Idle chrome does not poll or request frames without a state change.

SHM or software rendering may satisfy this headless correctness gate. It does
not satisfy the zero-copy gate.

### Evidence

- Automated three-view lifecycle and resize tests
- Transient screenshots under `artifacts/screenshots/runs/`; curate a useful
  gate image into `baselines/` when visual evidence must be preserved
- Buffer acquire, present, replace, and release counters
- Process and memory snapshots before and after repeated view destruction

### Failure Action

Fix ownership, synchronization, or event-loop integration. Do not proceed to
hardware composition while the copy path can deadlock, leak, or display stale
tab content.

## Gate 2B: Hardware Zero-Copy Composition

### Question

Can WPE DMA-BUF frames be imported and presented by GPUI/WGPU without a CPU
readback or a broad GPUI fork?

### Entry Conditions

- Gate 2A passes.
- DMA-BUF format and modifier negotiation is understood.
- The renderer patch is isolated from layout, input, and general GPUI APIs.

### Pass Criteria

- A WPE DMA-BUF is imported directly into the GPUI renderer.
- Frame presentation honors explicit or implicit synchronization required by
  the driver.
- The rendered page has correct color, alpha, orientation, size, and stride.
- Three live views switch without CPU copies or stale content.
- Scrolling and resize remain responsive on the target i915 laptop.
- Buffer release occurs only after the renderer no longer uses the frame.
- The GPUI change is confined to one Linux external-texture primitive and its
  narrow public entry point.
- No permanent CPU-copy fallback is used by the product path.

The headless GTX 1650 may be used if container GPU setup is straightforward.
Do not turn matching NVIDIA userspace into a separate infrastructure project.
The gate is not passed until the i915 path runs on the Omarchy laptop.

### Evidence

- A renderer trace showing zero CPU readbacks
- Buffer and fence lifecycle counters
- Curated screenshots under `artifacts/screenshots/baselines/`
- A recorded i915/Hyprland launch and interaction check
- The complete GPUI fork diff

### Failure Ladder

1. Fix the narrow DMA-BUF import and synchronization path.
2. Evaluate a WPE Wayland subsurface only if parenting, clipping, focus,
   scaling, and synchronization remain clean.
3. Stop and compare alternative native hosts, including GTK4/WebKit.

Never continue toward polished browser features on a CPU-copy renderer.

## Gate 3: Input And Focus Contract

### Question

Can GPUI own browser commands while WPE owns webpage interaction without lost,
duplicated, or misrouted input?

### Entry Conditions

- Gate 2A passes for headless input work.
- Gate 2B passes before final laptop input acceptance.
- A deterministic page exercises links, scrolling, text, composition,
  clipboard formats, pointer capture, and fullscreen requests.

### Pass Criteria

- Pointer movement, buttons, hover, wheel scrolling, and cursor changes reach
  the correct WPE view.
- Keyboard events and modifiers use the target Wayland keymap correctly.
- `Ctrl+L` transfers focus from the page to the address bar.
- Returning focus to the page restores webpage keyboard input.
- Browser shortcuts do not leak into webpage text fields or composition.
- Clipboard integration supports plain text, HTML, and images.
- Text selection and copy, cut, and paste work.
- fcitx composition works in webpage text fields on the Omarchy laptop.
- Focus switching between the toolbar, sidebar, page, and transient popup does
  not strand input.
- Closing or crashing a tab releases its input ownership.

Synthetic input may prove routing headlessly. Real fcitx, cross-application
clipboard, and Hyprland focus behavior require the laptop confirmation pass.

### Evidence

- Focus and key-context GPUI tests
- Deterministic webpage input tests
- Headless synthetic pointer and keyboard runs
- A recorded laptop checklist for fcitx, clipboard, focus, and scrolling

### Failure Action

Fix the shared input boundary. Do not add workarounds at individual toolbar,
tab, or website call sites.

## Review Checkpoint

After all gates pass, pause implementation for review. The review build must
contain rough but real product chrome:

- a full-height left vertical sidebar;
- expanded and collapsed tab modes;
- three live WPE tabs;
- a top toolbar with Back, Forward, Reload/Stop, and address controls;
- zero-copy rendering on i915;
- working pointer, keyboard, scrolling, clipboard, focus, and fcitx input.

The review decides whether implementation continues into the
[personal MVP alpha](mvp-alpha.md).
