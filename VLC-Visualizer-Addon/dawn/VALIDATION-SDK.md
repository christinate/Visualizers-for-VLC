# Independent native runtime and renderer builds

Observed 2026-11-16 on Windows AMD64 and Linux x86_64. Mac rebuild/validation
is deferred in todo.md M02 while macbook1 is offline. No installer is rebuilt
or published by this source/build milestone.

`Runtime.cmake` now defines the shared production libraries and module-only
host. The window path loads API 4 modules and has no linked Terrain renderer
or renderer source dependency. Production dispatch resolves module/assets
from current manifests. The workbench discovers its adjacent default module;
`--terrain-shader` remains an assets-directory convenience. Direct Terrain
and Prism Flow scene comparisons remain separate renderer probes.

`sdk/dawn` snapshots that same C++/CMake source, plus font/image services,
capture/backend sources, dependency pins, manifests and the native C headers.
The SDK includes the pinned miniaudio source/patch and source-build tools.
It excludes generated build directories from synchronization/hash indexes.
The host's GPLv3 option/license text accompanies Apache-2.0 dependencies.
Complete binary dependency/notice audits remain D50/D51 release gates.

`tools/check_sdk.py` verifies all 114 indexed source hashes in the Addon,
Geiss64, MilkDrop2-64 and Terrain64 snapshots. Independent roots can build
their runtime with `VIS_BUILD_DAWN_RUNTIME=ON` and the pinned Dawn/SDL SDKs.
Geiss and Terrain also build their own API 4 modules. MilkDrop retains its
original Windows engine and Mac/Linux projectM module. Existing legacy-only
builds remain available with the option off.

Observed builds:

- Windows: SDK-only runtime, Geiss runtime/Prism Flow module, Terrain runtime/
  module, and MilkDrop runtime all compile from their own source snapshots.
- Linux: SDK-only runtime, independent Prism Flow/Terrain modules, and the
  MilkDrop-root runtime compile. No renderer checkout is required by the
  SDK-only runtime. Modules hide their C++ implementation symbols and expose
  their documented C entry points.
- Independent Prism Flow's Windows module compares exactly against the direct
  renderer across 1,002,000 finite components, including supersampling,
  transitions, resize and Experimental linear offscreen output.
- Twelve production dispatch/settings/original-engine/player-ownership cases
  pass on Windows D3D12 and Linux lavapipe with independent modules. Windows
  Terrain's different DLL output location was corrected and the matrix rerun
  using the actual independent DLL; the first run was not used as evidence
  for that DLL.
- Five module-only workbench lifecycle cases pass on Windows and Linux:
  resize/fullscreen restoration, Escape, right-click, menu close and controlled
  device loss. Default module discovery loads API 4 without a built-in renderer.

The shared Threads target originally had directory-local visibility, which
blocked the separate Geiss module during CMake generation. Making the imported
target global corrects that independent-build failure. No renderer algorithm
or fixed pixel comparison limit was changed.

Evidence is in local `build/native-sdk-*` / `build/native-module-workbench-*`
directories and corresponding owner-machine directories under
`~/vlc-visualizers-work`. Existing NVK numerical/allocation findings remain
recorded, and this milestone does not claim physical HDR, actual Winamp/VLC UI,
extended-soak or installer/Defender completion. HDR remains Experimental/off
by default. Native packaging/N-API cleanup and release provenance remain open.
