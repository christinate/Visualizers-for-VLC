# VLC Visualizer Catalog

Native migration candidate: **0.5.0-rc.1**, still under validation on
`feature/dawn-native`. Prior 0.4.0-rc.1 artifacts are preserved. Final native
installers/releases are pending. Mac return validation resumed on 2026-11-17;
see the addon `todo.md` for remaining release gates.

Source archives require Python 3.11+ and Git. From a clean, committed checkout,
run `python tools/source_release.py`. Each run uses a fresh
`releases/<VERSION>/source/` directory; `--output PATH` selects another fresh
location. Archives, metadata and checksums bind to one fixed commit, and earlier
release files are retained. The generated artifacts remain local for review.


Native Dawn migration is underway on `feature/dawn-native`. The shared host
and API 4 dispatcher pass generated-PCM integration checks and bounded actual
VLC activation/PCM checks on Windows/Linux/Mac. Mac arm64 archive ownership and
exact Metal parity also pass. Mac passes 60 actual menu/media checks,
including stale Open/Settings actions, symlink escapes and all four engines
with ten generated formats. Real capture/manual input and the
remaining real-player/final-source gates remain open. The
Addon repository's `todo.md` tracks remaining integration and release gates.
See [DAWN.md](DAWN.md) for the native schema, buildable plugin and plugin-only
installer example. `visualizers.json` separates development capabilities from
the preserved 0.4.0-rc.1 artifacts; final native installers remain pending.

> The combined **VLC Visualizer Addon 0.2.0-rc.1 installer is on security
> hold** after a reported Defender quarantine. See Project 3's SECURITY.md.
> This detection has not been determined to be a false positive.

Project 4 maintains the JSON list of visualizers implementing VLC Visualizer
Addon APIs 1 (legacy Windows), 2 (native OpenGL), 3 (preserved browser WebGPU),
and the development native Dawn API 4, their package schemas and installer guidance. It has an
independent Git repository, version, release archive, and checksum file.

`visualizers.json` is the public compatibility directory. It does not silently
download or execute plugins. Repository/download URLs are deliberately `null`
until the project remotes and published releases are supplied.

The addon maintains a separate **local installed catalog** at
`VLC/visualizers/installed-visualizers.json`. This cache persists between VLC
launches. Adding this project's public JSON entry does not install a renderer.

API 3 packages and their installer contract are documented in [WEBGPU.md](WEBGPU.md).
The current development catalog includes Prism Flow, MilkDrop 2 and Rhythm Terrain.

## Current native API 4 packages

Use [DAWN.md](DAWN.md), [manifest-v4.schema.json](schemas/manifest-v4.schema.json)
and the buildable [dawn-visualizer example](examples/dawn-visualizer) for new
native Dawn modules. Each plugin has one unique `plugins/<id>` directory,
its architecture-matched module, contained assets and notices. The suite owns
one shared native runtime/Dawn implementation; plugin-only installers must
not replace or duplicate it.

An installer copies its owned package, then runs the suite launcher's
`--scan-visualizers` command (or `VisualizersCtl scan --suite PATH`). It rescans
after removal as well. Cache entries persist across player launches and are
revalidated before both Open and Settings. Scanning never executes the module.
NUL paths, malformed Unicode and resolved path escapes have bounded regression
coverage; see the Addon's catalog-safety report for exact scope.

The following interface sections retain the API 1/2 integration contracts.
API 3 describes the preserved browser candidate; the native 0.5 production
runtime includes no browser engine. Public catalog records of an older artifact
do not make it loadable as an API 4 module.

## Installable package layout

```text
VLC/
  vlc.exe                         # patched, reversible launcher
  visualizers/
    Visualizers.exe
    VisualizersCtl.exe
    installed-visualizers.json
    plugins/
      example_visualizer/
        manifest.json
        vis_example64.dll
        assets/...
        licenses/...
```

An installer copies its own files into one unique package directory and runs:

```powershell
& 'C:\Program Files\VideoLAN\VLC\vlc.exe' --scan-visualizers
```

The command scans package manifests, checks API version and AMD64 PE format,
rejects invalid paths and packages, and atomically replaces the persistent
catalog. It does **not** load renderer DLLs. Concurrent scans use a file lock.
It requires write access to the VLC visualizers directory; installers targeting
Program Files should request administrator rights before copying and scanning.
The installed menu reads the cache on opening; updates also appear next launch.
After removal, run the same command to remove stale entries. Until rescanning,
missing packages remain listed but disabled. Clicking an entry also validates
its package again before starting the host, so a DLL removed while a menu is
open cannot be loaded through a stale path.

See `examples/installer.nsi` for a minimal NSIS installer/uninstaller. Supply your
own DLL before compiling the example. The two real renderer projects include
complete installers with independent VLC/Winamp choices and matching bridges.

## Renderer interface

The rendering DLL must be native AMD64. API 1 currently uses the legacy Winamp
visualization structure for Init/Render/Quit and 576-sample signed 8-bit stereo
waveforms at 44.1 kHz, plus spectrum data. Export:

```cpp
extern "C" __declspec(dllexport)
winampVisHeader* winampVisGetHeader(HWND protocolParent);
extern "C" __declspec(dllexport)
const unsigned char* SuiteGetFrame(int* width, int* height);
extern "C" __declspec(dllexport) HWND SuiteWindow();
extern "C" __declspec(dllexport) void SuiteSetSize(int width, int height); // optional
```

For `adapter=winamp`, `SuiteWindow()` returns the renderer's native input window;
the host hides it, forwards keys to its original procedure, and presents frames
in a separate borderless window. `SuiteGetFrame` returns top-down contiguous
BGRA8 pixels, width × 4 bytes per row, valid until the next renderer callback.

For `adapter=geiss`, export `SuiteKey(UINT, WPARAM, LPARAM)` instead of
`SuiteWindow`. The protocol parent is separate from the visible host to prevent
legacy forwarded-key loops. Post `WM_APP + 42` to it to request fullscreen.

The host owns display size, fullscreen state, settings persistence, SDR/scRGB
presentation, process lifetime, and VLC audio transport. Keep renderer resource
ownership inside its DLL and release it in Quit/destructors. Each renderer runs
in a separate process from VLC. Settings are per visualizer under the user's
Local AppData directory; do not store mutable user settings in Program Files.

See `schemas/manifest.schema.json` and `examples/example-visualizer/manifest.json`.
The current two adapter names describe their input-routing contracts; they do
not imply that arbitrary unmodified Winamp DLLs implement the frame export.

## Add an entry and release

Add a unique ID, version, upstream attribution, license, tested API/architecture,
and HTTPS repository/installer URL when available. Mark unverified submissions
as release candidates. Do not label an entry tested based only on compilation.

```powershell
python tools/validate.py
python tools/release.py
```

Commit the intended source before packaging. The catalog ZIP includes its
documented tools; the packager validates that delivered snapshot and records
its fixed commit, archive hash and candidate status in RELEASE.json. Use fresh
`--output` and `--stage` paths for additional drafts. Eight isolated actual-Git
cases cover dirty sources, ignored output, preserved destinations, invalid
catalogs and concurrent HEAD changes; see
[VALIDATION-RELEASE.md](VALIDATION-RELEASE.md).

Release output is in `releases`, with SHA-256 checksums. The example DLL is not
provided: this repository is a compatibility catalog and developer guide.

## Native API 2: macOS and Linux

`schemas/manifest-v2.schema.json` describes native packages. API 1 remains
supported unchanged on Windows; an API-1 DLL is not a native API-2 library.
Use `examples/manifest-native.json` and `examples/visualizer_api.h`, then build
`examples/native-visualizer` as a small working OpenGL example:

```sh
cmake -S examples/native-visualizer -B build/native-example -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-example --parallel
```

The host owns a borderless SDL2 window and current OpenGL context (Linux 3.3
core or macOS 4.1 core). All callbacks run on its rendering thread. Export
`VisualizerGetApi(2)` with C linkage and a static, size/version-tagged table.
`create` receives drawable pixel dimensions and the package's UTF-8 assets
path. The library owns its instance and must release all CPU/GPU resources in
`destroy` before returning. Never throw an exception through a callback; return
null from `create` or a nonzero result from `render` on failure. Do not terminate
VLC or manipulate its windows from a renderer.

`render` receives 576 stereo float PCM frames at 44.1 kHz and elapsed seconds.
Audio pointers are borrowed until the callback returns. `resize` updates
presentation dimensions; `key` receives SDL key symbols and a down/up flag.
`key_state` is optional diagnostic state for a reversible toggle, used by the
suite's L-key self-test. Do not retain host pointers or pass C++ allocator-owned
objects across the ABI. Build the same architecture as the host: x86_64 on the
tested Linux package, arm64 on the tested Mac package. Fat/universal Mach-O
libraries are not currently accepted; distribute a matching thin library.

A native package uses `platform=linux` or `macos`, `architecture=x86_64` or
`arm64`, `adapter=opengl`, and an `.so` or `.dylib` entrypoint. Its directory name
must equal its unique manifest ID. Dependent libraries should live beside the
entrypoint, using `$ORIGIN` on Linux or `@loader_path` on macOS. Keep source,
license notices and required assets with your distribution. Native HDR is not
implemented in this release; use `hdr_output=sdr`.

Default suite directories:

| Platform | Directory |
| --- | --- |
| Linux | `${XDG_DATA_HOME:-$HOME/.local/share}/vlc-visualizers` |
| macOS | `$HOME/Library/Application Support/VLCVisualizers` |

Your installer copies only its own package files into
`SUITE/plugins/your_unique_id`, with the manifest at that directory's root.
Require the addon first. Avoid replacing the addon runtime, core VLC files or
another package's files. Track every file your installer owns; on uninstall,
remove those files, retain user presets and modified files, then rescan:

```sh
"$suite/VlcVisualizerLauncher" --scan-visualizers
# equivalent control API, useful for installers
"$suite/VisualizersCtl" scan --suite "$suite"
```

The addon installer has its own component snapshot/backup journal; it is not
a general installer service for third-party IDs. Build your own installer with
the ownership rules above. `--prefix` on the suite's installers allows staging
and testing without changing the real installed suite. On macOS sign your own
binaries; do not alter or re-sign the original VLC app. The addon's launcher
activates the native interface module while preserving VLC's installation.

The public compatibility list records actual tested platform/version pairs.
The current tests cover Windows VLC 3.0.24 AMD64, Mint 22.3/VLC 3.0.20 x86_64
under X11, and M1/macOS 26.6.2/VLC 3.0.24 arm64. New platforms need their own
build and tests; a catalog entry alone is not evidence of compatibility.
