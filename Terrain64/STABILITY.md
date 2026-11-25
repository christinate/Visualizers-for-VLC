# Native validation scope

The current candidate is **0.5.0-rc.1** and uses native C++/Dawn.
[README.md](README.md) and [RELEASES.md](RELEASES.md) describe current backend,
media and resource results and the remaining release gates. The Addon
repository's `todo.md` is authoritative for suite completion.

The earlier canvas/Node/capture-broker observations below describe the preserved
browser implementation, not the native production process. Keep their original
measurement/version scope; they do not qualify the new runtime. Current native
Vulkan window checks use lavapipe; the older browser no-adapter result is not
an assertion that every native Linux backend is unavailable.

## Earlier browser validation

The Windows Intel Iris Xe test rendered more than 7,800 frames over 270 seconds
and visited all four camera modes without a reported renderer error. Actual
Windows UI checks verified borderless display, double-click fullscreen and
restoration, and Escape closure. An Apple M1 native smoke test rendered Terrain
using an RGBA16F HDR canvas. These bounded tests do not prove indefinite
stability or certify physical HDR appearance.

History storage is fixed at 256 × 128 RGBA16F texels. Audio packets allow only
one outstanding write, GPU work is awaited before the next frame, and closing
the renderer releases its capture stream and GPU device. Logs rotate at 8 MiB.
Node access remains in the main process; renderer pages are sandboxed and
network requests are blocked. Self-test renderer failures return a failing
process status instead of a successful timed exit.

The half-float encoder now carries a rounded mantissa into the exponent;
the previous bitwise combination incorrectly mapped values close to 2.0 back
to 1.0. The recording broker stops tracks and closes its AudioContext if
initialization fails after acquiring the input device.

Linux Mint's current software/old-driver graphics stack returned no compatible
WebGPU adapter. That test failed and remains a compatibility limit. No driver
blocklist or sandbox has been disabled to mark it as passing.


## 0.4.0-rc.1 validation and remaining work

HDR is Experimental and disabled by default, as requested by the owner. No
physical HDR display is available; calibration/appearance is not certified.

The original classic engines retain their prior 30-minute and sanitizer
validation. The new WebGPU renderers have bounded smoke/flight tests, not an
indefinite-stability guarantee. Terrain completed all four 60-second camera
modes in a 270-second Windows test (7,820 reported frames). Prism Flow reported
two RGBA16F feedback textures and four active motions on Intel Iris Xe.
MilkDrop standalone received nonzero PCM through the synthetic capture broker.

Specific fixes: capture initialization failure now stops acquired tracks and
closes its AudioContext; input lists remain usable after permission denial;
synthetic input does not open a microphone; native source reselection drops a
stale IPC mapping; normal native playback no longer appends self-test metrics
forever; browser logs rotate at 8 MiB; C++ exceptions are caught at N-API
callbacks. The terrain half-float rounding carry is covered by a regression.
Settings migration preserves older Windows INI display preferences. Installers
refuse an older registered package, including the legacy Geiss64 product,
before it can conflict with the new package's uninstaller.

The local-only Windows installer matrix passed standalone plus both synthetic
Winamp architecture layouts for all three plugins, including upgrade refusal,
owned-file uninstall and preservation of a user file. This does not replace
real Winamp playback validation. Computer Use was stopped by physical Escape;
real Winamp UI checks remain pending. Extended soaks of the new WebGPU engines
and physical recording-device changes also remain for stable-release signoff.

Mac and Linux archive install/scan/opt-out/uninstall checks passed, including
modified-file refusal and exact restoration. A separate regression verified
framework directory links, file links, changed dangling-link refusal, and
original symlink restoration on both platforms. Mac Terrain rendered using a
floating-point canvas. Mint's current graphics stack returned no compatible
WebGPU adapter; Classic Geiss/projectM remain the tested options there.
