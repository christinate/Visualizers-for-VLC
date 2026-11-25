# Native palette and presentation validation

The subsequent matching-backend original-JavaScript comparison is recorded in
[VALIDATION-NATIVE-REFERENCE.md](VALIDATION-NATIVE-REFERENCE.md). All 24 cases
now match exactly on D3D12, NVK, lavapipe and SwiftShader. The historical
cross-adapter failures below are retained under their unchanged limits.

Initial observations on 2026-11-16 during D33 used unchanged presentation and
palette-overlay WGSL. `gpu_present.cpp` executes them with a packed 256-entry RGBA8 palette,
native uniforms, title-mask sampler and three output formats: RGBA16F, RGBA8
and BGRA8. The caller owns the output render pass; palette UI can be drawn
after presentation and future bokeh. Preparing once per submitted frame keeps
uniform uploads ordered. No readback or wait is added to rendering.

The private helper validates dimensions, headroom, finite uploads/uniforms,
title presence and sample coverage. Supersamples must fit the feedback texture;
invalid geometry is rejected before a draw. Offscreen linear RGBA16F rendering
does not establish display HDR support. Native surface capability/fallback
integration remains D37, with HDR Experimental and off by default. The native
calibration correction below supersedes the original last-square behavior;
the earlier evidence later in this report remains historical.

## Native calibration correction — 2026-11-16

The preserved shader declares 14 brightness values but limits its lookup to
index 12. The new independent GPU regression reproduced the final square at
2.798828125 instead of 3.0 on Windows. `tools/native_shader_fixes.cjs` now applies
one explicit native-only correction: use `kSquareCount - 1u` for that clamp.
The helper requires exactly one original lookup in a 14-square shader, refusing
missing, duplicated or already-patched source. `tools/extract_prismflow_shaders.mjs`
records the fix ID and original shader hash in the native manifest. Sixteen
other native shaders and all preserved upstream source hashes remain unchanged.

`calibration_gpu_probe.cpp` checks all 14 levels, each contrast circle and all
14 selected borders at four geometries. Its 56 cases / 6,496 channels pass on
Windows D3D12, Mint NVK and Mint lavapipe. The final background (3.0), circle
(2.75) and binary-exact border values match exactly. Other decimal arithmetic
allows one adjacent half-float step; observed maximum error is
0.0015625953674316406. The first probe revision incorrectly required CPU-nearest
bits for every decimal result; it failed at 0.85. After correcting that test
expectation, the unchanged shader still failed at the final square before the
actual shader fix. Both failed reports are retained.

The owner-local reference harness has an explicit `--native-calibration-fix`
option for presentation captures only. It applies the same declared correction
in memory, preserves source files and their hashes, labels fixture provenance,
and refuses to overwrite an existing fixture set. Default captures retain the
original behavior. The separate `tools/check_calibration_reference.py` checks
that every covered pixel of the final square changes to its independently
expected value and no other pixel changes. On Windows and SwiftShader it finds
exactly 2,900 corrected pixels / 11,600 channels; the other 23 output images,
all inputs, packed palettes, title bytes and scene definitions are identical.

| New comparison | Result |
| --- | --- |
| Windows native / corrected same-adapter capture | 24 cases / 3,710,976 components, exact |
| SwiftShader native / corrected same-adapter capture | 24 cases / 3,710,976 components, exact |
| NVK / corrected Windows capture | 24 cases, 35 differing components, max 0.0009765625, fixed limits pass |
| Ordinary Windows full scene / preserved unmodified capture | 36 frames / 5,381,100 components, exact |
| Lavapipe / corrected Windows capture | Existing scene-6 title mismatch still fails: max 1.80078125, mean 0.019049369917289973, 17,970 differing components |

No original reference tolerance was changed or failing title case removed.
Windows CTest passes `dawn_prismflow_calibration`,
`dawn_prismflow_presentation_reference` (new corrected fixtures, exact mode) and
`dawn_prismflow_shader_reference`. Register corrected presentation fixtures
explicitly using `PRISMFLOW_PRESENT_FIXTURES`; retain the original captures as
baseline evidence. Full-scene fixtures without calibration remain unmodified.
Mac reruns stay deferred under M08. This is offscreen calibration validation;
no physical HDR display result is claimed, and D35/D37 remain open.

Local evidence includes the Addon's `build/calibration-before-fix-v2/`,
`build/calibration-after-fix/`, `build/prism-calibration-expected-difference-v2/`,
`build/prism-present-corrected-native-v1/`,
`build/prism-scene-calibration-fix-regression/` and `build/calibration-evidence/`.
Mint retains its `native-calibration-*` reports and reference fixtures.

## Local original-renderer oracle

Use the preserved local development Electron runtime with
`tests/prismflow_feedback_reference.cjs --case presentation`. The harness
retains sandboxing and loopback-only serving/external-request blocking. Its
server appends only `export {shaderWGSL}` to expose the original private shader
constant. Source files and shader/method bodies are unchanged; original source
hashes and that visibility shim are recorded in the fixture provenance.
Native builds and installers use no Electron/Node/reference generator.

`tests/prismflow_present_browser.mjs` invokes the original palette uploader,
uniform updater, bind/draw methods and `PaletteOverlayPass`. It supplies an
offscreen render target, a deterministic synthetic title-alpha texture and a
bokeh forwarding sink. It does not run the original canvas/HDR negotiation,
font rasterizer, bokeh or full feedback controller. Those features are not
claimed by this comparison.

Twenty-four cases cover eight scenes in all three formats: wide/portrait
output, SS1/SS2/SS3 and 0.5 undersampling, HDR-valued/clipped palette uploads,
foreground/background mixing, palette swatches with/without a background
swatch, partially clipped title overlay and calibration geometry. The oracle
captures actual palette upload bytes and actual uniform writes; native inputs
come from the same synthetic floating-point palette and half-float feedback.

Every passing run checks 24,576 exact packed palette bytes, 648 exact uniform
components and 3,710,976 output components, including alpha and raw BGRA order.
It also rejects invalid scale/headroom/calibration, NaNs, malformed palettes,
missing title textures, undersized feedback and unsupported output formats.

| Native / original-renderer path | Differing output components | Maximum absolute error | Result |
| --- | ---: | ---: | --- |
| Windows RTX 3070 Ti Laptop D3D12 / same adapter | 0 | 0 | exact pass |
| macbook1 M1 Metal / same adapter | 0 | 0 | exact pass |
| Linux SwiftShader Vulkan / same adapter | 0 | 0 | exact pass |
| Linux SwiftShader ASan/UBSan/leak detection / same adapter | 0 | 0 | exact pass, no sanitizer report |
| Linux GTX 1660 Ti NVK / common Windows NVIDIA capture | 35 | 0.0009765625 | cross-adapter pass |

The fixed general limits, chosen before observing results, are maximum error
0.004 and mean error 0.0002, with byte errors normalized by 255. `--exact`
additionally requires zero differing output components. NVK mean error is
7.2697e-9. CTest registration passes on Windows/Metal with exact mode enabled
and NVK with explicitly cross-adapter fixtures. Byte-for-byte extraction still
verifies all seventeen production WGSL modules and the shader manifest.

Lavapipe's common-NVIDIA comparison fails at scene 6's synthetic title overlay
(the nineteenth case): maximum error 1.80078125, mean 0.019049369917289973 and
17,970 differing RGBA16F components. The preceding eighteen cases meet the
fixed numerical limits. This failure is retained, not made to pass by changing
limits or removing the title case. At that milestone the browser's Vulkan
request selected SwiftShader, so no original-renderer lavapipe capture was
available. The subsequent native-Dawn JavaScript oracle now produces one:
original/C++ lavapipe is exact, while original D3D12/original lavapipe reproduces
the same 17,970 differences and error values. This establishes variation in
the original effect across backends; the exact low-level arithmetic cause is
not claimed. D35 remains open for final/Mac checks. Earlier feedback
cross-adapter failures and independent passing lavapipe lifetime checks are
documented in VALIDATION-FEEDBACK.md.

The sanitizer build instruments the native C++ core/renderer; the linked
shared Dawn and Vulkan driver libraries are not instrumented. These bounded
checks do not prove indefinite stability or absence of all leaks. Full scenes,
longer resource soaks, particles, image/text rasterization, keyboard/controller
integration and production dispatch remain separate gates.

## Reproduction

From Geiss64, using an existing owner-local development runtime:

```text
<local-electron> tests/prismflow_feedback_reference.cjs --case presentation --assets assets/prismflow --output build/presentation-reference
```

Windows `--high-performance-gpu` and Linux `--vulkan-reference` are development
process-only options. Always inspect the recorded adapter rather than assuming
the requested backend/driver was selected. Original-renderer profiles and all
fixtures remain ignored build data on the owner's machines.

Build `prismflow_present_probe` in the addon workbench, then run:

```text
prismflow_present_probe --assets ../Geiss64/native/prismflow/shaders --fixtures <local-reference>/fixtures.json --output build/presentation-native --exact
```

Optional `--backend d3d12|metal|vulkan` chooses the native backend. Omit `--exact`
only for an explicitly cross-adapter comparison; the fixed limits still apply.
Set `PRISMFLOW_PRESENT_FIXTURES=<absolute-local-path>/fixtures.json` and
`PRISMFLOW_PRESENT_EXACT=ON` in an existing addon CMake build to register
`dawn_prismflow_presentation_reference`. This test neither retrieves fixtures
nor launches Electron automatically.

## Original calibration fault (corrected natively above)

The original calibration shader declares fourteen brightness levels including
3.00 but clamps its lookup index to 12. Consequently its fourteenth square
uses 2.80 rather than 3.00. The original D33 capture deliberately retained
this reference behavior. The native correction and its expected-difference
regression are recorded above; the preserved upstream assets remain unchanged.
