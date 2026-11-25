# Native HDR capability and fallback checks

Measured locally on 2026-11-16. HDR remains Experimental, opt-in and disabled
by default. The owner has no HDR display; physical HDR appearance is unverified.
This is development evidence, not a final installer or all-platform signoff.

## Implementation and limitations

The pinned Dawn revision rejects the `SurfaceColorManagement` extension in
`src/dawn/native/Surface.cpp`. Advertising a float16 internal texture alone
would therefore not establish a native HDR presentation path.

Windows D3D12 now uses the native DXGI float16 route without that extension.
The host queries the window's actual monitor through a fresh DXGI factory and
`IDXGIOutput6::GetDesc1`, checks that Windows reports HDR enabled, and checks
that Dawn advertises `RGBA16Float` for the surface. An API 4 package must also
declare `settings.hdr_output: "native-float"` before the production host asks
it for `VIS_NATIVE_LINEAR_OUTPUT`. SDR-only third-party packages keep working.
Each returned image's encoding is checked against its requested mode.

Linear plugin RGB uses 203-nit reference white. The Windows presenter scales
it by 203/80 for the floating-point DXGI swapchain's default linear scRGB space.
This follows Microsoft's [Advanced Color guidance](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range).
No Windows display settings, driver settings or HDR metadata are changed.
Scaled output is bounded to the finite half-float range, ±65504: multiplying
a valid extreme half-float source by 203/80 would otherwise overflow float16.

Unavailable HDR selects the existing encoded SDR rendering path. An HDR-started
session also checks the display about once per second: losing HDR switches
to an SDR surface with bounded linear-to-SDR tone mapping; returning to HDR
restores float16 presentation. The plugin, preset state and capture session
remain alive. A session that starts in SDR stays SDR until reopened, preserving
its renderer's output encoding. Diagnostics explain this choice. The default
HDR-off path does not keep polling display capabilities.

Mac Metal EDR now has bounded capability/conversion, actual layer/color-space,
headroom-transition and production-window checks; see
[Mac return validation](VALIDATION-MAC-RETURN.md). It tone maps to SDR in its
linear surface when current headroom is 1.0. The pinned Vulkan swapchain
chooses the sRGB color space; native Linux HDR transport remains open. Original engines
retain their existing output paths; this change adds native presentation for
Prism Flow and Rhythm Terrain and does not certify original-engine HDR behavior.

## Observed results

| Check | Observed result | Scope |
| --- | --- | --- |
| Capability policy | 11 cases pass | Off, unavailable display/transport/format, available HDR, SDR-only plugin |
| GPU presentation conversion | 30 cases / 7,680 channels pass per backend | Windows D3D12, Mint NVK, Mint lavapipe |
| SDR copy | Exact half-float channel bits | Includes negative, zero and extended-range source values |
| scRGB output | Maximum finite channel 65504 | Offscreen float16; extended range retained and extreme input bounded |
| Conversion error | Maximum absolute 0.003124237060546875 | Fixed half-float limits: max(0.0006, abs(expected) × 0.001) |
| Windows real surface transitions | 3 configurations pass | Float16 → SDR → float16, injected display observations, black frames |
| Windows SDR-started / SDR-only plugin | Pass | No encoding change; reopening explanation / SDR-only reason |
| Windows production host | 4 cases pass | Both modules × HDR off/requested; actual SDR display, Escape closure |
| Linux production host | 4 cases pass | Both modules × HDR off/requested; lavapipe windows, explicit Vulkan fallback |
| Window regression | 8 cases pass | Fullscreen restoration, Escape/right-click/menu closure, device loss and initialization failures |

Windows: RTX 3070 Ti Laptop, D3D12 driver 32.0.16.1060. Linux: GTX 1660 Ti,
NVK Mesa 25.2.8 and lavapipe LLVM 20.1.2. NVK offscreen checks pass; its window
presentation remains unsupported in Mint's non-DRI3 X session. None of these
results resolves the separately recorded NVK numerical/allocation findings.

`presentation_surface_probe` injects display observations only into the C++
presenter; it still configures real Dawn/Windows surfaces. There is no production
CLI override for enabling HDR against the OS capability result. This test
does not stand in for an HDR display test. Conversion probes use known constant
pixels and independent CPU expectations. No recording or application data is
uploaded; the window checks use generated audio.

## Reproduction and evidence

Build `presentation_probe`, `presentation_surface_probe` (Windows), `dawn_window`
and `VisualizerNative` through the native workbench. CTest names are
`dawn_presentation` and `dawn_presentation_surface`. Run
`tools/check_native_presentation.py --help` for the two-module production or
workbench check; `--expect-sdr` asserts fallback on an SDR machine.

Owner-local reports:

- `build/dawn-check-source/presentation-test/result.json`
- `build/dawn-check-source/presentation-surface-test/result.json`
- `build/native-hdr-settings-windows-v3/result.json`
- `build/native-hdr-window-regression-v1/`
- `build/native-hdr-evidence/linux-{nvk,lavapipe,settings}.json`
- Mint: `~/vlc-visualizers-work/native-hdr-settings-linux-v5/`

The first new offscreen probe used a source texture without CopyDst usage;
Dawn rejected its upload. That test-setup error was corrected before the
passing runs. Early Linux production attempts lacked a scanned catalog, then
used the wrong scanner flag; those failed reports/logs remain separate. Passing
Linux production checks use an expendable scanned suite and fresh native host.

## Upstream Vulkan limitation and subsequent qualification

The original unpatched Dawn source pin
`7daf3b0bee2c73621001ab15b256b5a84fb0b45f` fixes Vulkan presentation to
`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` and rejects `SurfaceColorManagement`.
Vulkan defines the format and presentation color space separately. A Linux
HDR path needs an HDR-capable surface format/color-space pair and a compositor
that accepts it. Mint's available test desktop is X11 and uses lavapipe for
application windows. The earlier Terrain payload also uses the SDR fallback
when tested on nested Muffin Wayland with HDR requested. Its real Wayland
surface lists float16 with sRGB nonlinear and pass-through color spaces and
offers no extended-linear, PQ or HLG pair.
[Vulkan presentation color spaces](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html)
[Scene release follow-up](VALIDATION-SCENE-RELEASE-CHECKS.md)

The Dawn main-branch surface validator still rejected `SurfaceColorManagement`
when checked on 2026-11-19. A dependency update needs a Vulkan implementation
that negotiates a supported HDR format and color-space pair.
[Dawn surface validator](https://dawn.googlesource.com/dawn/+/refs/heads/main/src/dawn/native/Surface.cpp)

The exact rebuilt Linux x64 archive now includes a checksum-pinned patch
for that Dawn source. With Weston desktop-shell it passes four real VLC
HDR cases, four explicit SDR cases and four SDR fallback cases with the
compositor color-management protocol disabled. All four scenes are exercised
in each mode. Experimental HDR remains off by default. Physical HDR output
remains unmeasured. [Packaged Wayland and VLC evidence](VALIDATION-SCENE-RELEASE-CHECKS.md)

The Mac production run observes both SDR and active Metal EDR, depending on
the opt-in setting and headroom reported by the display. Physical brightness
and color remain unmeasured.
[Current Mac presentation evidence](VALIDATION-CANDIDATE-2026-11-19.md)
The preserved calibration shader's final-square defect has since been corrected
explicitly in native assets: 56 GPU cases pass on D3D12/NVK/lavapipe, and
expected-difference checks isolate the 2.8-to-3.0 correction. See Geiss64's
`native/prismflow/VALIDATION-PRESENTATION.md`. Upstream source remains unchanged;
no reference thresholds were relaxed. The Mac calibration rerun passed. The
matching current macOS rebuild and release gates remain in `todo.md`.
