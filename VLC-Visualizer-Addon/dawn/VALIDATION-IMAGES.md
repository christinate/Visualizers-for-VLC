# Native local image services

Observed 2026-11-16 during D33. `image_decode.*` replaces browser file decoding
with [WIC color management](https://learn.microsoft.com/en-us/windows/win32/wic/-wic-colormanagement),
[ImageIO thumbnails/orientation](https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/ImageIOGuide/imageio_source/ikpg_source.html)
and [Qt image reading](https://doc.qt.io/qt-6/qimagereader.html). These are
host-owned CPU services, not new plugin ABI structures. Font/image pixels share
the internal `RgbaImage` value type; no STL ownership crosses a plugin boundary.
The final renderer/controller, drop handling and production dispatch remain open.

`Geiss64/native/prismflow/image_palette.*` separately ports the complete original
image-to-palette algorithm. Its 457 identical-input comparisons pass on Windows,
Mac and Linux plus Linux sanitizers; see its `VALIDATION-IMAGE-PALETTE.md`.

## File behavior and limits

The service reads local files, selects the first frame, handles EXIF orientation,
converts reported profiles to sRGB, downsamples with premultiplied alpha, then
returns straight-alpha RGBA8. Untagged RGB uses sRGB. It does not upload images,
open a network URL, invoke a browser or keep a file/image cache in application
state. Codecs are OS/system supplied; unsupported/corrupt files report an error.
PNG, GIF and JPEG are verified here. Additional codec availability and complete
drop/controller behavior are not certified by these cases.

Compressed files are limited to 64 MiB, source dimensions to 32,768 per axis and
67,108,864 pixels, and output longest side to 1..512 (default 128, no upscaling).
GIF logical screens and first-frame dimensions are checked separately before
pixel decoding. A bounded first-frame metadata scanner handles global/local
tables and extension subblocks. Codec operations have cancellation checks around
them; the caller must still reject a superseded completion before publishing it.
These input bounds do not assert an exact maximum for OS decoder allocations.

Windows owns each COM session/factory/decoder/frame/transform with scoped
references. GIF canvas padding is a virtual `IWICBitmapSource`, avoiding a
full-canvas application bitmap. Its rectangle/stride/byte bounds are checked
before copying and cancellation is polled while clearing padding. The metadata
locations are documented in Microsoft's
[native format queries](https://learn.microsoft.com/en-us/windows/desktop/wic/-wic-native-image-format-metadata-queries).
Mac owns and releases its ImageIO/CoreGraphics/CoreFoundation resources. Linux
releases the Qt decoder's cached frame before normalizing a GIF and scaling it;
large first-frame clearing also checks cancellation. Final RGBA packing occurs
after scaling, keeping that extra bitmap small.

## Confirmed implementation faults and corrections

- The initial Mac bitmap transform vertically flipped decoded pixels. An
  asymmetric 2x2 RGBA control exposed it; removing the extra CTM flip preserves
  the actual ImageIO row orientation.
- WIC returned an offset GIF frame's rectangle rather than the logical screen.
  A 4x2 frame at (2,3) inside a 12x10 screen exposed the missing canvas. The
  virtual transparent canvas now preserves size, offset and uncovered pixels.
- The initial combined WIC rotation/reflection mappings reversed EXIF 5 and 7.
  Original-decoder pixel comparisons exposed the error. Corrected mappings now
  pass a six-color, asymmetric 24x16 JPEG under all eight orientations, including
  independent corner expectations.
- Qt filled the uncovered region of an opaque offset GIF with black/alpha 255;
  the original returned transparent pixels. The initial 112 differing bytes
  changed the palette by up to 0.4869965094. Its background-color option did not
  correct this case. Native code now clears outside the actual first-frame
  rectangle before resampling; the same lossless fixture matches the original.

These are fixes to the new native decoder implementation and its API adaptation;
they are not evidence about the historical 32-bit system crash.

## Bounded verification

`tools/make_image_decode_fixtures.py` creates owned synthetic fixtures without
third-party image libraries: opaque/partial/zero-alpha PNG, a Unicode filename,
downsampling, global/local-table GIFs, transparent/opaque offset frames, a sparse
downsampled GIF, truncated tables, missing frames, corrupt/oversized input, an
embedded synthetic wide-gamut ICC profile, and all eight asymmetric JPEG
orientations. The profile is generated from numeric ICC tags/curves; JPEG pixels
are owned synthetic blocks. No unrelated picture/profile is redistributed.

All 23 cases pass on Windows AMD64, macOS arm64 and Linux x86_64. Pre-cancelled
requests and invalid output sides are rejected. Actual decoded images feed the
native palette algorithm; transparent images reject palette extraction. The
ICC control demonstrably changes color: Windows's [31,73,217] untagged pixel
becomes [0,91,227] under the synthetic profile, rather than ignoring the tag.

Each process also completes 120 decode/release calls:

| Backend | Handles/descriptors, 24 -> 120 calls | Resident bytes, 24 -> 120 |
| --- | --- | --- |
| Windows WIC | 143 -> 143 | 10,305,536 -> 10,219,520 |
| macOS ImageIO | 4 -> 4 | 12,845,056 -> 12,910,592 |
| Linux Qt 6 | 7 -> 7 | 19,435,520 -> 19,435,520 |

Linux ASan/UBSan/leak detection passes extraction and the complete file-decoder
corpus without suppressions. The shared font-header move also passes the
native font CPU/GPU regressions on all systems: targeted CTest 4/4 on Windows
and Mac, 5/5 on Linux. These results do not instrument every OS codec or prove
absence of all leaks. The separate, unsuppressed 64-byte NVK device-creation
finding remains recorded in `VALIDATION-FONTS.md`; this CPU decoder does not
initialize that GPU driver.

## Original file-path comparison and explicit differences

The preserved local development browser runs the unchanged
`GeneratePaletteFromImage()` on the same files. A proxy captures its actual
Canvas `getImageData` without replacing its decoder or algorithm. It serves
only loopback, permits same-origin blobs for the original fallback decoder,
blocks external requests and preserves sandboxing. Native binaries never use
these captured pixels or the development browser.

`tools/compare_native_images.py` checks matching oriented dimensions, exact
lossless control pixels, at most one byte of alpha/premultiplication rounding,
and at most three bytes for JPEG/ICC conversion. When decoded pixels are
identical, palette components must agree within 2e-7. All three systems pass
those checks. Resampled controls are recorded rather than asserted identical.

**File-to-palette output is not claimed identical across native codecs or to
Canvas.** Native resampling/filtering and color rounding change input pixels;
even a one-byte difference can change cluster choices and a seeded palette.
For the sparse GIF, Windows area filtering returns two partially covered pixels
(alpha 32/96); Canvas returns one opaque sampled pixel. The observed maximum
palette differences in this corpus are 0.458956 (Windows), 0.806764 (Mac) and
0.459457 (Linux). The Mac maximum occurs on a JPEG with a one-byte decode
difference. These are explicit decoder-dependent outputs, not a claim of full
scene/image parity or a substitute for the identical-RGBA algorithm comparison.

## Reproduction and remaining work

Build `image_decode_probe`; run `ctest -R dawn_native_image --output-on-failure`
(Release on Windows). CTest generates the fixtures with development Python.
The installed services need neither Python nor Node. For the independent local
file comparison:

```text
electron Geiss64/tests/prismflow_feedback_reference.cjs --case images
  --assets <preserved-prismflow-assets> --inputs <native-image-fixtures>
  --output <local-reference>
python tools/compare_native_images.py --native <native-image-test>
  --reference <local-reference> --output <comparison.json>
```

Keep reports/raw pixels under ignored build/work folders on the owner's machines.
No Defender setting changed and no sample/report was uploaded. Drop selection,
mixed-file policy, newest-request routing, complete renderer/controller use,
scene parity, HDR capability/fallback and final installers remain separate
gates in `../todo.md`. Existing candidate versions/tags/artifacts are preserved.
