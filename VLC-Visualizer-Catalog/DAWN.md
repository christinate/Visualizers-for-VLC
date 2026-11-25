# Native Dawn plugin packages (API 4)

This is the `feature/dawn-native` development contract. API 4 dispatch passes
local Windows AMD64/D3D12, macOS arm64/Metal and Linux x86_64/lavapipe checks.
Bounded actual Windows/Linux VLC playback/menu and draft installer checks also
pass. Full media/Winamp UI, current Mac return work and final-source/release
validation stay open. The preserved 0.4.0-rc.1 artifacts use their original APIs; they
cannot load a new API 4 module. `visualizers.json` keeps those release entries
and separately labels each entry's development capabilities. No native
installer or repository URL is invented.

## Package and runtime

```text
visualizers/
  Visualizers[.exe]                  # existing production frontend
  VisualizersCtl[.exe]               # scanner; never executes modules
  native/
    VisualizerNative[.exe or .app]
    webgpu_dawn / SDL and platform runtime dependencies
  installed-visualizers.json
  plugins/
    audio_color/
      manifest.json
      audio_color.dll                # .so on Linux; .so/.dylib on Mac
      assets/...
      LICENSE.txt
```

The suite owns **one shared source-built Dawn implementation**. A plugin-only
installer adds files under its own unique ID directory. It must not install
Electron, Node, browser audio code, a private Dawn build or copies of the
central native runtime. A public standalone installer may include a compatible
shared runtime, with its dependencies and notices; the combined suite must
not embed those standalone installers wholesale. Windows/Linux/macOS arm64 native builders
implement this ownership plan; final-source and full platform checks remain
Addon D50-D54 work.

[`schemas/manifest-v4.schema.json`](schemas/manifest-v4.schema.json) defines
the portable manifest. `entrypoint` names the architecture-matched module;
`assets` names a contained directory, which must exist. Use forward slashes,
no absolute paths or parent traversal. The directory name and `id` must match.
Names are limited to 128 UTF-8 bytes by the runtime. Schema platform/architecture
pairs describe currently tested targets; do not infer support for other CPUs.
The exact `dawn_revision` pin is checked by both scanner and module loader.

Optional `classic` selects an existing API 1 Windows or API 2 OpenGL module
for `--classic` / the saved Geiss engine preference. It does not convert an
arbitrary Winamp DLL. Windows MilkDrop retains its original engine; Mac/Linux
use projectM. HDR remains Experimental/off by default. Float feedback is
independent of display HDR. A package that supports the native linear-output
creation flag declares `settings.hdr_output: "native-float"`. Linear RGB uses
203-nit reference white. Missing/other values keep the package in SDR. Keep
the SDR-only example's declaration unchanged unless its renderer implements
linear output. The host checks the OS display and native surface separately;
the manifest declaration alone never enables HDR. Current Windows D3D12 has
Experimental scRGB presentation; Mac Metal uses Experimental EDR with SDR
fallback at headroom 1.0. Linux native HDR transport remains open and
falls back to SDR. Physical HDR display output has not been verified.

## Build the example

The MIT-licensed [`examples/dawn-visualizer`](examples/dawn-visualizer)
contains a complete small module. It clears an RGBA16F image with brightness
from RMS, changes color every four beats and handles Space to change color.
It creates no asynchronous callbacks, queues or background threads.

First build the Addon's pinned shared Dawn dependency. With the Catalog and
Addon checkouts adjacent, from this repository:

```text
cmake -S examples/dawn-visualizer -B build/dawn-example -DADDON_SOURCE=../VLC-Visualizer-Addon -DDawn_DIR=../VLC-Visualizer-Addon/build/dawn-install/lib/cmake/Dawn
cmake --build build/dawn-example --config Release
```

Select an x64 generator on Windows (`-A x64` for Visual Studio). On Mac build
arm64; Linux currently targets x86_64. Use the actual printed `Dawn_DIR` if
the SDK installs into `lib64`. Output is `build/dawn-example/package/audio_color`:
module, platform-specific manifest, assets and license. No runtime is copied.
On Mac, use a matching `@rpath/libwebgpu_dawn.dylib` install name or resolve the
dependency to `@loader_path/../../native/libwebgpu_dawn.dylib` in your staged
module, then sign your module. Do not re-sign installed VLC or modify the
central runtime. Linux's example uses `$ORIGIN/../../native`.

Export `VisualizerGetNativeApi(uint32_t)` using
`VLC-Visualizer-Addon/sdk/native/visualizer_dawn_api.h`. The base C table remains
API 4; optional image commands use the separate control extension header.
Return NULL for unsupported versions. Match structure sizes, ID, revision
and the canonical shared `wgpuGetProcAddress` implementation.

The host provides analyzed float waveform/spectrum/RMS/beat data. Borrowed
GPU handles need AddRef/Release if retained; never Destroy the host's device,
replace its callbacks, pass STL ownership across the ABI, or let an exception
escape a callback. Keep allocation/free pairs inside the module. Return a
borrowed RGBA16F texture with its real dimensions and encoding; release your
resources in destroy. The host retains the module through GPU teardown.
Track any future callbacks exactly as specified in the Addon's
`dawn/NATIVE-API.md`, including constructor failures. The example uses only
synchronous resource creation and queue submission, so has no futures to track.

Shared native settings expose display size, fullscreen and relevant supported
host controls; unknown plugin JSON is preserved. This ABI does not provide
arbitrary custom settings-dialog widgets. Validate your own stored JSON and
document renderer-specific controls. Standalone recording/source selection
is owned by the native host; Ctrl-L/Command-L reopens its device list, and
`--line-in default`, an exact ID or unambiguous label selects startup input.
Player-owned windows use the player's PCM and cannot switch to recording.
Escape/right-click/menu closure and double-click fullscreen belong to the host.

## Install, scan and remove

On Windows the plugin-only NSIS example is
[`examples/dawn-visualizer/installer.nsi`](examples/dawn-visualizer/installer.nsi).
Build the module first, then run NSIS with absolute output/payload paths:

```text
makensis /DPACKAGE_DIR=C:\path\to\build\dawn-example\package\audio_color /DOUTPUT_FILE=C:\path\to\build\AudioColorExample-setup.exe examples/dawn-visualizer/installer.nsi
```

It requires an existing native Addon installation, refuses an occupied package
directory, copies only its module/manifest/assets/license, checks compatibility
with `VisualizersCtl check`, then runs:

```text
vlc.exe --scan-visualizers
```

Its uninstaller checks every owned file against bytes embedded in that exact
uninstaller before removing anything. Changed files are preserved; extra user
files keep the directory from being removed. The example does not support an
in-place upgrade: uninstall the existing package first. Larger installers can
use a journal and native streaming hashes instead of the example's byte reads.
Installer runtime/rollback/security checks remain release QA; compilation is
not proof of Defender clearance.

On Mac/Linux, an installer uses the same package layout in the installed suite.
Copy only to an unused `plugins/<id>` directory; retain file hashes and ownership
for safe uninstall. Check before registration:

```text
VisualizersCtl check --suite /absolute/suite --package /absolute/suite/plugins/audio_color
VlcVisualizerLauncher --scan-visualizers
```

For a development-only check, `VisualizersCtl scan --suite /absolute/suite`
uses the same scanner without starting VLC. Scans replace the local registry
atomically under a lock. They check paths, native format/CPU, manifest API and
revision without executing a module. A menu click and the native runner each
validate the current package again; the loader also checks the exported ABI.
After removing your owned files, scan again to remove the entry persistently.
Do not delete or rewrite another plugin, central runtime, VLC executable or
the Addon's backup journal. The Addon installer owns VLC integration/restoration.

Test installation, scan persistence, settings, generated and real player audio,
reselection, fullscreen/resize, missing-file rejection, rapid close and clean
uninstall on each claimed target. Test original-engine behavior separately.
Publish only source-matched binary packages with the required dependency
notices and explicit tested/unsupported platforms. Use `python tools/validate.py`
for this repository's schemas/examples/catalog; it makes no network requests.
