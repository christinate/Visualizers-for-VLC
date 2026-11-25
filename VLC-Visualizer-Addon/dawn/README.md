# Native Dawn runtime and migration worklog

This directory implements the native C++ runtime used by the 0.5.0-rc.1
development candidate. Windows/Linux/Mac production package builders use it;
final releases remain pending. The v0.4.0-rc.1 tag preserves the browser
candidate. Track current gates in [../todo.md](../todo.md) and use
[../sdk/NATIVE-RUNTIME.md](../sdk/NATIVE-RUNTIME.md) for current build/package
commands.

The runtime supplies API 4 dispatch, native windows/settings/capture, player
PCM ownership and bounded Dawn submissions. Prism Flow and Terrain are ported
and pass same-backend reference and bounded real-player checks. Original
Classic/MilkDrop/projectM engines remain separate. Current evidence includes
[real VLC actions/PCM](VALIDATION-LIVE-MENU.md),
[the guarded Windows installer](VALIDATION-PACKAGING-GUARD.md) and
[catalog revalidation](VALIDATION-CATALOG-SAFETY.md),
[60 Mac menu/media checks](VALIDATION-MAC-MENU.md) and
[Mac deployment/independent packages](VALIDATION-MAC-COMPATIBILITY.md).
Real recording, manual input, remaining player/soak checks and final-source
artifacts stay open.

The sections below retain the original design and chronological migration
milestones, including intermediate incomplete states. Their older open-work
statements are historical; the checklist above records current completion.

## Architecture and ownership

- Keep the existing VLC audio filter/menu, Winamp capture bridges, installed
  catalog, per-player ownership and nonce-protected window-close protocol.
- Keep visualizers in separate processes from media players. Native rendering
  must not put new graphics work into VLC's or Winamp's own process.
- Host native windows, audio capture, settings, timers and source selection
  in C++. The audio callback must not create GPU resources or call UI APIs.
- Use Dawn through its pinned WebGPU headers, with one implementation per
  suite. [Native API 4](NATIVE-API.md) uses versioned C structures, borrowed opaque
  handles and explicit retain/release responsibilities; no C++ allocations,
  STL objects or ownership cross a plugin boundary.
- Use Direct3D 12 on Windows, Metal on macOS and Vulkan initially on Linux;
  record the actual adapter/backend and test additional backends separately.
  Do not promise a native backend will fix Mint's existing adapter limitation.
- Bound work in flight. Finish or cancel outstanding work before destroying
  dependent resources. Resize and device loss must release/recreate resources
  in a documented order and keep window closure responsive.
- RGBA16F feedback and supersampling are independent of display HDR. HDR
  remains Experimental and off by default; SDR must remain usable.

The planned host keeps the UI/event loop on the main thread, capture callbacks
limited to bounded PCM ring-buffer writes, and analysis/render work on the
render thread. Allow at most two submitted frames, pausing while minimized.
On resize, release acquired surface textures before reconfiguring; keep
internal feedback resize policy explicit. On device loss, stop submissions,
report the adapter/error, and tear down renderer resources before the device.
Retry must be bounded and user-visible. Shutdown stops capture first, joins
workers, completes/cancels callbacks, releases renderer and surface references,
then releases device/adapter/instance. These are implementation requirements;
the current workbench exercises native windows and bounded GPU submissions;
the production host still requires dispatch and UI integration.

## Browser dependency inventory

Prism Flow's controller/preset/motion code is JavaScript. It uses a module
Worker for CPU warp construction, WebGPU render/compute passes, browser
image/palette and text helpers. The native migration must replace those
facilities and preserve four active motions/eight transition slots, preset
history, all passes, 16-bit feedback and keyboard controls. Terrain's WGSL
is already a separate source file; its camera/history/beat controller is a
smaller initial port.

D32's native CPU replacement now lives in `Geiss64/native/prismflow`: all
motion generators, palettes/background choice, preset parameters, bounded
history, navigation and cancellable worker/controller scheduling. Common
reference and lifecycle checks pass on Windows/Mac/Linux, including Linux
sanitizers. See its `VALIDATION.md`. Prism Flow GPU passes and render/control
integration remain D33 onward; this CPU milestone is not production dispatch.
The private `prismflow_dawn` library now executes dynamic maps, feedback and
exposure with the original WGSL. See
`Geiss64/native/prismflow/VALIDATION-FEEDBACK.md` for exact same-adapter image
checks, bounded cache/cleanup evidence and the unresolved lavapipe pixel
comparison. Remaining draw passes and full controller integration are open.
Native palette/supersampling/title-mask presentation and swatches now have
separate evidence in `Geiss64/native/prismflow/VALIDATION-PRESENTATION.md`.
This is offscreen rendering, with physical HDR capability/fallback still D37.
Explicit-point drawing and persistent warp-dot compute/render now have evidence
in `Geiss64/native/prismflow/VALIDATION-DOTS.md`, including the retained Windows
exact-pixel and NVK cross-adapter state failures. Full scene integration remains
open.

Native bokeh simulation, rendering and asynchronous statistics now have scoped
evidence in `Geiss64/native/prismflow/VALIDATION-BOKEH.md`. This covers all seven
compute passes, GPU history rollover, bounded cancellation/registry recovery
and two reproduced source overflow guards. Native text/image helpers and full
Prism Flow controller/dispatch integration remain open.

The text-mask state/stamp helper now has evidence in
`Geiss64/native/prismflow/VALIDATION-TEXT-GPU.md`. All shader variants have native
execution coverage, with the original WGSL unchanged. Native font rasterization
now uses DirectWrite, CoreText and Qt 6; see [VALIDATION-FONTS.md](VALIDATION-FONTS.md)
for Unicode/bounds, confirmed clipping corrections, native glyph-to-GPU checks
and 3,000-call resource samples on each OS. The original glyph masks remain
development fixtures only. Native WIC/ImageIO/Qt file decoding and the full
image-palette extraction algorithm now have separate evidence in
[VALIDATION-IMAGES.md](VALIDATION-IMAGES.md). Codec/resampling differences are
explicit; identical-RGBA extraction is compared independently. Full
renderer/controller/host integration remains required before D33 is complete.

The current Electron main process also owns the HTML settings windows,
getUserMedia/AudioWorklet input selector, runtime process lifecycle, PCM
N-API bridge, CLI dispatch, diagnostics and window registrations. All these
paths require native replacements before Electron is removed from installers.
The original Classic/MilkDrop engines and existing audio IPC can be retained.

## Dependency bootstrap

`python tools/prepare_dawn.py` fetches and verifies the pinned SDK archive.
Use the returned Dawn_DIR when configuring this directory with CMake.
The pin records the upstream source revision and platform SHA256 values.
GitHub's nightly native SDKs are a best-effort, unsigned upstream convenience;
they are development inputs, not our final dependency-release policy.
Final releases require a documented pinned source build and complete notices.

From the Addon repository, with CMake 3.24+, Python 3.12+, Git and a C++20
compiler installed:

```text
python tools/build_dawn.py
python tools/check_dawn.py --dawn-dir build/dawn-install/lib/cmake/Dawn
```

The build tool selects D3D12, Metal or Vulkan for its current platform, builds
one shared Dawn library, disables samples/tests/Node bindings and unused
shader writers, and writes `build/dawn-install/build-provenance.json`. The
Windows recipe has been exercised with MSVC 14.50 and Windows SDK 10.0.26100;
the Metal source recipe also passes on the M1 Mac. The Linux source recipe
passes with GCC 13.3 on Mint. The current Linux
install layout may use `lib64`; use the actual `Dawn_DIR` printed by the tool.
Linux source builds need the Vulkan/window-system development dependencies.
Windows keeps the SDK's `d3dcompiler_47.dll` beside `webgpu_dawn.dll`; copying
only the Dawn DLL is insufficient. The builder matches that DLL's SHA256 to
its SDK and collects the corresponding SDK license and third-party notices.
It records the SDK version and notice hashes in `share/dawn/windows-sdk-source.json`.
Notices are collected for audit, which
remains a release gate. This recipe records inputs; byte-identical builds
across different compilers are not claimed.

Windows builds use `/MT`, including Abseil's matching static-runtime option.
The window host, Dawn DLL and SDL DLL have been inspected and do not import
MSVCP/VCRUNTIME redistributable DLLs. Only WebGPU C entry points cross the Dawn
boundary; C++ wrappers stay local to each module. Future plugins must keep
their allocation/free pairs in the same module, and remain loaded until all
callbacks are completed/cancelled and the host GPU context is released.
Do not use Dawn Native C++ APIs returning STL ownership across that boundary.

For development dependencies extracted into a private Linux prefix, pass
`--dependency-prefix /path/to/prefix/usr` to the source builder. This includes
the X11/XCB development headers needed by Dawn's Xlib backend.

For a development-only prebuilt SDK, save the JSON printed by
`prepare_dawn.py`, then run:

```text
python tools/check_dawn.py --sdk-result build/dawn-sdk-result.json
```

The pinned Windows prebuilt SDK requires MSVC runtime symbols absent from
our installed 14.50 toolset; use the source recipe there. The pinned macOS
and Linux SDKs passed the foundation checks with AppleClang 21 and GCC 13.3.
An explicit `--cmake` executable path is accepted by both build/check tools.
`--jobs` bounds CMake parallel jobs; upstream compiler-level parallelism may
also run within a job.

The GPU CTest cases check every pixel of a known 64x64 RGBA16F target and
render the existing Terrain shader at 192x128 with deterministic waveform
history, four camera modes and a silence comparison. JSON reports and PPM
previews stay in the ignored build directory. See [VALIDATION.md](VALIDATION.md)
for observed results and their limits.

`dawn_probe --terrain-shader <terrain.wgsl> --terrain-audit --output-dir <dir>`
also checks 128 animated SDR/linear HDR intermediate frames, preserving raw
half-float samples for diagnosis. CTest runs this check on the selected
adapter and, when installed on Linux, separately on lavapipe. See
[the diagonal-pixel regression](VALIDATION-PIXELS.md) for the reproduced defect,
fix and bounded comparison results.

With Node available as a development tool, CTest also compares the native
float PCM/FFT/RMS/beat analyzer against 612 JS reference windows and the native
Terrain controller against 24 JS fixtures. Node is not a native runtime
dependency. Machines without Node can use the same generated synthetic
fixtures with `tools/check_audio_reference.py` and
`tools/check_terrain_reference.py`.

## Native window workbench

`VIS_BUILD_NATIVE_WINDOW=ON` builds `dawn_window`, a borderless SDL window with
a Win32 HWND, Cocoa/Metal layer, or X11/Wayland surface presented by Dawn.
It opens a native input picker for standalone use, with Ctrl-L/Command-L
reselection. `--line-in default`, a device ID or an exact label selects input
without the startup picker; `--synthetic` explicitly selects generated audio.
`capture_probe --list` lists devices without opening them. Native recording
checks pass on all three systems. Mac builds produce `capture_probe.app` and `dawn_window.app`
with microphone usage descriptions and asynchronous AVFoundation permission
requests. Run the binary under `Contents/MacOS` for CLI usage, or launch the
bundle through `open --args ...`. An unsigned development bundle may need a
local ad hoc signature (`codesign --force --sign - <app>`); this is not release
signing or notarization. Both the Mac helper and integrated visualizer capture
checks pass after their separate OS approvals. It continues processing closure
events while permission is pending; development rebuilds may require approval
again because ad hoc signatures change with build contents.
Production dispatch remains checklist work.

On Windows, `python tools/prepare_windows_sdl.py` verifies and extracts the
pinned SDL 2.32.10 development SDK. Then, for example:

```text
python tools/check_dawn.py --dawn-dir build/dawn-install/lib/cmake/Dawn --build-dir build/dawn-check-source --sdl-dir build/sdl-sdk/SDL2-2.32.10/cmake
python tools/check_dawn_window.py --exe build/dawn-check-source/Release/dawn_window.exe
```

macOS/Linux accept the installed SDL CMake package directory with `--sdl-dir`.
Run `dawn_window --terrain-shader /path/to/Terrain64/assets/terrain/terrain.wgsl`
for standalone input selection, or add `--synthetic` for an untimed generated
audio demo. Double-click/F11/Alt+Enter toggles fullscreen;
Escape/right-click closes it; Space changes color.

`input_picker_probe` exercises dialogs without opening a recording device.
`tools/check_dawn_input.py --exe <dawn_window> --picker-probe <input_picker_probe>
--plugin <terrain_dawn module>` tests startup/keyboard reselection with real
capture in windowed and fullscreen modes. Linux accepts `--software` for its
installed lavapipe path. On Mac, pass the app's `Contents/MacOS/dawn_window`
executable and approve its normal microphone request before running the
checker. See [native input validation](VALIDATION-INPUT.md) for results/limits.

`dawn_window --settings terrain` opens the native settings editor without
initializing graphics or recording. `geiss` and `milkdrop` select their own
options. `--profile <directory>` overrides the existing per-user profile
location. JSON preferences and unknown plugin options survive edits; legacy
Windows INI settings are imported when JSON is absent. Native audio selection
saves its stable device ID. Browser device IDs require a fresh native selection.
Changes apply on the next launch. Default windows are borderless 640x480 at a
60 FPS limit; saved size, frame limit and startup fullscreen are honored.
HDR stays Experimental and off by default. The native Windows D3D12 path
checks the current display's HDR state, the surface's float16 support and the
plugin's declared linear-output capability before selecting scRGB. It falls
back to SDR when unavailable. Mac Metal checks current display headroom and
selects EDR when available. Linux uses SDR with the pinned Dawn Vulkan
swapchain. Physical brightness and color remain unmeasured.
[Native HDR validation](VALIDATION-HDR.md)

`settings_probe`, `settings_ui_probe` and `tools/check_dawn_settings.py` cover
storage/migration, native controls and the next renderer launch. See
[settings validation](VALIDATION-SETTINGS.md) for the tested cases, atomic-save
fix and production integration limits.

`--vlc-pid <pid>` (alias `--player-pid`) selects player-supplied float PCM
without opening recording input. Recording-selection shortcuts are disabled
in that mode, and the window closes when its owning process exits. `--suite
<directory>` identifies the suite for nonce-protected menu-close requests.
The native Linux player watch requires pidfd support. See
[player ownership validation](VALIDATION-PLAYER.md) for the generated-PCM,
multi-window, owner-exit and cleanup checks. Actual VLC/Winamp dispatch and
playback validation remain checklist work.

`resource_probe` checks repeated renderer initialization/resize/destruction
against live GPU texture/buffer accounting and exercises the callback quota.
`capture_lifetime_probe` checks repeated context/error cleanup; explicit
`--record-input default` additionally opens recording streams. These are
development checks and save no audio. See [lifecycle validation](VALIDATION-LIFETIME.md)
for bounds, measured results and the remaining soak/release gates.

The window checker uses app-owned SDL events, not global desktop input, to
exercise resize/fullscreen restoration, fullscreen Escape/right-click/menu
closure and controlled Dawn device loss. Submission completion is polled
without blocking the event loop and holds at most two submitted frames.
Internal targets keep the 16-million-pixel allocation budget. Render views
and presentation bindings are cached until their source texture changes.

Mint's NVK adapter supports hardware offscreen rendering, but its current Xorg
session lacks DRI3. The host detects that specific presentation limitation
before configuring the swapchain and reports it cleanly. The window tests
pass with installed lavapipe using `--software` on the checker; that changes
only the test child's Vulkan driver environment. The user can still use the
Classic renderer. Wayland presentation has not been tested.

SDL window/surface references: [native window handles](https://wiki.libsdl.org/SDL2/SDL_GetWindowWMInfo),
[Metal view](https://wiki.libsdl.org/SDL2/SDL_Metal_CreateView).

## Native dependency notices

The native host source uses GPL-2.0-or-later. For the combined native host with
Apache-2.0 dependencies such as Abseil, distribute under the GPLv3 option and
retain component notices; `licenses/GPL-3.0.txt` supplies that license text.
See the [Apache compatibility explanation](https://www.apache.org/licenses/GPL-compatibility).
This does not replace the original engines' or individual source files'
licenses. The builder collects tracked vendored notices as well as separate
dependency-repository notices; final runtime notice staging remains a release
gate.

Authoritative references:

- https://dawn.googlesource.com/dawn/+/refs/heads/main/README.md
- https://dawn.googlesource.com/dawn/+/refs/heads/main/docs/quickstart-cmake.md
- https://github.com/google/dawn/releases/tag/v20261113.214538

## Package contract

The production module-only runtime now has a self-contained source snapshot
under `sdk/dawn`; all renderer repositories can build it independently with
`VIS_BUILD_DAWN_RUNTIME=ON`. See [VALIDATION-SDK.md](VALIDATION-SDK.md) and
`sdk/NATIVE-RUNTIME.md` for source/hash/build checks and remaining packaging gates.

The combined installer must contain one native runtime and plugin-only
components. Public standalone installers can include the native runtime for
independent use; they must not be embedded wholesale again in the combined
installer. Do not copy SDK archives, development libraries, PDBs, build caches
or unrelated backend tools into runtime payloads. Record measured compressed,
installed and temporary sizes against the 654,552,819-byte baseline.

Renderer parity, hardware support and release readiness require the tests in
todo.md; a successful device creation alone is insufficient.

Original-engine standalone capture now uses `VisualizerNative --capture-for`
on Windows/Linux, including native reselection/cancellation. Canonical hosts
have no Electron launch path for rendering, settings or capture. Mac checks
are deferred while macbook1 is offline; packaging/N-API removal remains a
release gate. See [VALIDATION-CAPTURE-SERVICE.md](VALIDATION-CAPTURE-SERVICE.md)
for streaming, real backend, single-writer, cleanup and sanitizer evidence.

## Prism Flow workbench

With the adjacent Geiss64 checkout, native builds now produce
`prismflow_dawn.dll` (Windows) or `prismflow_dawn.so` (Mac/Linux), using the
same API 4/shared Dawn contract as Terrain. Run the existing `dawn_window`
workbench with `--visualizer geiss --plugin <module> --assets <shader-dir>`;
the assets directory is `Geiss64/native/prismflow/shaders`. Standalone capture
uses `--line-in default` or `--select-input`; Ctrl-L/Command-L reselects it.
The public launcher now dispatches API 4 manifests to `VisualizerNative`;
installer migration remains open. See [VALIDATION-DISPATCH.md](VALIDATION-DISPATCH.md).

Drop a local image onto Prism Flow to derive a palette through native codecs
and a bounded background worker. A newer image supersedes older work; P
cancels pending import and randomizes the palette. Unsupported/corrupt images
report a recoverable notice. The optional image-control extension leaves base
API 4 plugins compatible. See NATIVE-API.md and the renderer's
`native/prismflow/VALIDATION-IMAGE-IMPORT.md` for bounds and measured coverage.

`prismflow_scene_probe` checks module/direct rendering, while
`prismflow_scene_reference_probe` compares complete preserved-renderer scenes.
See [VALIDATION-PRISMFLOW.md](VALIDATION-PRISMFLOW.md) for observed native window
behavior, current capture limitations and remaining work. Keep the reported
NVK cross-adapter/driver failures visible. The subsequent original-JavaScript
oracle on the same pinned native Dawn backend matches 24 presentation cases
and 36 full-scene frames exactly on Windows D3D12 and Linux NVK, lavapipe and
SwiftShader. See the renderer's
`native/prismflow/VALIDATION-NATIVE-REFERENCE.md` for reproduction and the
original-to-original cross-backend failures. This development-only Node runner
is excluded from production payloads; the new Metal run remains M08.
Native display capability/fallback checks are in VALIDATION-HDR.md; offscreen
linear-output tests do not demonstrate physical HDR presentation.
