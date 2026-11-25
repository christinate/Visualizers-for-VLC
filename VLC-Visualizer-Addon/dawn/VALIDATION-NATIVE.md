# Native windows, analyzer and Terrain controller

Continuation of [the initial foundation validation](VALIDATION.md), observed
2026-11-15. This is development evidence. Production dispatch and installers
still use the preserved candidate until the migration checklist is complete.

## Window and GPU lifecycle

Five app-owned SDL event cases pass on each tested path:

| Platform | Presented backend | Results |
| --- | --- | --- |
| Windows 11 / RTX 3070 Ti Laptop | D3D12 hardware | 5/5 |
| macOS 26.6.2 / Apple M1 | Metal hardware, source-built Dawn | 5/5 |
| Mint 22.3 / llvmpipe | Vulkan software / lavapipe | 5/5 |

Cases cover resizing and two fullscreen toggles with exact window-bound
restoration, fullscreen Escape closure, fullscreen right-click closure,
fullscreen nonce-protected menu closure, and Dawn `ForceLoss` delivering a
controlled error/exit. Normal cases present at least 90 frames, exercise all
four accelerated camera modes, hold at most two pending frame completions,
and release their window-control registration/request files. Device-loss
cases return the injected error and also clean up those files.

The native event loop polls completion without blocking for GPU work. A
surface timeout/reconfigure path also tracks already-submitted scene work;
repeated lost/outdated surfaces are bounded to three retries. Texture views
and compositor bindings are reused until their source changes. Rendering
targets check device texture limits and a 16,777,216-pixel budget.

Mint's NVK hardware adapter passes offscreen rendering, but its Xorg display
has DRI2 and no DRI3. Attempting NVK swapchain presentation caused an X I/O
termination. The new preflight reports that specific unsupported combination
before swapchain configuration. The software test selects the existing
lavapipe ICD only in the test child's environment; no system graphics
configuration or driver was modified. Wayland has not been exercised.

These are real native windows/surfaces driven by internal test events, not
physical keyboard/mouse or integrated media-player tests. They do not prove
indefinite stability or absence of every driver/resource leak.

## Numeric reference evidence

The native analyzer receives exactly the same float32 stereo PCM bytes as
`browser/ui/audio.mjs`: 612 windows, including 16 full waveform/spectrum
comparisons, silence, six tones, above-full-scale finite samples, opposite
channels, nonfinite inputs, tiny values, deterministic noise and a 600-window
rhythm sequence. All three platforms pass. The rhythm sequence detects 40
beats; maximum observed absolute numeric difference is
`2.220446049250313e-16`. Nonfinite channel samples are explicitly zeroed.

Linux also passes these fixtures with address and undefined-behavior
sanitizers, `detect_leaks=1`, and halt-on-error. This covers the analyzer and
diagnostic code; it does not instrument the whole graphics driver or final
plugin/capture paths.

The native Terrain controller matches 24 browser-controller fixtures,
including repeated history ticks, camera transitions, changing camera/color
intervals and manual palette changes. All half-float history rows match
exactly, and the compared camera/palette/uniform values match exactly on all
three systems. A missing peak-luminance uniform was found by this test and
corrected, even though SDR shading did not use it.

## Owner-requested ring removal

The repeated circular arches were decorative geometry in `terrain.wgsl`.
Their union with the terrain field has been removed. The same file is used
by the browser renderer and native scene. Windows D3D12, Mac Metal and Linux
hardware Vulkan offscreen checks all pass with the ring-free shader, including
four camera fixtures and the waveform/silence image comparison. Windows
readback previews were inspected locally. The original release artifacts
remain preserved; new packages will include this source change.

## Runtime and notice preparation

Windows Dawn and Abseil now use matching static C runtimes. Dependency
inspection of the window executable, Dawn DLL and SDL DLL shows no
MSVCP/VCRUNTIME redistributable DLL imports. WebGPU C handles/callbacks are the
module boundary; allocations are freed by the module that created them.

Before the API 4/capture additions, measured Windows workbench components were
508,928 bytes (window executable),
7,318,528 (Dawn DLL), 1,586,176 (SDL DLL), and 4,741,488 (SDK compiler DLL):
14,155,120 bytes combined. This excludes remaining engines, assets, settings,
capture, player bridges and installers, and is not a final installer-size
claim.

Source builds pass on Windows, Mac and Linux. Provenance collects dependency revisions/hashes and
top-level plus tracked vendored notices; final runtime notice staging and
the remaining release checks are still open.

Raw reports, generated synthetic fixtures and build logs stay in ignored
build folders on the owner's machines. No diagnostic samples were submitted
to third-party analysis services.

## Subsequent API, capture and pixel checks

[Native API 4](NATIVE-API.md) now passes C11/C++ layout, shared-runtime
identity, negative-loader, module/direct-scene output and failed-initialization
callback-lifetime checks on all three platforms. A queued map callback after
failed plugin initialization exposed a real lifetime defect: releasing only
the instance could leave the callback and its retained buffer undelivered.
Tracked futures are now completed/cancelled before module release. Pending
unresponsive callbacks pin their module until the isolated process exits.

Native recording checks now pass on Windows WASAPI, Linux PulseAudio and Mac
CoreAudio, with float stereo at 44.1 kHz and no FIFO drops. Native dialog,
format-conversion and device-removal evidence, the reproduced PulseAudio
shutdown hang/fix, and the remaining integrated Mac permission check are
recorded in [VALIDATION-INPUT.md](VALIDATION-INPUT.md). No audio is saved.

The reported Linux diagonal was reproduced in raw GPU pixels and corrected;
[VALIDATION-PIXELS.md](VALIDATION-PIXELS.md) records the isolation, all-channel
regression test, hardware image comparisons and remaining display limits.

The Windows shader-compiler DLL now matches its SDK 10.0.26100.0 source by
SHA256; that SDK's license and third-party notices are collected with their
hashes. Together with the pinned source builds and dependency provenance on
all three systems, this completes the source-dependency preparation in D17.
Final payload notice staging/redistribution audit and release packaging remain
separate open release gates.
