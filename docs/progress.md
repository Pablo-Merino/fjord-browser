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
`artifacts/reports/runs/vulkaninfo.txt`.

**Next:** Prove that WPE WebKit works safely on its own.

## 2026-08-23: WPE Lifecycle Harness

**Changed:** Added a headless WPE test for local HTML, JavaScript, HTTPS,
rendered frames, sandbox checks, and webpage-process termination.

**Checks:** `dev.sh verify`, `dev.sh wpe-smoke`,
`dev.sh wpe-smoke-network`, `dev.sh wpe-stress`, and
`dev.sh headless-smoke` passed.

**Evidence:** `artifacts/reports/runs/gate1/`.

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

## 2026-08-24: In-Process WPE Lifecycle Check

**Changed:** The WPE harness now creates and destroys seven views in one WPE
host process. It checks that every view releases its rendered buffer and does
not increase open file descriptors after two startup views establish the
baseline. The one-view smoke test continues to verify API web-process
termination.

**Checks:** `dev.sh verify`, `dev.sh wpe-smoke`, `dev.sh wpe-smoke-network`,
`dev.sh wpe-stress`, and `dev.sh headless-smoke` passed.

**Evidence:** `artifacts/reports/runs/gate1/wpe-smoke.txt`,
`artifacts/reports/runs/gate1/wpe-smoke-network.txt`, and
`artifacts/reports/runs/gate1/wpe-stress.txt`. The stress report recorded
seven views with 16 file descriptors before and after the repeated views.

**Next:** Test direct GPU frames on the i915 laptop before calling Gate 1
complete.

## 2026-08-24: WPE Process Lifecycle Check

**Changed:** The repeated-view check now terminates each web process and fails
if a sandboxed WPE descendant remains after teardown. Routine Sway and Vulkan
text diagnostics now save under `artifacts/reports/runs/`.

**Checks:** `dev.sh verify`, `dev.sh wpe-smoke`, `dev.sh wpe-smoke-network`,
`dev.sh wpe-stress`, and `dev.sh headless-smoke` passed.

**Evidence:** `artifacts/reports/runs/gate1/wpe-smoke.txt`,
`artifacts/reports/runs/gate1/wpe-smoke-network.txt`, and
`artifacts/reports/runs/gate1/wpe-stress.txt`. The stress report recorded seven
views, no remaining sandboxed descendants, and 20 file descriptors before and
after the repeated views.

**Next:** Run the direct GPU-frame test on the i915/Hyprland laptop. This host
does not expose i915.

## 2026-08-24: WPE Gate Hardening

**Changed:** Scoped sandbox verification to Fjord's descendant processes. The
network smoke test now fails on load errors or a final URI other than the one
requested. The stress check compares every post-baseline view against the
steady-state descriptor count.

**Checks:** `dev.sh verify`, `dev.sh wpe-smoke`, `dev.sh wpe-smoke-network`,
and `dev.sh wpe-stress` passed.

**Evidence:** `artifacts/reports/runs/gate1/wpe-smoke.txt`,
`artifacts/reports/runs/gate1/wpe-smoke-network.txt`, and
`artifacts/reports/runs/gate1/wpe-stress.txt`.

**Next:** Test direct GPU frames on the i915 laptop before calling Gate 1
complete.

## 2026-08-24: WPE Repeated-View Blocker

**Changed:** Moved routine WPE command output to `artifacts/reports/runs/` and
added `ripgrep` to the pinned development image. The repeated-view harness now
shares one WPE display and GLib context, releases each view, and checks for
descriptor growth after teardown.

**Checks:** `dev.sh image` passed and `rg --version` reports 15.2.0. The
in-process `dev.sh wpe-stress` check fails: it retains 23 file descriptors
after seven views, above the 17-descriptor steady-state baseline.

**Evidence:** `artifacts/reports/runs/gate1/wpe-stress.txt`.

**Next:** Identify and release the WPE resources retained across destroyed
views. Do not begin the i915 GPU-frame test until this check passes.

## 2026-08-24: WPE Steady-State Lifecycle Check

**Changed:** Corrected the repeated-view baseline to include WebKit's
long-lived Linux memory-pressure monitor. The monitor opens its permanent
`/proc` and cgroup files after its first five-second poll. The harness keeps it
enabled, waits for that initialization, then checks five further view lifecycles
against the steady-state descriptor count.

**Checks:** `dev.sh verify`, `dev.sh wpe-smoke`, `dev.sh wpe-smoke-network`,
`dev.sh wpe-stress`, and `dev.sh headless-smoke` passed.

**Evidence:** `artifacts/reports/runs/gate1/wpe-smoke.txt`,
`artifacts/reports/runs/gate1/wpe-smoke-network.txt`, and
`artifacts/reports/runs/gate1/wpe-stress.txt`. The stress report recorded seven
views with 23 file descriptors before and after the repeated views.

**Next:** Test direct GPU frames on the i915 laptop before calling Gate 1
complete.

## 2026-08-24: i915 DMA-BUF Preflight

**Changed:** Ran the standalone WPE lifecycle harness with the i915 laptop's
DRM devices available in the pinned container.

**Checks:** `dev.sh wpe-hardware` passed. WPE rendered and released an
`800x600` DMA-BUF from `/dev/dri/card1`; it reported two planes, modifier
`0x0100000000000004`, and 65 preferred format/modifier pairs. Sandbox
verification and web-process teardown also passed.

**Evidence:** `artifacts/reports/runs/gate1/wpe-hardware.txt`.

**Next:** Build the approved narrow GPUI Linux renderer path to import WPE's
DMA-BUF. The current WPE platform does not advertise explicit synchronization,
so document and validate the available synchronization primitive before import.

## 2026-08-24: DMA-BUF Plane Boundary

**Changed:** The WPE harness now duplicates dma-buf plane descriptors, offsets,
and strides into its plain C report before releasing WPE objects. The Rust
runner closes those duplicated descriptors after reporting.

**Checks:** `dev.sh verify` and `dev.sh wpe-smoke` passed locally.
`dev.sh wpe-hardware` passed on Athena with a two-plane i915 dma-buf.

**Evidence:** `artifacts/reports/runs/gate1/wpe-hardware.txt` on Athena.

**Next:** Import and sample the duplicated dma-buf with Vulkan/wgpu on Athena
before changing GPUI's renderer.

## 2026-08-24: Athena Vulkan Modifier Check

**Changed:** Checked Athena's direct-DRM Vulkan device for the extensions
required to import WPE's i915 CCS dma-buf modifier.

**Checks:** ANV exposes `VK_EXT_external_memory_dma_buf` and
`VK_KHR_external_semaphore_fd`, but does not expose
`VK_EXT_image_drm_format_modifier`.

**Evidence:** Athena direct-DRM `vulkaninfo` extension report.

**Next:** Negotiate a non-CCS WPE modifier that ANV can import, or take the
WPE Wayland subsurface fallback. Do not add a broad GPUI renderer fork for the
unsupported modifier path.

## 2026-08-24: Athena EGL DMA-BUF Import

**Changed:** Added an EGL image import probe for duplicated WPE dma-buf planes.
The probe imports each plane with its format, offset, stride, and modifier, then
destroys the EGL image before WPE descriptor cleanup.

**Checks:** `dev.sh wpe-smoke` passed locally. `dev.sh wpe-hardware` passed on
Athena with `egl_imported=true` for the two-plane CCS dma-buf.

**Evidence:** `artifacts/reports/runs/gate1/wpe-hardware.txt` on Athena.

**Next:** Add the approved narrow GPUI GLES renderer feature to sample the EGL
image directly. Keep the Vulkan path out of scope because ANV lacks
`VK_EXT_image_drm_format_modifier`.

## 2026-08-24: Multi-GPU Zero-Copy Policy

**Changed:** Replaced the i915-only GPU gate with a runtime-probed, multi-GPU
policy. GPUI imports supported dma-bufs directly. Unsupported format and
modifier combinations use a zero-copy WPE Wayland subsurface instead.

**Why:** The browser must work on normal Linux GPUs, not only the i915 laptop.

**Next:** Implement the runtime path probe and validate it on active Linux
hardware. Athena is optional compatibility coverage.

## 2026-08-24: GTX DRM Preflight Blocker

**Changed:** Made `dev.sh wpe-hardware` fail unless WPE produces and EGL imports
a dma-buf. The command now passes host DRM group IDs into the test container.

**Result:** The headless GTX 1650 host is not a usable WPE DRM target. Its
driver reports no PCI driver and rejects KMS buffer creation, so WPE times out
without a frame.

**Next:** Use any active Linux GPU with a working DRM stack for the hardware
gate. The GTX host cannot provide that evidence until its graphics driver and
KMS access work.

## 2026-08-24: GPUI Wayland Subsurface Protocol

**Changed:** Added a stock-GPUI smoke window that extracts the native Wayland
display and parent surface. The WPE boundary binds `wl_subcompositor`, creates a
desynced child surface, sets an empty input region, commits it, and releases it.

**Checks:** `dev.sh verify` passed locally. `dev.sh gpui-smoke` passed on
Athena's Hyprland session without stalling GPUI's event loop.

**Evidence:** Athena GPUI smoke output: `GPUI Wayland subsurface protocol
ready`.

**Next:** Attach a static dma-buf to the child surface and wait for
`wl_buffer.release` before connecting live WPE frame ownership.

## 2026-08-24: GPUI GBM DMA-BUF Subsurface

**Changed:** Generated the standard `linux-dmabuf` client protocol at build
time. The Wayland probe allocates a linear GBM dma-buf, attaches it to the
desynced GPUI child surface, detaches it, and requires `wl_buffer.release`.

**Checks:** `dev.sh verify` passed locally. `dev.sh gpui-smoke` passed on
Athena's Hyprland session.

**Evidence:** Athena GPUI smoke output: `GPUI Wayland subsurface protocol
ready` after the GBM buffer release check.

**Next:** Attach live WPE dma-buf frames to the GPUI-owned child surface and
release each WPE frame from `wl_buffer.release`.

## 2026-08-24: Live WPE DMA-BUF Subsurface

**Changed:** The GPUI Wayland smoke now creates a WPE headless view, imports
its live dma-buf into the GPUI-owned child surface, and returns the frame to
WPE only from `wl_buffer.release`. It repeats this release cycle for eleven
animated WPE frames while resizing the WPE toplevel ten times on the same child
surface.

**Checks:** `dev.sh verify` passed locally. `dev.sh gpui-smoke` passed on
Athena's Hyprland session.

**Evidence:** Athena GPUI smoke output: `GPUI Wayland live WPE subsurface
smoke ready`.

**Next:** Exercise two live WPE views with repeated switching.

## 2026-08-25: Alternating Live WPE DMA-BUF Subsurface

**Changed:** The GPUI Wayland smoke now keeps two deterministic WPE headless
views alive. It alternates the active view across 20 dma-buf release cycles on
one child surface. Hidden-view buffers return directly to WPE without a
Wayland attachment. Active buffers still return only after `wl_buffer.release`.
The existing repeated resize coverage continues for 19 of those cycles.

**Checks:** `./scripts/dev.sh verify` passed locally.

**Evidence:** Athena GPUI smoke output: `GPUI Wayland live WPE subsurface
smoke ready`.

**Next:** Implement basic forwarded pointer, scroll, keyboard, and focus input.

## 2026-08-25: Persistent WPE Subsurface Bridge

**Changed:** Replaced the app smoke path with an opaque, same-thread WPE bridge.
It creates one GPUI-owned child surface and one WPE headless view, then pumps
only pending private-queue and GLib events from GPUI animation frames. Live
dma-buf frames still return to WPE only from `wl_buffer.release`.

**Checks:** `./scripts/dev.sh verify` passed locally.

**Evidence:** Athena GPUI smoke output: `GPUI Wayland live WPE subsurface
bridge ready`.

**Next:** Implement basic forwarded pointer, scroll, keyboard, and focus input.

## 2026-08-25: Basic Pointer Buttons

**Changed:** The persistent GPUI WPE subsurface bridge now forwards root left
mouse press and release events to WPE at the GPUI event coordinates. The Rust
and C boundaries reject invalid input and report bridge errors without panics.

**Checks:** `./scripts/dev.sh verify` passed locally.

**Evidence:** Local build, test, and Clippy output from `./scripts/dev.sh verify`.
Athena startup smoke passed; interactive click confirmation remains pending.

**Next:** Manually confirm page clicks on Athena. Pointer movement, scrolling,
keyboard input, and focus remain unimplemented.

## 2026-08-25: Basic Scroll Forwarding

**Changed:** The persistent GPUI WPE subsurface bridge now forwards root scroll
events to WPE. Pixel deltas are precise; line deltas are non-precise. Both
boundaries reject invalid values and report failures without panics.

**Checks:** `./scripts/dev.sh verify` passed locally.

**Evidence:** Local build, test, and Clippy output from `./scripts/dev.sh verify`.
Athena startup smoke passed; interactive scroll confirmation remains pending.

**Next:** Manually confirm page clicks and scrolling on Athena. Pointer movement,
keyboard input, and focus remain unimplemented.

## 2026-08-25: Character-First Keyboard Forwarding

**Changed:** The root GPUI view now takes focus after a pointer press and forwards
printable ASCII characters plus Enter, Backspace, Tab, Escape, and arrow keys to
WPE. The bridge creates WPE keyboard down/up events with `keycode=0`. Ctrl, Alt,
and platform-key combinations stay in GPUI. Named keys use xkbcommon keysyms.

**Checks:** `./scripts/dev.sh verify` passed, including the printable and named
key mapping unit test.

**Evidence:** Local build, test, and Clippy output from `./scripts/dev.sh verify`.
Athena startup smoke passed; interactive keyboard confirmation remains pending.

**Next:** Interactively confirm text entry, named keys, focus after click, and
GPUI shortcut preservation on Athena.

## 2026-08-25: Input Development Fixture

**Changed:** Replaced the bridge's minimal animated page with a development-only
input fixture. It shows a click target, text field, scrollable page, and visible
log for pointer, text, key, wheel, and page-scroll events.

**Checks:** `./scripts/dev.sh verify` passed.

**Next:** Use the fixture for the interactive Athena input checklist.

## 2026-08-25: Visible WPE Subsurface Frame

**Changed:** The persistent bridge now keeps the active WPE dma-buf attached to
the child surface until a later frame replaces it. The development fixture is
therefore visible instead of leaving a transparent GPUI window.

**Checks:** `./scripts/dev.sh verify` passed locally. Athena startup smoke
passed.

**Next:** Interactively validate the visible input fixture on Athena.

## 2026-08-25: GPUI Bridge Trace

**Changed:** `dev.sh gpui-smoke` now saves a Gate 2 report under
`artifacts/reports/runs/gate2/`. The bridge logs frame ownership and forwarded
input counters to isolate stalls without relying on visual guesswork.

**Next:** Reproduce the interaction stall on Athena and inspect the saved
bridge trace.

## 2026-08-25: Headless Buffer Ownership Blocker

**Discovery:** The WPE headless view releases its prior buffer internally on
every new frame before the child compositor surface can safely release it.
This makes the current live headless bridge inherently prone to flashing or
stalling; input counters confirm forwarding continues while presentation stops.

**Decision:** Do not tune this bridge further. Replace the headless view with a
custom Fjord WPE platform view that holds each dma-buf until
`wl_buffer.release`.

**Next:** Timebox a custom WPE platform subclass spike in `crates/webkit`.

## 2026-08-25: WPE Subsurface Frame Pacing

**Changed:** The persistent WPE dma-buf subsurface bridge now waits for the
child surface's Wayland frame callback before attaching a replacement. It keeps
one newest queued WPE buffer, returns superseded queued buffers immediately,
and still returns attached buffers only from `wl_buffer.release`.

**Checks:** `./scripts/dev.sh verify` passed locally.

**Evidence:** Local build, test, and Clippy output from `./scripts/dev.sh verify`.
Athena startup smoke passed; interactive smoothness confirmation remains pending.

**Next:** Interactively confirm smooth fixture updates on Athena.

## 2026-08-25: Compositor-Owned WPE Buffers

**Changed:** Replaced the live bridge's headless WPE view with a small Fjord
platform display and view. The view caches one Wayland buffer per WPE dma-buf,
reports frames from the child surface callback, and returns buffers only after
Hyprland releases them. One newest resize frame waits when GPUI's shared
Wayland queue has not delivered the prior frame callback yet. Resize requests
are coalesced in the bridge pump so WPE is not flooded during a continuous
drag. Wayland callbacks run before a bounded WebKit work slice so neither queue
starves the GPUI thread. The display advertises mouse and keyboard input. GPUI
viewport changes now resize the WPE view and child surface. Keyboard repeat is
limited to about 25 Hz. Each accepted key is sent as an immediate down/up pair,
so repeat remains usable without building a post-release WPE input backlog.
`Ctrl+Backspace` and `Alt+Backspace` carry their WPE modifier flags for word
deletion; other Ctrl/Alt/platform shortcuts remain in GPUI.

**Checks:** `./scripts/dev.sh verify` passed locally. The Athena i915/Hyprland
smoke remained live through grouped-tab resize and continued handling keyboard
input afterward. The final run reached 305 rendered frames, 306 attached
frames, and 304 compositor releases without a bridge failure. A long-hold key
smoke continued rendering with matched key down/up counters after release.

**Evidence:** `artifacts/reports/runs/gate2/gpui-smoke.txt`.

**Next:** Exercise the two-tab swap gate through the custom platform view.
