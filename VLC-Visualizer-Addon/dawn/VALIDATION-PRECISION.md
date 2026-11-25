# Native half-float conversion validation

Observed 2026-11-16. All analysis and fixtures stay on the owner's machines.

The new Prism Flow spectrum upload comparison exposed two confirmed defects
in the previous shared `encodeHalf(float)` helper:

- `encodeHalf(2^-24)` returned zero, discarding a valid half-float subnormal;
  the correct bits are `0x0001`.
- `encodeHalf(1.00048828125)` returned `0x3c01`; this exact midpoint must round
  to even, `0x3c00`.

The replacement preserves subnormals and rounds to nearest-even, including
the zero/subnormal and subnormal/normal boundaries. It retains signed zero,
finite saturation at +/-65504 and the existing nonfinite-to-zero host policy.
The preserved reference JavaScript and prior releases/installers are unchanged.

`tests/half_conversion.cpp` checks all 63,488 finite half bit patterns through
decode/encode, every one of 31,743 adjacent positive half-value midpoints,
negative midpoint ties, and the immediately neighboring float32 values on
both sides. The total is 126,972 midpoint/neighbor checks. It additionally
checks saturation, NaN/infinities and significand carry. This exhausts finite
half round trips and midpoint boundaries, not all possible float32 inputs.

The converter test passes on Windows AMD64/MSVC, macOS arm64/AppleClang and
Linux x86_64/GCC, and under Linux ASan/UBSan/leak detection. The actual Prism
Flow R16F spectrum upload matches 512 reference components including subnormal,
tie, clipping, signed-zero and nonfinite cases on D3D12, Metal, NVK and lavapipe.

Affected regression checks also pass after rebuilding the relevant executables
and relinking the native Terrain module:

- The 24 common Terrain controller fixtures retain exact half-float histories
  and bounded camera/palette uniform differences on all three systems. Linux
  uses the same Windows-generated fixtures without needing Node.
- RGBA16F readback, the animated Terrain pixel audit and native-plugin ABI/
  loader/render checks pass on Windows/Mac/Linux. Linux's software pixel audit
  also passes. The native plugin check is repeated against the relinked module.
- Shared renderer-bank creation still passes all 17 shaders and 23 pipeline
  variants on the four tested paths.

GPU compute texture storage is a separate conversion: procedural noise uses
toward-zero RGBA16F stores on all tested paths. Full-precision shader-output
instrumentation verifies that behavior independently; the CPU upload fix does
not change WGSL or force a new GPU storage-rounding policy. See
`Geiss64/native/prismflow/VALIDATION-GPU.md` for that evidence.

The sanitizer GPU check runs the owner's existing lavapipe driver with local
ASan/UBSan and leak detection enabled. It reports no sanitizer or leak error
in this bounded run. This is not proof of indefinite stability, all renderer
lifetimes or complete Prism Flow image parity; D33/D35/D60/D61 remain open.
