# Native frame orchestration and complete scenes

Measured 2026-11-16 on the owner's Windows, macbook1 and mint machines. All
reference captures, generated signals and analysis stay on those machines.
These are development checks; final installers have not been rebuilt.

The later [matching-backend reference report](VALIDATION-NATIVE-REFERENCE.md)
adds original-JavaScript captures on pinned native Dawn. All 36 frames match
exactly on D3D12, NVK, lavapipe and SwiftShader. It also reproduces the retained
cross-adapter first-frame failure using two original-renderer outputs.
The new Metal rerun remains deferred while macbook1 is offline.

## Implementation

`waveform.*` ports the original shrinking-edge box blur, piecewise-linear
area/point resampling and four-flip multiscale waveform alignment.
`frame_cpu.*` blends the active presets, palettes/background, angle wrapping,
eight motion strengths and star-follow weights. It generates the original
flat/stereo/circular waveform points, beat dots, three chasers, grid and
bokeh spawns, including energy attenuation, breathing, shifts, title placement
and auto-darkening. It retains the source's random-draw order. Point storage
is reused, capped at 32,768; bokeh spawning is capped at 4,096.

`scene_dawn.*` connects the preset controller to all native GPU passes:
dynamic maps, warp-dot and bokeh compute, exposure, feedback warp, CPU-point
and GPU-dot drawing, title burning, palette presentation and bokeh drawing.
Feedback remains RGBA16F. SDR colorization and bokeh blending happen in
RGBA8 UNORM before an exact component copy to the ABI's RGBA16F texture;
moving quantization after blending would change the source output. The
Experimental linear path blends in RGBA16F instead. Neither path alone
enables HDR on a display.

Initialization consumes the shared dynamic-noise seed, warp-dot seed,
Gaussian draws, initial feedback noise and the original engine's unused
seed-bank draws in the same order. The latter do not allocate unused CPU
image buffers. Supersampling falls back to 1x when scaled dimensions exceed
the device's limits or the 16,777,216-pixel simulation budget. The public
module/settings allow 1x and 2x.

`plugin_dawn.cpp` exports native API 4 with compatibility ID `geiss` and
display name **Prism Flow**, sharing the host's pinned Dawn implementation.
Malformed settings, invalid frame data and initialization/render errors
return bounded C errors. Internal C++ values do not cross that interface.
Ordinary plugin instances obtain an OS-backed random seed; explicit seed
settings make local comparison runs repeatable without fixing every launch
to the same opening preset. Settings JSON is bounded in bytes and nesting.

## CPU comparison

The local `tests/prismflow_frame.mjs` oracle executes preserved `engine.js`
and its dependencies. An inserted observation statement copies local frame
values; presenter stubs record the generated GPU inputs. Three controlled
presets isolate frame arithmetic from independently tested worker scheduling.
No original source/algorithm is rewritten to generate expected results.

Twelve stateful sequences / 144 frames compare:

| Measurement per platform | Result |
| --- | --- |
| Point components | 2,425,724; exact on all three systems |
| Waveform components | 230,654; exact on all three systems |
| Scalar/palette/parameter components | 205,368 |
| Bokeh spawns | 5,970; all fields and draw counts checked |
| Maximum scalar error, Windows | 0 |
| Maximum scalar error, Mac/Linux | 1.3322676295501878e-15 |

Controls cover landscape/portrait and large simulation sizes, one/two active
presets, palette lock/unlock, SDR/linear headroom, waveform modes/smoothing,
beat/no beat, chasers/grid, zero stars, short/zero cached star lifetime, title
placement, frozen simulation and rising/falling energy. Blur/resampling also
cover empty/single/odd/large arrays and up/downsampling. Invalid/nonfinite
input, sample-count changes and alignment-fraction changes are rejected or
handled explicitly. Linux ASan/UBSan with leak detection passes this corpus.

The preserved aligner computes its offset limit from the *old* input length
before initializing the new length. At 2,048 samples and fraction 0.55 its
first window incorrectly starts at 0 rather than centered offset 461.
Native initialization/length changes compute lengths first and reset the
previous template when its size changes. Production fraction 1 matches the
unchanged startup. Fractional-window reference controls explicitly initialize
the original length first; the unmodified startup fault is captured separately.

An added preset seed 2141510928 checks the state following complete scene
startup. The expanded Windows preset corpus passes 79 cases / 151 snapshots,
including 1,102,880 sampled motion components and all rebuild round trips.

## Complete original GPU scenes

`tests/prismflow_scene_browser.mjs` runs the complete preserved Engine,
GPUWarp and WebGPUPresenter with generated audio and deterministic randomness.
It captures both feedback and final color output. Only offscreen target,
shader export, state reset and diagnostic visibility shims are used. The
background color picker captures its RNG at import time: the harness installs
one seeded proxy *before* importing Engine. An early harness without that
proxy produced a false mismatch and is retained in local build evidence.
Per-frame snapshots are copied so later preset edits cannot mutate evidence.

The native `prismflow_scene_reference_probe` replays actual delayed bokeh
lifetime values recorded by the original frame. This one asynchronous scalar
is controlled for reproducibility; the actual native particle compute,
death history, draw passes and normal asynchronous cache still execute.
The normal module never supplies this optional replay value.

All 36 frames / **5,381,100 components** match exactly on each same-adapter
comparison: Windows D3D12 / RTX 3070 Ti Laptop, Mac Metal / Apple M1, and
Linux Vulkan / SwiftShader. Initial random counts, every subsequent random
count and preset clock are also checked. Scenes cover 1x/2x supersampling,
SDR and Experimental linear output, four active/eight transition slots,
resize, soft next-preset navigation and M/P/W/D/S rerandomization during a
transition. Native GPU text/font/image checks remain separate so platform
font/codec differences are not misreported as identical scene output.

Direct/native-module comparison independently passes 1,002,000 components
on D3D12, Metal, NVK and lavapipe, including resize, 1x/2x, linear encoding,
key commands and title rendering/burning. Invalid settings/dimensions and
nonfinite audio are rejected without destroying the host device.

Linux ASan/UBSan with leak detection also passes all 36 complete scenes,
including exact 5,381,100-component output and teardown, using SwiftShader.
This instruments the renderer/host support compiled for that workbench;
the separately built Dawn runtime and complete GPU driver are not thereby
fully instrumented. The independent NVK driver-allocation failure remains
unsuppressed and is not erased by this software result.

### Retained cross-adapter failure

NVK against the original SwiftShader capture fails the fixed limits. At
scene 0/frame 0, feedback maximum error is 0.82537841796875 and mean error
0.0023502756986999883 despite identical CPU random counts. Across all 36
frames 4,017,468 components differ, maximum error 2.37176513671875. This is
not the same-adapter comparison above and is not a pass. Subsequent pinned-Dawn
original-JavaScript NVK/SwiftShader captures reproduce the first-frame values,
while original/C++ NVK matches exactly over all 36 frames. Cross-adapter output
identity is therefore not claimed; the observed failure also occurs in the
original effect. Limits remain unchanged: feedback max .002 / mean
.0002; linear color max .004 / mean .0002; SDR color max 1/255 / mean .0002.
No changes were made to source WGSL to hide these differences.

Mint's non-DRI3 X session still cannot present NVK windows. Native Linux
window checks use installed lavapipe in that child's environment; hardware
offscreen rendering remains independently tested.

## Local reproduction

Build CPU targets using this directory's CMake project. Run:

```text
node tests/prismflow_frame.mjs <prismflow_frame_probe> <owner-local-output>
```

The full scene targets are built through the Addon's `dawn/CMakeLists.txt`,
using the existing pinned source-built Dawn SDK and this Geiss64 checkout:

```text
<local-reference-electron> tests/prismflow_feedback_reference.cjs --case scene --assets assets/prismflow --output <owner-local-reference>
<prismflow_scene_reference_probe> native/prismflow/shaders <reference>/fixtures.json <owner-local-comparison> --exact
```

The reference runtime is a development oracle and is not a native runtime
dependency or final payload. The harness permits loopback requests only,
denies browser permission requests and bounds output bytes/files. Linux's
reference uses `--vulkan-reference`; the exact native replay selects its
existing SwiftShader ICD in the child environment. Adapter identity is
recorded with each result. Optional `PRISMFLOW_SCENE_FIXTURES` and
`PRISMFLOW_SCENE_EXACT` CMake settings register owner-local scene CTests;
fixtures are never downloaded or included in release packages.

Image-file drop/worker integration is now verified in
[VALIDATION-IMAGE-IMPORT.md](VALIDATION-IMAGE-IMPORT.md). Production dispatch, final platform/HDR
presentation, original-engine retention, extended soaks, installer/Defender
checks and release packaging remain checklist work. These bounded results
do not establish indefinite stability, absence of all leaks or a fix for the
historical machine crash or Defender detection.
