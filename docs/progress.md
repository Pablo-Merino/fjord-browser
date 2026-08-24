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
