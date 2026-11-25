# Production native dispatch

Measured 2026-11-16 on Windows AMD64/D3D12 (RTX 3070 Ti Laptop), macOS
arm64/Metal (M1), and Linux x86_64/lavapipe on Mint's current Xorg session.
These are local integration suites, not rebuilt installers or a release.
NVK hardware presentation remains unsupported on that non-DRI3 session.

The existing `Visualizers` frontend now dispatches API 4 manifests to
`native/VisualizerNative` (`.exe` on Windows, `.app` on Mac). The runner uses
the same native window, audio, settings and module loader as the workbench.
It accepts registered IDs, resolves current contained package paths and
checks current metadata again before loading. It does not accept CLI plugin
or asset overrides. A schema 4 manifest selects `adapter: dawn`, API 4,
platform/architecture, a contained module and assets directory, and the exact
pinned Dawn revision. Binary API/revision/implementation checks remain in the
module loader. Scanner validation does not execute modules.

`--classic` and the saved Geiss engine preference retain the original engine
host. Windows MilkDrop retains its original DLL; Mac/Linux retain projectM.
Native settings are also available for these original engines. Modern
dispatch never falls back to the browser when its runtime/module is absent.
The POSIX frontend previously validated a fresh manifest but dispatched using
old cached fields; it now uses the fresh metadata. A deliberately altered
cache exercises that correction through the production frontend.

## Observed checks

`tools/check_dawn_dispatch.py` passes twelve cases on each platform:

- Prism Flow, Terrain, and fresh-manifest-over-stale-cache rendering, with
  resize, double-click restoration and Escape/right-click/menu-protocol close.
- Native settings save for all three IDs.
- Original Geiss Classic and MilkDrop/projectM initialization, generated audio,
  fullscreen restoration and existing key round-trip checks.
- Post-scan missing-module and incompatible-Dawn-pin rejection before launch.
- Two simultaneous native modules receiving generated float PCM for one
  process owner, blocking standalone input selection, and closing on owner
  exit. Audio IPC and nonce window records are released afterward.

This generated-player fixture verifies launcher forwarding and ownership; it
does not replace real VLC or Winamp UI/playback validation (D44). Original
engine behavior/key coverage and longer soaks remain D36/D61. No audio is
recorded or saved by this matrix.

Local evidence is in `build/native-dispatch-check-windows-player/summary.json`
and the corresponding `native-dispatch-check-{mac,linux}-player` directories
under `~/vlc-visualizers-work` on the owner's machines. Copied summary reports
remain in ignored local build directories. Each run requires fresh result
files; an old report cannot satisfy a new process check.

The Mac frontend uses Launch Services so microphone authorization belongs to
the native app bundle. `open -W` reports launch completion rather than the
app's exit status; these checks also inspect fresh app reports and observed
window/IPC state. The previous workbench capture retry still reported pending
permission after owner approval; its local stderr retains that failed attempt.
The new production bundle needs its own stable signing/permission validation.
No TCC record is reset, edited or bypassed.

## Build and package boundaries

Build `dawn` with `VIS_BUILD_NATIVE_WINDOW=ON` for `VisualizerNative` and the
existing workbench. Alternatively configure the Addon root with
`VIS_BUILD_DAWN_RUNTIME=ON`, the pinned source-built `Dawn_DIR`, and `SDL2_DIR`.
The default original-host build remains available without a Dawn SDK.

`tools/stage_dawn_dispatch.py` stages a **new empty integration suite** with
one shared runtime, shader assets, both modern modules and original engines.
It deliberately excludes Electron and does not mutate installed VLC, prior
release artifacts or original dependency files. This reduced test directory
is not a complete release payload; dependency/license audits, VLC modules,
installer selections, native source-selection helpers for original hosts,
and standalone installers remain D41/D45/D50 onward. Original-engine
standalone capture still uses the old helper in existing production packages;
that helper is intentionally absent from this generated-audio test suite.
Do not describe this as completed Electron removal or final package size.

Subsequent Windows/Linux original-engine native capture checks are documented
in VALIDATION-CAPTURE-SERVICE.md. They replace the canonical host's capture
dispatch; preserved installers still contain their original helper. Mac and
complete packaging/N-API removal remain pending.

HDR remains Experimental/off by default. This runner presents SDR, pending
explicit native display capability and fallback checks (D37). Existing NVK
numerical/leak findings remain recorded separately and are not cleared here.
