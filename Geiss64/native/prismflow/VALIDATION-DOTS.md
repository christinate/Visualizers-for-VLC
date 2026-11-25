# Native points and advected-dot validation

Observed 2026-11-16 during D33. `gpu_dots.cpp` now draws explicit point lists
and executes the original persistent warp-dot compute/render passes. The
preserved WGSL, corner geometry, four fixed-point advection iterations,
integer-centered drawing and additive RGBA blend/write behavior are retained.
The original shader actually writes all four channels despite an older source
comment describing only R/G; the native port follows the shader.

`GpuDots` owns bounded native vertex/uniform buffers, uploads borrowed point
spans and retains the reference's capacity truncation with a returned used
count. Default capacity is 32,768 points (512 KiB instance storage); empty
uploads clear the count. Malformed/nonfinite used instances, invalid capacities,
missing dimensions and invalid draw buffers are rejected. GPU-generated draws
can use an independent storage/vertex buffer without copying it through CPU
point storage.

`GpuWarpDots` keeps 1,024 full-precision positions (8 KiB), 1,024 draw instances
(16 KiB) and 32 control bytes on the GPU. It preserves seed wrap, invisible
repositioning, strength gating, time/size changes and resize ratios. It computes
after dynamic maps and before feedback warping, then draws after explicit
points into the new feedback target. Encoding adds no readback or CPU wait.
Abandoned frames do not promise to rewind the CPU seed/frame state, as already
documented for feedback. These helpers are private native implementation;
no new C++ ownership crosses the plugin ABI.

## Original-renderer comparison

The local development harness now supports `--case dots`. It runs the unchanged
original `GPUWarp` step and dot methods. Diagnostic `COPY_SRC` usage is added
only to its three particle buffers, allowing direct comparisons of persistent
positions, draw instances and actual mixed float/uint control bytes. Its
loopback-only server, external-request block and browser sandbox remain enabled.
Synthetic point data is used; no audio/media is recorded or uploaded. Source
hashes, actual adapter and diagnostic usage changes are recorded in provenance.

The sixteen-frame feedback fixture includes ten static/dynamic maps, eight
bound slots, time/point-size changes, three disabled-strength frames, empty
point lists, 768 explicit points of six sizes, two resizes and twenty-four
protected cache evictions. It compares 98,304 particle float components, 512
exact control bytes, 365,568 half-float feedback components and exposure.
Gaussian uploads, native CPU maps and the original warp uniforms are also
checked as in VALIDATION-FEEDBACK.md.

| Native / reference path | Particle components with different bits | Feedback components with different bits | Maximum feedback error | Result |
| --- | ---: | ---: | ---: | --- |
| Windows RTX 3070 Ti Laptop D3D12 / same adapter | 0 | 6 | 0.00048828125 | fixed-limit pass |
| Mac M1 Metal / same adapter | 0 | 0 | 0 | exact observed output |
| Linux SwiftShader Vulkan / same adapter | 0 | 0 | 0 | exact observed output |
| Linux SwiftShader ASan/UBSan/leak detection / same adapter | 0 | 0 | 0 | exact observed output, no sanitizer report |

Windows particle state has zero numerical error and zero differing bits;
controls and exposure are exact. Pixel mean error is 6.0106e-9. Its stricter
`--exact` run fails at frame 2 with one half-float component difference; that
report is retained. The passing run uses the already established feedback
limits (maximum 0.002, mean 0.0002, exposure 0.000001), not newly relaxed limits.
The six pixel differences are reported rather than described as bit exact.
Their cause is not established by this bounded comparison. Point draws are
additive and can exceed 1.0 before the next warp; the diagnostic requires them
to be finite/nonnegative instead of incorrectly rejecting legitimate sums.
Feedback-only output still retains its [0,1] check.

The NVK/common-Windows particle comparison fails at frame 3, position component
1,035: 0.4135756492614746 versus 0.41460084915161133, error
0.0010251998901367188 versus the preselected limit 0.00020041460084915162.
This is about 0.001 simulation pixel. It remains a failed cross-adapter state
comparison, with `particle-state-detail.json` retained. Limits are unchanged;
there is no original-renderer capture on that NVK adapter yet. This does not
replace its passing feedback-only/presentation evidence or establish a native
porting fault. NVK full particle/scene parity remains open under D35.

## Cleanup and checks

The existing forty-cycle feedback lifecycle test now also creates, resizes,
aborts, renders and destroys explicit-point/advection resources. It passes on
Windows, Metal, SwiftShader and independently on NVK and lavapipe in explicit
lifetime-only mode. Every run also rejects forty deliberately failing partial
feedback constructors. Live texture accounting returns to the cold baseline
and total live GPU bytes do not grow after warmup. The positive retained-texture
accounting control remains required.

Between cycles 8, 24 and 40, handle/descriptor counts remain Windows 428, Metal
8, SwiftShader 6, NVK 8 and lavapipe 5. Ordinary NVK/lavapipe sampled RSS stays
constant at 95,584,256 / 124,698,624 bytes; Metal stays 54,673,408. Software
allocator/process RSS can vary despite stable live GPU accounting; it is not
claimed to be universally constant. The Linux SwiftShader sanitizer run also
passes all forty cycles with no ASan/UBSan/leak report. Native C++ is
instrumented; linked shared Dawn and driver libraries are not. Longer full-
renderer soaks and absence of all possible leaks remain unproven.

Optional point/advection CTest and the relinked exact feedback-only CTest pass
2/2 on Windows and Metal. Native CLI comparisons cover the Linux paths above.
D33 remains open for bokeh, native image/text rasterization and full controller
integration. CPU waveform generation, controls and full scenes remain D34/D35.
No production dispatch, installer or release artifact is changed by this port.

## Reproduction

From Geiss64, using the already preserved local reference runtime:

```text
<local-electron> tests/prismflow_feedback_reference.cjs --case dots --assets assets/prismflow --output build/dots-reference
```

Run the addon's `prismflow_feedback_probe` with that fixture:

```text
prismflow_feedback_probe --assets ../Geiss64/native/prismflow/shaders --fixtures <local-reference>/fixtures.json --output build/dots-native --cycles 40
```

`--exact` retains its stricter zero-difference requirement; Windows currently
fails that stricter pixel check. `--backend d3d12|metal|vulkan` selects the native
backend; per-child `VK_DRIVER_FILES` can select the locally preserved SwiftShader
ICD. Inspect the actual reported adapters. The browser's Vulkan software capture
selected SwiftShader, not lavapipe. No installed driver setting is changed.

Configure `PRISMFLOW_DOTS_FIXTURES=<absolute-local-path>/fixtures.json` in an
existing addon workbench to register `dawn_prismflow_dots_reference`. It uses
the fixed numerical limits and includes forty cleanup cycles. Native builds
neither launch Electron nor download fixtures. All raw fixtures/reports and
the reference browser profile remain ignored development data on owner machines.
