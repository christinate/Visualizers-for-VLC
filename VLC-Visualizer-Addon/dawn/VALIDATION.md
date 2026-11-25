# Native Dawn foundation validation

Observed 2026-11-15. This is an offscreen development milestone, not a new
release or a completed replacement for the existing Electron host.

The subsequent window/analyzer/Terrain-controller work is recorded in
[VALIDATION-NATIVE.md](VALIDATION-NATIVE.md). Measurements below describe the
initial foundation milestone.

## Preserved baseline

All repositories began clean at their existing `v0.4.0-rc.1` tag. Migration
work uses `feature/dawn-native`; existing tags and release artifacts are kept.

| Repository | Baseline commit |
| --- | --- |
| Geiss64 / Prism Flow | `400a91310223c8b37a7e1dd43af6763136b32137` |
| MilkDrop2-64 | `4a756b1d5b17c3acb1d2558d0db71de47c987787` |
| Terrain64 | `2c2503b244b33cc976533573963e335831be4440` |
| VLC-Visualizer-Addon | `8936362384fb941cd953233a6b52d68aba11126b` |
| VLC-Visualizer-Catalog | `7333a630e31b416128489c6b22d280d21fa8e35f` |

Combined installer baseline: **654,552,819 bytes**, SHA256
`9f2ed3e19d515524392b81ad7087ea1e8cc201e9fef55c3184270bb640d7d9cd`.
Existing artifact checksum manifests were captured locally before changes.

## Observed GPU results

| System / compiler | Dawn input | Actual hardware backend | Checks |
| --- | --- | --- | --- |
| Windows 11, MSVC 14.50.35717, SDK 10.0.26100 | Locally built pinned source | D3D12, RTX 3070 Ti Laptop, driver 32.0.16.1060 | 2/2 pass |
| macOS 26.6.2, AppleClang 21 | SHA256-verified pinned development SDK | Metal, Apple M1 | 2/2 pass |
| Mint 22.3, GCC 13.3 | SHA256-verified pinned development SDK | Vulkan, GTX 1660 Ti, NVK Mesa 25.2.8 | 2/2 pass |

All three report hardware adapters, not software rendering. The RGBA16F test
checks all 4,096 pixels and four channels against exact half-float encodings
of `(0.5, 0.25, 2.0, 1.0)`, including a value above 1. The conversion test also
checks the mantissa-carry boundary near 2.0.

Terrain compiles its existing WGSL, uploads deterministic 256x128 RGBA16F
waveform/beat history, renders above/under/side/valley camera fixtures at
192x128, checks finite/varied red-channel output and verifies a changed
waveform changes the image. Mac/Linux preview images were inspected locally.
Checksums differ across GPU backends; pixel-identical cross-backend rendering
or browser/native visual parity has not been established.

An unavailable Metal request on the D3D12-only Windows build returns a JSON
error and exit code 1 without crashing. No window or media player is opened
by these tests; microphone capture is not requested.

## Concrete issues found and addressed

- The pinned prebuilt Windows library references unsigned SIMD STL symbols
  absent from the installed MSVC 14.50 libraries. Building the pinned source
  with that compiler resolves the link failure. No ABI shims were added.
- Dawn's Windows FXC loader fails when the current SDK compiler DLL is absent
  beside the runtime. The source build and probe now stage the SDK compiler
  in that location. Shader compilation/rendering then passes.
- Windows dependency checkout truncated three long-path DXC test files.
  The generated dependency checkout was restored, and the build tool now
  propagates Git long-path support to child dependency fetches without
  changing global Git settings.

## Size evidence and remaining gates

The initial Windows probe is 126,976 bytes; its locally built Dawn DLL is
6,898,688 bytes, and the SDK compiler DLL is 4,741,488 bytes. Their combined
11,767,152 bytes exclude visualizer assets, capture/settings/windows, player
bridges, installers and complete release notices. This is not the size of
the finished suite or a promised installer size.

The source-build tool records Git dependency revisions, fetched origins,
CMake configuration, installed artifact hashes and upstream top-level
notices. macOS/Linux source builds and the final runtime license audit remain
pending. Raw build logs/provenance/results stay in ignored local build folders.

Native windows, full Terrain history/analyzer, Prism Flow controller/worker
and passes, native capture/settings, plugin ABI/dispatch, installers and the
new long-run/Defender tests remain required in `../todo.md`. Existing candidate
test results do not certify these new binaries. HDR remains Experimental,
off by default, and physically unverified.
