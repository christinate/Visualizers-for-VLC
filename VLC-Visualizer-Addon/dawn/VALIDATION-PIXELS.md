# Terrain diagonal-pixel regression

Observed and corrected 2026-11-15. The owner reported white square pixels
along the bottom-left to top-right diagonal during Linux window tests.
This was reproduced in Terrain's raw RGBA16F output, before presentation,
the desktop compositor or remote display transport. Remoting is therefore
not required to produce this defect.

## Isolation and correction

The affected path was lavapipe/llvmpipe, Mesa 25.2.8 with LLVM 20.1.2, using
the locally source-built pinned Dawn. Linux NVK hardware, Windows D3D12 and
Mac Metal did not reproduce it. Flipping the fullscreen triangle flipped
the defect's diagonal. Diagnostic shader variants reproduced incorrect
late-read output-control values at edge lanes after divergent ray tracing.
Reading those controls before the trace removed the defect. The exact
upstream compiler/driver root cause has not been established.

`terrain.wgsl` now snapshots `g.color` before calling the ray tracer and
uses that snapshot for SDR/linear-output selection and scaling. The change
does not clamp or conceal the erroneous output, disable HDR, replace the
terrain, or alter camera/audio logic. The shared shader serves both the
preserved browser renderer and the new native renderer. The preserved
release packages themselves have not been rebuilt or overwritten.

## Regression evidence

`dawn_probe --terrain-shader <terrain.wgsl> --terrain-audit --output-dir <dir>`
renders 128 deterministic animated frames at 192x108: 96 SDR and 32 linear
HDR intermediate frames, across all camera modes and transitions. It checks
every RGBA16F channel for finite values, alpha for opacity, and SDR RGB
channels for the expected [0,1] range. Linear HDR values above 1 remain valid.
Reports include channel ranges, pixel coordinates/raw half-float bits for
failures, and frame checksums. Selected frames are also saved as unmodified
little-endian RGBA16F data; 8-bit previews alone can conceal a numeric defect.

| Native rendering path | Fixed frames passing | Comparison with original |
| --- | ---: | --- |
| Windows / RTX 3070 Ti Laptop / D3D12 | 128/128 | All frame checksums identical |
| Mac / Apple M1 / Metal | 128/128 | All frame checksums identical |
| Linux / GTX 1660 Ti / NVK Vulkan | 128/128 | All frame checksums identical |
| Linux / llvmpipe / software Vulkan | 128/128 | Original fails 89 SDR frames |

Each path checks 2,654,208 pixels and 10,616,832 channels. No nonfinite
channels or bright-white SDR pixels were observed in the fixed fixtures.
The original shader fails the same new test, so this is a demonstrated
regression check rather than a test that only exercises successful startup.

CTest includes `dawn_terrain_pixels`. On Linux with an installed lavapipe ICD,
it also includes `dawn_terrain_software_pixels`, selecting that driver only
for the test process. The full native suites pass: Windows 7/7, Mac 7/7 and
Linux 6/6 (Node reference checks are run separately on that machine).
The five Linux window/fullscreen/closure/device-loss cases also pass again
with the fixed shader and API 4 Terrain module.

Raw reports and shader-isolation variants remain in ignored local build
folders. No source, diagnostic sample or executable was submitted to an
external analysis service. These checks establish corrected renderer output;
they are not a direct physical-display inspection. NVK presentation remains
unavailable on the tested non-DRI3 Xorg session. HDR remains Experimental and
off by default, with no physical HDR-display claim.
