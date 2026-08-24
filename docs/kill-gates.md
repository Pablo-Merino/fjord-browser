# Fjord Technical Gates

Fjord must pass these gates before it becomes a browser product. A passed build
is not enough. Each gate needs working behavior and saved evidence.

## Test Setup

| Part | Current choice |
| --- | --- |
| Browser interface | GPUI on Wayland |
| Web engine | WPE WebKit 2.52.x from Arch Linux |
| Build environment | Pinned Arch Linux container on the headless host |
| Hardware test machines | Omarchy i915 laptop and GTX 1650 host |
| Early UI tests | Headless Wayland with software graphics allowed |

The progress log records the commands, results, and saved evidence. Exact
versions belong in the evidence when they matter.

## Gate 1: WPE Works Alone

**Question:** Can WPE WebKit create, run, and destroy a web view safely?

It must:

- Load local HTML, JavaScript, and `https://example.com`.
- Report the page address, title, loading state, and rendered frames.
- Report available frame types, including shared-memory and GPU-buffer details.
- Use WPE's process sandbox.
- Survive a webpage-process crash without killing Fjord.
- Create and destroy repeated views without leaking processes, buffers, or files.

**Proof:** automated lifecycle tests, short redacted logs, and a frame report.

**If it fails:** fix WPE setup and ownership first. Do not build GPUI browser
chrome around an unproven engine.

## Gate 2: GPUI And WPE Share Frames

### Headless Check

**Question:** Can GPUI and WPE run together without stale frames, leaks, or a
blocked interface?

It must:

- Show three WPE views and switch between them correctly.
- Keep only the newest pending frame.
- Release old frames quickly.
- Handle repeated resize without freezing or growing memory without limit.
- Stay responsive while pages load.

A temporary software or shared-memory path may prove this behavior. It is not a
shipping renderer.

### Hardware Check

**Question:** Can Fjord display WPE GPU frames without CPU copies on normal
Linux GPU stacks?

It must:

- Probe WPE's actual format and modifier at startup.
- Import the GPU buffer into GPUI without a CPU copy when the renderer supports
  it.
- Use a Wayland subsurface when direct import is unsupported. The compositor
  must import the buffer without a CPU copy.
- Respect the synchronization primitive available for the selected path.
- Render correct size, colors, and transparency.
- Switch three live views without stale content.
- Keep any GPUI change limited to one small Linux renderer feature.
- Exercise the i915 laptop and GTX 1650 host. Untested GPUs must use the
  runtime probe and select a working zero-copy path instead of crashing.

**Proof:** frame and synchronization counters, reviewed screenshots, and
recorded runs for each hardware test machine.

**If it fails:** first fix the small GPU import path. Then use a clean WPE
Wayland subsurface for that GPU. If neither path works without a CPU copy, stop
and reconsider GPUI as the host.

## Gate 3: Input Goes To The Right Place

**Question:** Can Fjord controls and webpage controls share input correctly?

It must:

- Send pointer movement, clicks, scrolling, and keyboard input to the active
  webpage.
- Move focus between the page, address field, sidebar, and popups correctly.
- Keep browser shortcuts out of webpage text fields.
- Support selection and text, HTML, and image clipboard data.
- Support fcitx, the text-input system used on the laptop.
- Release input ownership when a tab closes or crashes.

**Proof:** focused tests, a deterministic test page, and a laptop checklist for
fcitx, clipboard, focus, and scrolling.

**If it fails:** fix the shared input boundary. Do not add one-off fixes to
individual controls or websites.

## Review Point

Pause after all three gates pass. The review build must show:

- A left vertical tab sidebar with expanded and collapsed modes
- Three live web views
- A toolbar with Back, Forward, Reload/Stop, and address controls
- A working zero-copy GPU path on i915 and GTX 1650
- Working pointer, keyboard, scrolling, clipboard, focus, and fcitx input

Only then can Fjord move into the [personal alpha](mvp-alpha.md).
