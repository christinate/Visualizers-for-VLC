# Native Prism Flow CPU validation

Observed 2026-11-16. This validates D32 CPU work, not completion of the modern
renderer. All tests and generated numerical maps remain on the owner's
machines. No external analysis service or microphone is used.

Reference: unchanged `assets/prismflow/motion_wormhole.js`, SHA256
`fa32080d8c207d6c9a737f43aa16f9f433aa253f06bd783ded8d0f928deeb944`.
Node 20.18.0 generated the common fixtures on Windows; the same files were
compared by each native build.

The eight cases compare both float displacement components at every pixel,
including a full 512x512 map, for 295,041 pixels total. Plane/lip/tube/miss
classification counts match exactly. Default, wide, tall, orthographic,
reversed flow, stationary flow and nadir-miss behavior are covered.

| Native build | Result | Largest absolute displacement error |
| --- | --- | ---: |
| Windows AMD64, MSVC 19.50 | pass | 2.9103830456733704e-11 |
| macOS arm64, AppleClang 21 | pass | 2.9103830456733704e-11 |
| Linux x86_64, GCC 13.3 | pass | 2.9103830456733704e-11 |
| Linux ASan/UBSan, leak detection enabled | pass | 2.9103830456733704e-11 |

Most active-flow values are bit-identical. Small platform math-library
differences occur, especially in near-zero stationary displacements. The
probe checks a fixed tolerance (`2e-7 + abs(reference)*1e-6`) and reports
actual maximum errors and differing float bits; it does not call these maps
universally bit-identical.

Additional cases reject invalid dimensions/storage, zero radius, nonfinite
parameters and coordinates, excessive integration work, float32 overflow and
cancelled generation. The port retains the reference's 1,024 sphere-tracing
and 4,096 integration-step limits, plus the native map allocation budget and
row-boundary cancellation.

## All motion maps and weighted selection

`motion.cpp` and `selection.cpp` additionally pass 94 complete map/descriptor
cases: two seeds for all 40 selected modes, six additional reference branches
and the explicit dummy map. The four dynamic modes produce their reference
16-float descriptors; actual dynamic GPU evaluation remains D33.

Reference `motion.js` SHA256:
`4d5dfa1bc3bef424395eaf973294a0f164e7cc6732c33dfa371ae583cfb1e7db`.
The fixture generator records hashes for the imported reference modules too.
The same Windows-generated files were compared by all three native builds.

| Native build | Float components compared | Differing float bits | Largest absolute map error |
| --- | ---: | ---: | ---: |
| Windows AMD64, MSVC 19.50 | 45,088,896 | 6 | 1.862645149230957e-9 |
| macOS arm64, AppleClang 21 | 45,088,896 | 16 | 1.862645149230957e-9 |
| Linux x86_64, GCC 13.3 | 45,088,896 | 3 | 1.862645149230957e-9 |
| Linux ASan/UBSan, leak detection enabled | 45,088,896 | 3 | 1.862645149230957e-9 |

All 43 preferences, selection weights and four compatibility matrices match
exactly. Six 256-value random streams match exactly (1,536 values), including
negative and boundary seeds. The breathing-frequency `pow` calculation has
at most 5.551115123125783e-17 difference; it is checked with a separate tight
double tolerance. Map components use the fixed float tolerance above. Unknown
mode requests and cancelled generation are rejected.

Weighted selection passes 1,196 reference cases, covering each companion
mode, combined companions, repeated companions and choices near both ends of
the cumulative distribution. The native selector retains the reference's
float32 adjusted weights and double weight total rather than normalizing them
into a different distribution. Normal random preset construction can use this
selector without changing its compatibility policy.

The first sanitizer attempt was interrupted to correct a comparison-tool
inefficiency: it allocated diagnostic strings for every successful sample.
Formatting now occurs only on a mismatch; all samples and tolerances remain
checked. The optimized full comparison completed with no sanitizer/leak report.
The motion-only milestone passed Windows CTest 2/2.

## Background colors and palettes

The same Windows-generated fixtures pass on Windows MSVC, macOS AppleClang,
Linux GCC and Linux ASan/UBSan with leak detection enabled. All comparisons
remain local. Reference SHA256 values:

- `palette.js`: `d74ad5d975c422842016f501d1605f3e66352fb30a821513763536869abb81de`
- `palette_bkg_color.js`: `6a9c7008385b1838930574455a93f22f587aa41ba1d79ae203a4de2533907c97`

Background checks compare all 29,931 ranked candidates and their ten color/
score/probability components across 36 foreground/options cases, plus 108
selected colors and exact random draw counts. Foregrounds include primaries,
black, white, gray, dim colors and float32 values; options cover both search
limits and deterministic versus adventurous choice.

| Native build | Largest candidate component error | Largest companion color error |
| --- | ---: | ---: |
| Windows AMD64 | 5.684341886080802e-14 | 9.992007221626409e-16 |
| macOS arm64 | 2.842170943040401e-14 | 1.1657341758564144e-15 |
| Linux x86_64 | 4.263256414560601e-14 | 1.5543122344752192e-15 |
| Linux ASan/UBSan/leak detection | 4.263256414560601e-14 | 1.5543122344752192e-15 |

Palette checks cover all 355 bundled palettes, 79 seeded selections/generated
palettes and four custom palettes. They compare 13,900 source components,
448,512 expanded float components, 9,198 sampled components, exact names/indices,
random consumption and four-decimal serialization, including halfway rounding.
Adding palettes and saving embedded background colors preserve reference text.
All source/expanded/sample values and 998,451 ordinary tone-map components have
zero numerical error on each build. The background errors above are measured
separately. Fixed tolerances remain in the probes, not derived from the results.

Two intentional reference differences have explicit regression expectations:

1. `HdrToSdr(0, 0, 0, 0, 3, false)` returns three NaNs in the unchanged
   reference because it divides zero luminance by zero. Very dim colors can
   reach the same state after float32 underflow. The native helper returns
   finite black. The fixtures check 8,523 corrected components.
2. At `t=0.07058823529411765`, RGB
   `[4623.8115234375, 8.691176414489746, 7.058823143779591e-7]` with palette
   maximum `65504` and headroom `1`, the reference white fade exceeds 100%,
   yields negative luminance, and returns three NaNs. The native fade is
   bounded to its white endpoint. The fixtures check 2,367 corrected endpoint
   components, including finite reference extrapolations. Normal bundled
   palettes do not use this correction.

These are confirmed CPU-helper defects, not established causes of historical
system crashes. The original JavaScript source is preserved. Private parser/
serializer functions are exported only in the fixture generator's in-memory
module; the production native build contains no JavaScript runtime.

Invalid sizes, malformed/trailing/nonfinite/out-of-range data, bad random
sources, cancelled generation and the 512-entry catalog limit are tested.
Native palette storage allows 1..4096 colors, RGB through finite half-float
maximum 65504, and finite alpha; text is limited to 1 MiB. SDR-relative headroom
is restricted to [1,65504] so rescaled output remains finite and representable.
The palette milestone passed Windows CPU CTest 4/4. Those two native probes pass
against the same fixtures on Mac/Linux and under Linux sanitizers, with no
sanitizer or leak report in these bounded checks.

## Preset generation, history and worker scheduling

Reference `preset.js` SHA256:
`778b885cd9d18d3858761f6bd7fc7f829871b2dca84b911b86fa7e304d369f47`.
Reference `engine.js` SHA256:
`0300ae8167bd6a7c402d09f4dcca1e25e588721a1d0f610d3020dfaadd56f806`.

All three native release builds and Linux ASan/UBSan/leak detection pass the
same Windows-generated preset fixtures: 32 normally selected presets, each of
40 selectable motion overrides, and six additional reference motion branches.
Twelve cases additionally randomize waveform, dots, shader, palette and motion
settings and replace one debug warp slot. The 150 resulting states compare
32,100 listed numeric parameter components, star-follow/breathing values,
exact random draw counts, 158,388 palette components and 1,094,688 sampled
static-map/dynamic-descriptor components. Static maps are sampled at a 32x32
grid including edges; this is not a full-map reference comparison for every
new seed. The earlier 94-case motion test remains the full-map comparison.

| Native build | Largest parameter error | Largest sampled-map/palette error |
| --- | ---: | ---: |
| Windows AMD64 | 9.992007221626409e-16 | 0 |
| macOS arm64 | 4.440892098500626e-16 | 0 |
| Linux x86_64 | 2.248201624865942e-15 | 0 |
| Linux ASan/UBSan/leak detection | 2.248201624865942e-15 | 0 |

Each of the 78 native presets also passes full map release/rebuild round
trips: static storage capacity becomes zero, regenerated arrays match the
original native arrays exactly, dynamic descriptors survive, and foreground
versus worker rebuild-ticket handling is checked. Star-follow choice is stored
separately from regenerable arrays so replacing a map does not discard that
preset choice. This is an explicit native state-lifetime policy; the source
JavaScript remains unchanged. Invalid modes, slot indices, times, random
values, zero weights and cancellation are rejected.

History fixtures execute the unchanged history/smoothstep function bodies
extracted from `engine.js`, with browser error display and map-rebuild methods
stubbed for timing checks only. All 1,027 active-selection, smooth-weight,
most-active, hard/soft-next and previous cases match exactly. Actual native
map rebuilds are tested separately, rather than inferred from those stubs.

The native controller/worker checks cover:

- Chronological 128-entry pruning, a radius-three static-map cache, immediate
  rebuilding on activation/navigation, and four automatic lookahead cycles
  with the reference duration/fade ranges.
- A maximum of one outstanding request/result. A second submission is rejected
  until the completion is collected. A failed job reports a bounded error and
  the worker subsequently completes a valid job.
- Matching rebuild installation, preservation of concurrent waveform/dot/
  shader/palette edits, and rejection of invalidated or pruned-preset results.
- Four accepted cancellations, each producing a cancelled result with no
  success/error payload, plus 40 normal worker lifetimes and eight shutdowns
  with work queued/running. Destruction joins the worker.
- Fully dynamic four-motion presets retain ready 16-float descriptors;
  128 such entries occupy 32,768 bytes of descriptor arrays and need no static
  rebuild request.

The static-history exercise observed 33,554,432 bytes of map arrays, below its
58,720,256-byte bound for seven complete four-map sets. It also exercises rapid
navigation across pruned maps without growing that bound. This is map-array
accounting, not total process/GPU memory, a soak or proof of indefinite stability.
Transient generation, metadata, palettes and the worker completion are separate.
No sanitizer or leak report occurred in these bounded CPU checks.

The Windows CPU suite passes 6/6, with a targeted scheduler rerun after tightening
navigation memory bounds. Native preset/scheduler probes pass against common
fixtures on Mac/Linux and under Linux ASan/UBSan/leak detection. D32 is complete;
GPU passes, full renderer control integration and complete image parity remain
D33 onward. These components
are not yet dispatched by the addon or staged in installers. Original Geiss
Classic remains unchanged.

The subsequent native frame/controller, API 4 module and full GPU scene
results are recorded in [VALIDATION-FRAME.md](VALIDATION-FRAME.md). D34 is now
verified, including real native window key dispatch. D33 image-drop/worker
integration is now recorded in [VALIDATION-IMAGE-IMPORT.md](VALIDATION-IMAGE-IMPORT.md).
D35 cross-adapter issues and production/release gates remain open.
