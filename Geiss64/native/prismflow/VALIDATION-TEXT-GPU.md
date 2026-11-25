# Native text-mask state and GPU stamping

Observed 2026-11-16 during D33. `gpu_text.cpp` now uploads RGBA8 masks, retains
their logical supersample size, controls timed/hidden/immediate visibility and
executes the unchanged text-overlay WGSL into RGBA16F feedback. The original
shader writes additive intensity to all four channels; its older R/G-only
comment does not describe the actual shader/blend state. Native code preserves
the actual RGBA behavior.

This component accepts rasterized pixels. Native font rasterization now lives
in `VLC-Visualizer-Addon/dawn/text_raster.*`; see its `VALIDATION-FONTS.md` for
separate native glyph and GPU checks. The original-browser masks here remain
development fixtures, not a runtime fallback or an installed asset. Native image decoding/palette helpers, full
controller integration, scene parity and production dispatch also remain open.

## Ownership and behavior

Each helper owns one current mask, one transparent fallback texture, one
sampler, one 32-byte uniform buffer, one pipeline and one bind group. Upload
dimensions and byte counts are bounded before allocation, supersample is
1..8, and the existing device/16,777,216-texel budget applies. Same-sized uploads
reuse the texture; changing logical supersample preserves the actual pixels.
Replacing a mask releases old references after submitted work retires.

Clear/hide/show retain the original geometry/timing state where applicable.
The presenter receives the transparent fallback while hidden or empty; direct
stamping remains available for a loaded mask independently of presenter
visibility, as in the original. Stamping is one operation per submitted frame,
with no CPU readback/wait in production encoding. CPU placement and fade
calculations retain double precision until uniform upload.

## Local original-renderer oracle

The local harness supports `--case text`, running the original
`GpuTextOverlay` methods and shader. It supplies synthetic masks and captures
two original-canvas masks (Latin and Unicode) solely as GPU test inputs.
Diagnostic `COPY_SRC` usage permits mask readback. The original font rasterizer
is neither ported nor claimed by this comparison. Font-dependent mask sizes
differ between the machines, which is recorded in the byte counts below.
The server stays on loopback, external requests are blocked, sandboxing stays
enabled and original source hashes/actual adapter are recorded. No Electron
dependency is introduced into native code or final payloads.

Fifteen cases include empty state, before/at/during/after fade-in, hidden direct
stamping, clearing, showing without a mask, clipped/fractional placement,
same-texture rescaling, negative/over-one stamp intensity and supplied glyph
masks. They check mask uploads, actual original uniform writes, texture reuse,
presenter-view selection, logical rectangles, timing and raw output pixels.
The CPU placement/timing difference is zero on all listed runs; 96 uniform
components match the captured original values.

| Native / original-renderer path | Exact uploaded mask bytes | Different output components / 921,600 | Maximum output error |
| --- | ---: | ---: | ---: |
| Windows RTX 3070 Ti Laptop D3D12 / same adapter | 129,784 | 0 | 0 |
| Mac M1 Metal / same adapter | 133,996 | 0 | 0 |
| Linux GTX 1660 Ti NVK / common Windows NVIDIA fixture | 129,784 | 0 | 0 |
| Linux lavapipe / common Windows NVIDIA fixture | 129,784 | 3,383 | 0.0029296875 |
| Linux SwiftShader ASan/UBSan/leak detection / same adapter | 139,392 | 0 | 0 |
| Linux lavapipe ASan/UBSan/leak detection / common Windows fixture | 129,784 | 3,383 | 0.0029296875 |

The general maximum/mean limits were fixed before results at 0.004 / 0.0002.
Lavapipe's mean difference is 3.3488e-6. Exact mode requires zero differing
output bits; Windows, Metal, NVK and sanitizer SwiftShader pass that check.
The passing lavapipe stamp comparison does not convert the earlier failed
feedback/palette-title comparisons into passes. No same-adapter original
lavapipe capture is claimed.

## Confirmed source timing faults

The oracle demonstrates that `get_intensity()` returns NaN when a zero-duration
interval is queried at its start. It also returns NaN after the original
three-argument `show_overlay()` call omits timing fields (the existing
`GPUWarp.show_overlay_text()` wrapper makes that call). These invalid timing
states are captured as explicit expected source faults without submitting
NaN uniforms to the GPU.

Native timed display requires a finite positive interval and nonnegative finite
fade power. `showImmediate()` explicitly provides constant finite intensity
for display without an interval. Regression checks prove the invalid interval
is rejected and immediate visibility stays finite. Ordinary timed rendering,
placement and GPU output remain compared to the original. These findings do
not establish the cause of the historical system crash.

## Bounded lifecycle evidence

Forty construction/upload/hide/clear/reuse/draw/destruction cycles pass on every
listed path. A deliberately retained 32,768-byte texture must increase live
texture accounting before release. After submitted work and deferred deletions
retire, live texture usage returns to the cold baseline and total live GPU
bytes do not grow after warmup. Samples at cycles 8/24/40 retain Windows 434
handles, Metal 8 descriptors, NVK 9, lavapipe 6 and sanitizer software 6.
Ordinary Windows/NVK/lavapipe sampled RSS stays constant; Metal changes by
32 KiB. Sanitizer RSS is not constant, and no universal process-memory claim
is made. Linux ASan/UBSan/leak detection reports no defect in these checks;
linked shared Dawn/driver libraries are not instrumented.

The optional text CTest passes in exact mode on Windows, Metal and NVK. All
seventeen tracked WGSL modules remain byte-for-byte verified. With this pass,
all seven compute and all sixteen render-format variants now have native
execution coverage. That closes the shader-execution inventory, not D33:
native image services now have separate evidence in the Addon's
`dawn/VALIDATION-IMAGES.md`; complete renderer/controller/host integration remains.
Longer full-renderer/player/installer and HDR checks stay on the release list.

## Reproduction

From Geiss64, using the existing owner-local preserved development runtime:

```text
<local-electron> tests/prismflow_feedback_reference.cjs --case text --assets assets/prismflow --output build/text-reference
```

Build `prismflow_text_probe` in the addon workbench, then run:

```text
prismflow_text_probe --assets ../Geiss64/native/prismflow/shaders --fixtures <local-reference>/fixtures.json --output build/text-native --exact --cycles 40
```

Optional `--backend d3d12|metal|vulkan` selects a backend. Inspect actual adapter
metadata; Linux's software original-renderer capture used SwiftShader. Select
its local ICD with per-child `VK_DRIVER_FILES` for a matched native comparison.
Omit `--exact` only for explicitly bounded comparisons such as lavapipe versus
NVIDIA; the fixed numerical limits still apply.

Configure `PRISMFLOW_TEXT_FIXTURES=<absolute-local-path>/fixtures.json` and
optionally `PRISMFLOW_TEXT_EXACT=ON` to register
`dawn_prismflow_text_reference`. No reference runtime is launched or fixture
downloaded by native CTest/builds. All masks, profiles and reports remain ignored
local development data. No external analysis service or security/driver-setting
change is involved.
