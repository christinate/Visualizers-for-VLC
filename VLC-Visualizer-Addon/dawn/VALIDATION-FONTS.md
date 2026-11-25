# Native font rasterization

Observed 2026-11-16 as part of D33. `text_raster.*` replaces Canvas text
measurement/rasterization with DirectWrite/Direct2D/WIC on Windows,
CoreText/CoreGraphics on macOS, and the existing Qt 6 GUI dependency on Linux.
It shapes complete Unicode strings and uses installed font fallback. No browser,
Node, downloaded fonts or pre-rendered reference masks are runtime dependencies.
This is a host-owned CPU service; its STL values do not cross the plugin ABI.
The final Prism Flow controller and its host service integration remain open.

## Bounds and ownership

Requests accept strict UTF-8 through 8,192 bytes, reject NUL/invalid scalars,
normalize Canvas's four ASCII line/tab whitespace characters, and preserve
combining, Arabic, Japanese and supplementary-plane characters. Empty text
returns an empty image; spaces produce no ink. Fonts are 0.5..1,024 logical
pixels, padding 0..256, supersampling 1..8 and weight 100..900. Generic family
names map to Arial, Times New Roman or Courier New, with OS fallback when absent.
Linux rasterization requires the host GUI thread/session. Font availability,
fallback selection, weight and antialiasing can differ from Canvas and between
platforms; no pixel or metric equivalence between font engines is claimed.

The returned RGBA8 bitmap contains white RGB where alpha is nonzero and zero
elsewhere. Dimensions are limited to 8,192 per axis and 16,777,216 texels before
pixel allocation. All COM/CoreFoundation objects and temporary images have
scoped ownership, including failed measurement/allocation paths. Windows COM
initialization is balanced and accepts an existing STA without changing it.

## Confirmed clipping defect and correction

The original `GpuTextOverlay.set_text()` computes `padding + leftBearing`,
then subtracts that bearing when drawing. Thus the baseline is always at
`padding`, although allocation uses the actual ink width. A generous independent
Canvas redraw proves lost pixels: Windows `jgj AMW` at 18.25px, 3x, normal serif,
zero requested padding loses 19 ink pixels. The leading-spaces fixture loses
660. The initial native Windows implementation lost 16 pixels in the first
case and produced no visible text in the second.

All native backends now draw at `padding + leftBearing`. The probe compares
every corpus image with a separately rendered image having 32 additional
logical pixels of padding. All ten cases have zero clipped ink on all three
systems, including negative bearings/leading spaces. Native and Canvas images
were inspected locally: upright Latin, Arabic shaping, Japanese/emoji fallback,
combining accents, whitespace and the two corrected edge cases.

## Bounded checks

`text_raster_probe` checks the corpus, repeated bitmap identity, whitespace
equivalence, malformed UTF-8/NUL, invalid size/scale, tiny underflowing font size
and oversized masks. Each OS also completed 3,000 render/release calls over
three families, two weights, three supersample factors and four Unicode strings.

| Backend | Handles/descriptors, 500 -> 3,000 calls | Resident bytes, 500 -> 3,000 |
| --- | --- | --- |
| Windows DirectWrite | 173 -> 173; GDI 0, USER 1 | 26,730,496 -> 28,594,176 |
| macOS CoreText | 3 -> 3 | 21,725,184 -> 21,823,488 |
| Linux Qt 6 | 5 -> 5 | 37,830,656 -> 37,830,656 |

Windows resident memory fluctuated rather than staying flat (peak sampled
31,277,056 bytes). These samples do not prove absence of all leaks or indefinite
stability.

`text_native_gpu_probe` rasterizes six strings with the real native service,
uploads each mask to `GpuText`, verifies every uploaded byte by GPU readback,
and renders all four RGBA16F channels with the original text shader. Eighteen
cases use 1x/2x/4x supersampling and compare against an independent CPU bilinear
sampler. Allowed alpha is the two enclosing UNORM8 values, plus 0.0005 for
half-float rounding; mean error must be <= 0.0002. This diagnostic is not the
separate original-GPU renderer parity test.

| Device/backend | Exact mask bytes | Compared components | Maximum / mean error |
| --- | ---: | ---: | --- |
| RTX 3070 Ti Laptop / D3D12 | 1,157,944 | 437,128 | 0.00048828125 / 0.0000104811 |
| Apple M1 / Metal | 1,149,224 | 437,312 | 0.00048828125 / 0.0000093704 |
| GTX 1660 Ti / NVK | 1,326,028 | 491,628 | 0.00048828125 / 0.0000086636 |
| LLVM 20.1.2 / lavapipe | 1,326,028 | 491,628 | 0.00390625 / 0.0000260837 |
| SwiftShader 5.0.0 / Vulkan, ASan | 1,326,028 | 491,628 | 0.00048828125 / 0.0000000776 |

An initial ideal-float CPU oracle failed on lavapipe: a filtered alpha of
0.5/255 became 1/255. The final oracle explicitly permits RGBA8 quantization;
the mask upload still must be exact. An exploratory 3x ideal-CPU comparison on
Windows also exceeded its initial 0.001 limit (maximum 0.00439453125) on
fractional quad/texel phases. Those failed reports are retained locally.
Power-of-two factors provide exactly representable phases for this CPU test.
Arbitrary fractional geometry/3x sampling remains covered by the independent
unchanged-GPU text fixtures in `Geiss64/native/prismflow/VALIDATION-TEXT-GPU.md`;
their comparison limits were not changed. No production shader changed here.

Linux ASan/UBSan with leak detection passes the CPU font probe and the lavapipe
and SwiftShader GPU probes without suppressions. Windows also passes an
existing-STA control: the caller initializes STA, rasterization preserves its
bitmap, then the caller balances its own COM initialization. The NVK GPU test completes its pixel checks but
**fails leak detection**: two 32-byte allocations from
`libvulkan_nouveau.so` (build ID `e556aee4e51ab8708567ed7c632face20b914f84`),
through `dawn::native::vulkan::Device::CreateDevice`. A separately built
`dawn_probe`, with no font/Qt service linked and only the foundation RGBA16F
check, reproduces the same 64-byte leak and driver stack. This isolates the
reported allocations from the font service, but does not resolve the installed
driver retention. The hardware sanitizer result remains a failure and is not
suppressed or counted as a clean leak test. Dawn/drivers themselves are not
fully sanitizer-instrumented. Later D60/D63 must retain this finding.

## Reproduction and remaining gates

Build `text_raster_probe` and `text_native_gpu_probe` in the native CMake tree,
then run `ctest -R dawn_native_font --output-on-failure` (Release on Windows).
Linux CPU tests set `QT_QPA_PLATFORM=offscreen`; the software GPU test also sets
its child process's lavapipe ICD. For the longer resource sample:

```text
text_raster_probe --cycles 3000 --output <local-output>
```

The preserved local development browser can run
`Geiss64/tests/prismflow_feedback_reference.cjs --case fonts --assets <original-assets>
--output <local-reference>`. It serves only loopback, blocks external requests
and preserves browser sandboxing. `tools/compare_native_fonts.py --native <probe-output>
--reference <local-reference> --output <inspection-output>` writes a contact
sheet (original left/native right) and metrics/clipping differences. Reports,
raw masks and profiles stay in ignored folders on the owner's machines.

Native image helpers now have separate evidence in `VALIDATION-IMAGES.md`.
Full controller/render integration, physical HDR
capability/fallback, production dispatch and final installer/security checks
still gate D33 onward. Existing candidate tags/artifacts remain preserved.
