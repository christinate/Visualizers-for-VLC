# Prism Flow native port (in progress)

This directory contains the completed CPU preset/history/worker portion (D32)
and ongoing GPU work (D33) of the Dawn migration. It is not a released renderer.
Full visual parity and production dispatch remain open. The preserved reference
under `assets/prismflow` is unchanged.

`wormhole.cpp` ports the reference's complete geometry, camera projection,
plane/lip/infinite-tube intersections, prescribed flow and adaptive RK4 lookup
backtrace. It fills float32 displacement maps without browser APIs or a
JavaScript runtime. It retains reversed flow, orthographic/perspective views,
sky/nadir misses and surface classification. Native map storage is bounded;
generation supports row-boundary cancellation for the native worker.

`motion.cpp` now ports every registered CPU generation path: 40 selected modes,
the reference's additional branches (40, 41, 43, 44, 98 and 99), and its explicit
dummy map. This includes the 16-float descriptors for dynamic modes 35, 37, 38
and 39; the addon workbench now executes their GPU shaders as D33 work. The code
preserves float32 map writes, double arithmetic, seeded random-bank consumption,
all 43 motion preferences, breathing parameters and compatibility matrices.
`selection.cpp` preserves the preset's double weight total/float32 cumulative
weight behavior. CPU generation supports cancellation and bounds checks.

`background.cpp` ports the full foreground/background scoring, bounded gamut
search and weighted choice. `palette.cpp` retains all 355 bundled palettes,
random palette generation, companion colors, sampling, float32 expansion,
four-decimal serialization, palette editing and CPU tone mapping. Catalog
storage is bounded to 512 entries; palette input allows 1..4096 colors and
finite nonnegative RGB through 65504. Text input is limited to 1 MiB. Background
generation supports cancellation. These are CPU helpers, not HDR presentation.

Two reproducible reference tone-map faults are corrected deliberately: zero
luminance stays finite black rather than dividing 0/0, and extreme custom
palettes cannot extrapolate their white fade beyond 100%. Ordinary bundled
palette output is unchanged in the comparisons. SDR-relative display headroom
must be in [1,65504]. See VALIDATION.md for the separate reference/fix checks.

`preset.cpp` preserves four-motion choices, sampled motion metadata, weighted
preferences, waveform/stereo choices, dot/star settings, shader effects,
palette selection and individual re-randomization/debug-slot operations.
Map release frees static pixel allocations while retaining seeds and tiny
dynamic descriptors. Rebuilding preserves the selected star-follow value as
owned preset state, rather than losing it with the replaced map object.

`scheduler.cpp` provides the CPU controller, smooth preset transitions,
previous/next navigation, automatic 20..30-second lookahead with 3..6-second
fades, and a joined, cancellable native worker. History retains at most 128
entries; static pixel storage is released outside a radius of three entries
around the selected preset. Seven complete static-map sets are at most 56 MiB
of map arrays; metadata, dynamic descriptors, transient generation and the
single worker result are additional allocations. The worker accepts one
outstanding job, including a completed result awaiting collection. It never
reads live UI presets. Rebuild completions are checked by preset identity,
request ticket and saved motion seeds, and only replace the warps so live
waveform/effect/palette edits survive. Closing joins the worker before teardown.

The initial translation used local AST assistance, followed by native review,
compilation and full-map comparisons. The maintained C++ files compile directly;
no parser, JavaScript interpreter or generator is required by native builds.

The port is derived from Geiss HDR, copyright 2026 Ryan Geiss. Retain the
reference's `NOTICE.txt`, Apache-2.0 `LICENSE.txt` and `OUTPUTS.txt`; copies are
included here. Native changes use the Prism Flow name and do not imply upstream
endorsement. The original Geiss Classic implementation remains separate.

Build this CPU-only component with a 64-bit C++20 compiler and CMake 3.24+:

```text
cmake -S native/prismflow -B build/prism-core -DCMAKE_BUILD_TYPE=Release
cmake --build build/prism-core --config Release --parallel 4
ctest --test-dir build/prism-core -C Release --output-on-failure
```

Node is a development reference generator, not a native runtime dependency.
With Node available, CTest generates eight full-map reference cases directly
from `motion_wormhole.js`. Alternatively generate fixtures once and run the
native probe against them on another platform:

```text
node tests/prismflow_native.mjs --fixtures-only build/prism-core-reference
prismflow_core_probe build/prism-core-reference/fixtures.json result.json
```

The fixtures include wide/tall maps, orthographic projection, reverse flow,
stationary flow, the nadir miss and a full 512x512 motion map. The comparison
checks both float displacement components at all 295,041 pixels, exact surface
counts, finite output, invalid inputs and cancellation. The fixed numerical
tolerance permits small platform math-library rounding differences; reports
also record maximum errors and differing float bits. This is CPU map parity,
not a claim of complete Prism Flow image parity or HDR presentation.

Reference source SHA256 is recorded with generated fixtures. Native probe
outputs and maps stay under the ignored build directory on the owner's
machines. No audio or external analysis service is involved.

`tests/prismflow_motion.mjs` adds 94 complete static/dynamic map cases, six
seeded random streams and 1,196 weighted selection cases. CTest runs it when
Node is installed. To generate its common fixtures without Node on a target
machine, use `node tests/prismflow_motion.mjs --fixtures-only <directory>` on
the development machine, copy that directory locally, and run
`prismflow_motion_probe <directory>/fixtures.json <result.json>` on the target.
The full float maps are roughly 172 MiB of ignored development data and must
not be staged into installers. See [VALIDATION.md](VALIDATION.md) for numerical
results and the remaining renderer gates.

`tests/prismflow_background.mjs` compares complete ranked candidate sets for
36 foreground/options combinations. `tests/prismflow_palette.mjs` compares
every bundled palette, seeded random selection and generation, expansion,
serialization, companion colors and tone mapping, including explicit expected
differences for the two corrected reference faults. Both support the same
`--fixtures-only <directory>` development workflow and standalone native
`prismflow_background_probe` / `prismflow_palette_probe` executables. Their
generated fixtures are ignored build data and are excluded from installers.

`tests/prismflow_preset.mjs` compares construction and re-randomization against
78 reference presets, including samples of their motion maps. It additionally
checks full native map release/rebuild round trips. `tests/prismflow_scheduler.mjs`
compares 1,027 exact timing/navigation cases and exercises native memory bounds,
stale results, cancellation, failure recovery, lookahead and repeated worker
lifetimes. Both provide `--fixtures-only` and matching standalone probes like
the other CPU tests. CTest runs six reference/lifecycle checks when Node is
available. Native rendering itself uses none of these fixture generators.

D33 has begun with 17 preserved WGSL modules in `shaders/` and a native Dawn
pipeline probe built by the addon workbench. All 23 pipeline variants compile
on D3D12, Metal, NVK Vulkan and lavapipe Vulkan. Complete rendering remains
open. See [VALIDATION-GPU.md](VALIDATION-GPU.md).
Run `node tools/extract_prismflow_shaders.mjs --check` from the Geiss64 checkout
to verify the tracked WGSL against the unchanged reference. `--write` is for
intentional development regeneration; installed visualizers need neither mode.

The addon workbench now builds `prismflow_dawn`, a private C++ renderer library,
plus `prismflow_dynamic_probe`. The library executes shared noise and all four
dynamic-map passes using the reusable shader bank and per-instance uniforms.
Actual texture/hash/spectrum/uniform checks pass on all four backends, including
Linux sanitizers. `tests/prismflow_dynamic.mjs` generates development fixtures;
native execution uses none of that JavaScript.

`gpu_feedback.cpp` now executes the preserved exposure/warp shaders with
RGBA16F ping-pong feedback, four original Gaussian noise textures and a bounded
twenty-map/eight-slot cache. Same-adapter Windows, Metal and SwiftShader reference
images match exactly; Linux NVK passes the fixed cross-adapter comparison.
Bounded cleanup and Linux sanitizer checks pass. Lavapipe's cross-adapter pixel
difference remains recorded. See [VALIDATION-FEEDBACK.md](VALIDATION-FEEDBACK.md)
for scope, failures and reproduction.

`gpu_present.cpp` now executes native palette presentation, supersample averaging,
synthetic title compositing and palette swatches using the preserved WGSL.
All 24 cases/3.7 million components match exactly on same-adapter Windows,
Metal and SwiftShader; NVK passes the fixed cross-adapter limits. Linux sanitizer
checks pass. See [VALIDATION-PRESENTATION.md](VALIDATION-PRESENTATION.md), including
the failed cross-adapter lavapipe title scene and the confirmed reference
calibration-index fault tracked for D37. Physical HDR capability/fallback,
image/text rasterization and complete render-controller integration
remain open.

`gpu_dots.cpp` now draws explicit point lists and computes/draws 1,024 persistent
warp dots entirely on the GPU. Particle states/control bytes match the same-
adapter original renderer on Windows, Metal and SwiftShader; Windows has six
reported half-float pixel differences within the existing limits. Cleanup and
Linux sanitizer checks pass. NVK's cross-adapter particle-state comparison
fails its fixed limit and remains recorded. See
[VALIDATION-DOTS.md](VALIDATION-DOTS.md). Image/text rasterization and full
controller integration remain open.

`gpu_bokeh.cpp` now ports bounded bokeh spawning, simulation, depth/lifetime,
death history, asynchronous statistics and all three drawing formats. Native
state/history/control/pixel comparisons pass on Windows, Metal, NVK and
SwiftShader under the recorded fixed limits, including Linux sanitizers.
Forty-cycle failed-initialization/drawing/cancellation checks and callback-slot
recovery pass. Actual GPU ticket rollover is checked. Two reproduced source
float-overflow paths are rejected explicitly by native guards. See
[VALIDATION-BOKEH.md](VALIDATION-BOKEH.md). Text/image rasterization and complete
renderer/controller integration keep D33 open.

`gpu_text.cpp` now uploads supplied masks and preserves their logical size,
visibility, timing and GPU stamping with the original WGSL. All 921,600 output
components match exactly on Windows, Metal, NVK and sanitizer SwiftShader;
lavapipe passes the fixed comparison limits. Forty-cycle texture cleanup checks
pass. Two reproduced invalid-timing paths have explicit finite native behavior.
See [VALIDATION-TEXT-GPU.md](VALIDATION-TEXT-GPU.md). All shader variants now
execute natively. Native font rasterization now lives in the Addon host's
`dawn/text_raster.*` service (DirectWrite/CoreText/Qt 6), with separate evidence
in `dawn/VALIDATION-FONTS.md`. `image_palette.cpp` now ports the full alpha-weighted
Oklab clustering/selection/gamut algorithm; see
[VALIDATION-IMAGE-PALETTE.md](VALIDATION-IMAGE-PALETTE.md). Native WIC/ImageIO/Qt
decoders live in the Addon host, with their distinct output/limitations recorded
in `dawn/VALIDATION-IMAGES.md`. Complete controller/host integration and scene
parity are still required before D33 is complete.
