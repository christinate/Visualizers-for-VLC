# Native image-palette extraction

Observed 2026-11-16 during D33. `image_palette.cpp` ports the complete preserved
`assets/prismflow/image_palette.js` extraction algorithm. It consumes small,
already color-managed, straight-alpha sRGB RGBA8 images and returns eight
LINEAR sRGB colors. It does not decode files, access a window or execute JS.
The separate native file services live in `VLC-Visualizer-Addon/dawn`;
see that repository's `VALIDATION-IMAGES.md` for codec and resampling differences.
Complete renderer/controller integration and production dispatch remain open.

## Preserved behavior and bounds

- Ignore alpha below 0.05 and use the remaining alpha as statistical weight.
- Convert sRGB to linear RGB/Oklab; retain weighted k-means++ initialization,
  distance ties, fixed iteration count and removal of empty clusters.
- Preserve primary/accent scoring, angular restrictions, random draw order,
  smoothness 0..1, image-derived hue knots, no-repeat selection, exposure curve,
  saturation and highlight taper.
- Produce black followed by seven increasing-luminance colors, with a default
  endpoint of 0.85. Retain the 24-step chroma/gamut search and float32 RGBA
  packing used by the existing native palette/renderer helpers.

The native service accepts dimensions 1..512, cluster counts 1..32, iterations
1..50, highlight luminance strictly between zero and one, and optional finite
smoothness in [0,1]. Input byte counts must match exactly. Invalid random draws
and fully transparent images report errors. Generation checks cancellation
through sampling, initialization, clustering, random selection and color output;
the function does not mutate a live palette or publish intermediate state.
Its synchronous input remains caller-owned until it returns.

## Reference evidence

`tests/prismflow_image_palette.mjs` imports the unchanged original module from
an in-memory URL, with seeded Mulberry32 input. Source SHA256:

```text
9da9aa3be5672ae156931aad68e42af19905b1b3ad4568069004d7e0560ef12d
```

The common corpus has 20 images and 457 cases. It includes black/white/gray and
saturated solids, weighted and complementary pairs, transparency padding,
alpha 12 versus 13, wide/tall gradients, seeded color noise, maximum 512x512
input, 1/12/32 clusters, 1/3/10/50 iterations, smoothness 0/0.35/0.5/1/random,
near-one and low highlight luminance, repeated random endpoints and three seeds.
Forty-eight cases must reject invisible images without consuming random draws.

Every successful case checks all 9,816 double color components, exact random
consumption, black/monotonic-luminance/highlight endpoints and 13,088 float-packed
RGBA components. Fixed comparison limits are 1e-7 + abs(reference)*1e-7 for
double colors and 2e-7 for packed floats. Packed bits are recorded, not claimed
identical across different math libraries.

| Native target | Maximum double error | Differing float component bits | In-flight cancellation, including setup |
| --- | ---: | ---: | ---: |
| Windows AMD64 / MSVC | 2.886579864025407e-15 | 244 | 33 ms |
| macOS arm64 / Apple Clang | 2.220446049250313e-15 | 78 | 13 ms |
| Linux x86_64 / GCC | 3.219646771412954e-15 | 267 | 86 ms |
| Linux ASan/UBSan | 3.219646771412954e-15 | 267 | 289 ms |

Invalid dimensions/byte counts/options/random functions and pre-cancellation
are rejected. A separate 512x512/32-cluster/50-iteration job is stopped after
entering clustering and joined without returning a palette. Linux address,
undefined-behavior and leak checks pass without suppressions. This is bounded
helper evidence, not an indefinite application soak or full decoder coverage.

## Reproduction

Build `prismflow_image_palette_probe` in the CPU CMake tree and run
`ctest -R prismflow_image_palette_reference --output-on-failure` (Release on
Windows). Node is needed only to generate the local comparison corpus. To
replay on another owner machine without Node:

```text
node tests/prismflow_image_palette.mjs --fixtures-only <local-fixtures>
prismflow_image_palette_probe <local-fixtures>/fixtures.json <result.json>
```

All raw images, manifests and results stay in ignored folders on the owner's
machines. No source/reference JS becomes a native runtime dependency. The
original Geiss HDR notices/licenses remain alongside the port. D33/D35 still
require complete scene/controller and host integration; decoder/font differences
must remain distinguished from shader and identical-input numerical parity.
