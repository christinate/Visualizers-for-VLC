# Native Prism Flow GPU port validation

Observed 2026-11-16. D33 remains open. The pipeline and dynamic-map milestones
below validate compilation and actual procedural-noise/dynamic-motion passes.
They do not establish complete Prism Flow frame parity, feedback rendering,
visual quality, keyboard behavior, HDR presentation or release readiness.

The source-built shared Dawn revision is
`7daf3b0bee2c73621001ab15b256b5a84fb0b45f`, matching the addon workbench.
All analysis and generated reports stay on the owner's machines.

## Preserved shader assets

`tools/extract_prismflow_shaders.mjs` imports the preserved reference modules
locally and writes their assembled WGSL into tracked `shaders/*.wgsl` files.
Private constants are exported only in the tool's in-memory modules. The
reference JavaScript files are unchanged. The two procedural-noise shaders
come from the reference's own generator, using a device stub that captures
its shader text and pipeline description rather than executing graphics.

The 17 modules and manifest total 87,010 bytes. Each shader retains the
upstream notice preamble; retain this directory's LICENSE/NOTICE/OUTPUTS files.
`shaders/manifest.json` records imported source hashes, output hashes and
pipeline entry points/formats. Extraction is a development step: native
builds load these WGSL files directly and require no Node/browser runtime.

| Shader modules | Pipeline role |
| --- | --- |
| `warp` | RGBA16F feedback warp |
| `auto_exposure` | Exposure statistics compute |
| `dynamic_35`, `dynamic_37`, `dynamic_38`, `dynamic_39` | RG16F dynamic map rendering |
| `noise_2d`, `noise_3d` | RGBA16F procedural noise compute |
| `warp_dots` | Particle advection compute |
| `bokeh_advection`, `bokeh_deaths`, `bokeh_stats` | Bokeh lifetime/advection/statistics compute |
| `bokeh_draw` | Additive RGB bokeh rendering |
| `dots`, `text_overlay` | Additive feedback drawing |
| `present`, `palette_overlay` | Final presentation and palette overlay |

The shared renderer pipeline bank creates the reference's vertex/instance layout for dots, additive
blend states, RGB-only bokeh write mask, and common explicit seven-binding
layout for dynamic maps. Presentation, bokeh and palette-overlay pipelines
are created for RGBA16F, RGBA8 and BGRA8. These make 16 render and seven compute
pipelines. Shader compilation diagnostics and pipeline validation scopes are
checked using tracked, heap-owned asynchronous callback state.

## Observed results

| Machine/backend | Adapter/driver | Modules / render / compute | Result |
| --- | --- | --- | --- |
| Windows D3D12 | RTX 3070 Ti Laptop, 32.0.16.1060 | 17 / 16 / 7 | pass |
| macbook1 Metal | Apple M1, macOS 26.6.2 (25G83) | 17 / 16 / 7 | pass |
| mint Vulkan hardware | GTX 1660 Ti NVK, Mesa 25.2.8 | 17 / 16 / 7 | pass |
| mint Vulkan software | llvmpipe LLVM 20.1.2, Mesa 25.2.8 | 17 / 16 / 7 | pass |

The Linux software check sets `VK_DRIVER_FILES` for that child process only;
installed drivers and display settings are unchanged. NVK checks are offscreen;
the previously documented non-DRI3 X-session presentation limitation still
applies. Dawn's dynamic-buffer-limit warnings on Linux do not fail these
pipelines; these shaders do not require the larger reported offset counts.

The initial Windows targeted CTest checks passed 2/2: native pipeline creation and
byte-for-byte reference extraction. Other existing foundation tests are
unchanged; they were not repeated for this isolated shader/pipeline addition.

Build/run from the addon checkout after configuring its pinned Dawn workbench:

```text
cmake --build build/dawn-check-source --target prismflow_gpu_probe --config Release
ctest --test-dir build/dawn-check-source -C Release -R dawn_prismflow --output-on-failure
```

On Mac/Linux use the existing `build/dawn-check-native` tree. Configure again
after adding this target to a Unix Makefiles build. The standalone probe takes
`--assets <Geiss64/native/prismflow/shaders>`, `--output <local-directory>` and
optional `--backend d3d12|metal|vulkan`. Reports identify the actual adapter.

## Native noise and dynamic-motion execution

`gpu_support.cpp` now owns a bounded shader/pipeline bank, checked texture/
buffer creation, bindings and diagnostic half-float texture copies.
`gpu_dynamic.cpp` executes the two noise compute passes and the four dynamic
motion render passes with their unchanged WGSL. Each map instance has its own
96-byte uniform buffer and bindings. One engine-frame call refreshes dynamic
noise once, deduplicates repeated state/view pairs, and renders each requested
map. No CPU readback or wait is added to frame encoding. Readback belongs to
the diagnostic probe only.

The noise resources retain the reference dimensions: 64x64x16 static RGBA16F,
256x256 static RGBA16F, and 64x64 dynamic RGBA16F, plus a 512x1 R16F spectrum.
Spectrum clipping, nonfinite-to-zero conversion, signed zero and half rounding
are checked against the reference's actual upload method. Uniforms are captured
from the reference's actual encode method, without running browser APIs.
Inputs/fixtures and raw output remain local and are excluded from installers.

The first noise oracle incorrectly assumed texture stores used the same
nearest-even rounding as CPU uploads. Its first D3D12 mismatch was one bit
(`13562` versus `13563`). Instrumenting only the diagnostic storage format as
RGBA32F confirms the full-precision shader hash matches every predicted
component. The probe then checks each RGBA16F component against an exact
conversion of that verified float32 value, and requires a consistent nearest-
even or toward-zero policy per texture. All four tested paths use toward-zero
noise storage. The production WGSL remains unchanged; this is not a general
one-ULP tolerance or a claim about every GPU format's rounding.

| Path | Exact noise hash / verified stored components | Exact uniforms / spectrum | Finite motion components |
| --- | ---: | ---: | ---: |
| Windows D3D12 | 622,592 | 288 / 512 | 8,388,608 |
| macbook1 Metal | 622,592 | 288 / 512 | 8,388,608 |
| mint NVK Vulkan | 622,592 | 288 / 512 | 8,388,608 |
| mint lavapipe Vulkan | 622,592 | 288 / 512 | 8,388,608 |
| Linux ASan/UBSan/leak detection, lavapipe | 622,592 | 288 / 512 | 8,388,608 |

Four frame states distinguish silence, changed audio, changed time and a
repeated time/audio input. Modes 35/37/38 animate with time and remain unchanged
by audio alone; mode 39 changes with spectrum and remains unchanged by time
alone. All outputs are finite. Empty frames do not refresh noise; duplicate
bindings do not advance it twice; uint32 seed wrap is checked. Nonaligned rows,
3D layers, R16F/RG16F/RGBA16F copies, bad dimensions, invalid shader/format
requests, malformed states and rejected-draw seed preservation are checked.
These are behavior and data checks, not full reference-image comparisons.

The shared CPU half converter had a confirmed defect: `2^-24` became zero
instead of half bits `0x0001`, and midpoint ties rounded upward instead of to
even. Its corrected conversion passes all 63,488 finite half round trips and
all 31,743 adjacent-value midpoint boundaries, with 126,972 signed-tie/neighbor
checks. Existing saturation and nonfinite policies are retained. See the
addon's `dawn/VALIDATION-PRECISION.md` for regressions and exact limitations.

The new native GPU path and converter also pass Linux ASan/UBSan with leak
detection enabled and no sanitizer/leak report in these bounded checks.
All 23 pipeline variants are now tested through this same renderer bank on
all four paths. Affected Terrain reference, RGBA16F/pixel and relinked native
plugin checks pass on Windows/Mac/Linux, including Linux software pixels.

From the addon checkout, build `prismflow_dynamic_probe` and run the development
CTest `dawn_prismflow_dynamic_reference` when Node is available. Or generate
common fixtures with `Geiss64/tests/prismflow_dynamic.mjs --fixtures-only
<directory>` on the development machine, then use the native probe's
`--assets`, `--fixtures <directory>/fixtures.json`, `--output` and optional
`--backend` arguments on a target with no Node installation.

Native feedback/cache and exposure execution now have separate evidence in
[VALIDATION-FEEDBACK.md](VALIDATION-FEEDBACK.md). Remaining D33 work includes
other pass execution, image/text replacements and render-controller integration. D34/D35 still
require complete scene/control and deterministic audio/image comparisons.
HDR remains Experimental, off by default, with native capability/fallback work
tracked separately in D37. No physical HDR display result is claimed.
