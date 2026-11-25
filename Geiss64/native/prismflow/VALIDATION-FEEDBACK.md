# Native feedback and cache validation

Observed 2026-11-16 on the native migration branch. This is D33 feedback,
exposure and map-cache evidence, not full Prism Flow scene parity, production
dispatch, HDR presentation or release readiness. All fixtures, profiles and
reports remain in ignored build directories on the owner's machines.

Later complete-scene comparisons against original JavaScript on the same
pinned native Dawn backend are in
[VALIDATION-NATIVE-REFERENCE.md](VALIDATION-NATIVE-REFERENCE.md). NVK and
lavapipe both match those 36 frames exactly. The original-to-original
cross-backend first-frame comparison reproduces the complete-scene failure;
the separate cross-adapter feedback fixture failures below remain retained.

## Implementation

`gpu_feedback.cpp` owns two RGBA16F feedback textures, four original Gaussian
noise textures, exposure/uniform buffers and twenty preallocated 512x512 RG16F
map textures (20 MiB). Its LRU cache protects all eight bound transition slots.
Map identities are monotonic and thread safe; copies of immutable map data keep
the identity, while new/rebuilt maps get a fresh identity. The cache keeps no
CPU pixel arrays. Dynamic entries own separate uniforms and share the native
noise engine already checked in VALIDATION-GPU.md.

Frame encoding prepares dynamic maps, computes exposure and performs the warp
before one submission. It exposes the encoder and output view for subsequent
particle/wave/text passes. Phase checks reject binding, upload or resize during
an open frame. Abandoning an encoded frame preserves the displayed feedback
texture; it does not promise to rewind random-number/noise-seed consumption.
Resize replaces feedback while keeping cached maps/noise. Dimensions are
validated against device limits and a 16,777,216-texel budget before allocation.
Production encoding adds no readback or CPU wait.

## Preserved-renderer comparisons

`tests/prismflow_feedback_reference.cjs` runs the unchanged original `GPUWarp`
and WGSL using the already preserved, local Electron 45.3.3 development runtime.
It serves whitelisted source files on loopback, blocks external session
requests, retains browser sandboxing and disables background networking.
It records source hashes and the actual adapter. No dependency is downloaded
by this test. Electron/Node are development oracles, not native dependencies
or installer payload additions.

The scenes use synthetic inputs, empty waveforms and disabled particle spawning
to isolate feedback/exposure. They include ten static/dynamic maps, duplicate
dynamic modes with distinct seeds, eight bound slots, shader effects, SS1/SS2,
sixteen frames, two portrait/landscape resizes and twenty-four protected LRU
evictions. The native probe uses native-generated CPU maps, never reference
pixels in place of those maps. Each run checks:

- 2,097,248 CPU map components (zero error on these fixtures).
- 1,048,576 exact Gaussian upload bytes and 576 exact uniform components.
- 365,568 half-float feedback components, exposure values and cache assignments.
- Map identity, invalid/pruned input, phase guards and abandoned-frame behavior.

| Native / reference path | Half components with different bits | Maximum absolute error | Maximum exposure error | Result |
| --- | ---: | ---: | ---: | --- |
| Windows RTX 3070 Ti Laptop D3D12 / same adapter | 0 | 0 | 0 | exact pass |
| Mac M1 Metal / same adapter | 0 | 0 | 0 | exact pass |
| Linux SwiftShader Vulkan / same adapter | 0 | 0 | 0 | exact pass |
| Linux GTX 1660 Ti NVK / Windows NVIDIA reference | 48 | 0.00048828125 | 5.96e-8 | cross-adapter pass |
| Linux SwiftShader ASan/UBSan / same adapter | 0 | 0 | 0 | exact pass |

The cross-adapter limits were fixed before observing results: maximum pixel
error 0.002, mean error 0.0002 and exposure error 0.000001. `--exact` additionally
requires zero differing feedback bits and zero exposure error. These limits
were not loosened after failures. Metal initially exceeded the NVIDIA pixel
limit (frame 1 maximum 0.00341796875), then matched its own original renderer
exactly. This establishes backend variation for that Metal comparison.

Lavapipe compared to the NVIDIA reference still exceeds the pixel limit:
frame 0 maximum 0.00390625, mean 0.00006164175768693288, with 5,917 differing
components. The original browser's explicit Vulkan run selected SwiftShader,
not lavapipe; that result is labelled by its actual adapter. Lavapipe does not
have a same-adapter original-renderer comparison here. Its earlier shader,
dynamic-map and noise checks pass, but complete image parity remains open.
Failed reports are retained, and this document does not treat that pixel check
as passing.

## Bounded cleanup checks

Forty create/resize/abort/render/destroy cycles plus forty deliberately failed
partial constructors pass on Windows D3D12, Mac Metal, Linux NVK and SwiftShader.
The tests wait for submitted work, retire deferred deletions and inspect Dawn's
live GPU allocation accounting. A deliberately retained 32,768-byte texture
must increase accounting before its release, providing a positive control.
Live texture bytes return to the cold baseline; total live GPU bytes do not
grow after warmup. Handle/descriptor counts remain constant between cycles
8, 24 and 40 (Windows 428, Mac 8, NVK 9, SwiftShader 6).

SwiftShader's resident process memory rises about 5 MiB between those ordinary
samples despite stable live GPU allocation counts. Driver/compiler/allocator
retention is not classified as a demonstrated leak by this bounded test.
Linux ASan/UBSan with `detect_leaks=1` also passes the exact SwiftShader
comparison and these lifecycle checks with no sanitizer report. The new C++
renderer/core is instrumented; the linked shared Dawn library and Vulkan
drivers are not. This is not proof of indefinite stability or absence of all
leaks, and longer/full-renderer soaks remain checklist work.

An explicitly separate lavapipe lifetime-only run also passes forty cycles and
forty partial failures under ASan/UBSan/leak detection. Live GPU bytes stay 22,
live texture bytes stay at the 6-byte driver baseline, and descriptor count
stays 5. Resident memory increases from 234,835,968 to 314,073,088 bytes between
cycles 8 and 40 with sanitizers enabled; no sanitizer/leak report occurs.
This observation is retained without claiming stable process RSS. It does not
convert the failed lavapipe/NVIDIA image comparison into a pass.
The corresponding ordinary lavapipe lifetime-only run passes with the same GPU
and descriptor counts; RSS changes from 113,287,168 to 113,311,744 bytes (24 KiB)
between those samples. The final optional feedback CTest also passes on NVK
using the common NVIDIA fixtures with exact mode explicitly disabled.

## Reproduction

From the Geiss64 checkout, run the preserved local runtime explicitly:

```text
<local-electron> tests/prismflow_feedback_reference.cjs --assets assets/prismflow --output build/feedback-reference
```

The Windows `--high-performance-gpu` switch selected NVIDIA on the test laptop.
On Linux `--vulkan-reference` permits a software WebGPU adapter without disabling
sandboxing. Always inspect `reference-status.json`: forcing a Vulkan driver in
the environment does not establish which adapter the browser actually chose.
SwiftShader's local ICD can be selected for the native child via
`VK_DRIVER_FILES=<preserved-runtime>/vk_swiftshader_icd.json`. No driver or OS
security setting is changed by these per-process selections.

From the addon checkout, build `prismflow_feedback_probe`, then run:

```text
prismflow_feedback_probe --assets ../Geiss64/native/prismflow/shaders --fixtures <local-reference>/fixtures.json --output build/feedback-native --exact --cycles 40
```

Omit `--exact` only for an explicitly cross-adapter comparison; fixed numerical
limits still apply. Optional `--backend d3d12|metal|vulkan` selects a backend.
`--lifetimes-only --cycles 40` omits reference comparison explicitly, requires
no fixtures and labels its result as resource/state coverage only. It must
not be reported as image parity.

For an existing addon workbench build, configure
`PRISMFLOW_FEEDBACK_FIXTURES=<absolute-local-path>/fixtures.json` and optionally
`PRISMFLOW_FEEDBACK_EXACT=ON`. This registers
`dawn_prismflow_feedback_reference` without automatically launching Electron or
retrieving fixtures. That CTest passes on Windows and Mac against their local
same-adapter captures. Fixtures/profiles/raw output are excluded from Git and
release staging. D33 remains open for particles/dots, image/text replacements,
and complete render-controller integration; D34/D35 remain open. Native palette
presentation now has separate evidence in VALIDATION-PRESENTATION.md.
