# MilkDrop 2 64-bit

Native candidate: **0.5.0-rc.1**, on `feature/dawn-native`. Final releases are
pending validation. Each project keeps its own Git history, version and release
artifacts. Prior tags and packages remain preserved; no remote or release URL
is invented. The Addon repository's `todo.md` records the remaining gates.
Mac return validation resumed on 2026-11-17; remaining Mac gates stay open.
Fresh arm64 dependencies/binary drafts target macOS 11.0; execution is tested
on macOS 26.6.2 only. Older macOS and Intel Macs remain unqualified. Preserved
earlier drafts retain their macOS 26+ requirement.

Project 2 ports the supplied MilkDrop 2.25c renderer to native AMD64. Windows
retains the original Direct3D 9 engine and keys. macOS/Linux use the approved
projectM 4.1.8 OpenGL backend for `.milk` presets. Shared C++ code supplies
capture, device selection, settings and lifecycle handling; the native
production package excludes Electron.

HDR is **Experimental**, opt-in and off by default. Windows can expand the
original SDR image into scRGB on a supported HDR display; the original effect
buffers remain SDR. projectM output remains SDR. Physical HDR appearance is
unverified because no HDR display is available.

## Use

The Windows installer offers standalone, VLC and Winamp components. VLC needs
the compatible Addon first. Winamp loads a bridge matching its x86/x64
architecture; rendering stays in a separate 64-bit process. VLC-only installs
use the Addon's shared runtime instead of installing another Dawn copy.

Standalone mode opens an audio-input selector. Ctrl-L reopens it (Command-L
on macOS). A default input, exact device ID or exact case-insensitive label
can be selected at startup:

```text
Visualizers.exe --visualizer milkdrop --line-in default
Visualizers.exe --visualizer milkdrop --line-in "Recording device name"
Visualizers.exe --settings milkdrop
```

On Unix use `Visualizers` without `.exe`. Player-owned windows consume their
player's decoded PCM; source selection belongs to standalone mode. The
visualizer does not save recordings.

Windows open borderless at the configured size. Double-click toggles fullscreen
and restores the windowed bounds; the saved fullscreen preference controls
startup. Right-click or Escape closes the window. VLC provides per-visualizer
Open, Settings and Close, plus Close all and Rescan commands. Display changes
apply on the next launch. Uninstall retains user settings and refuses changed
owned files until they are repaired.

Windows keys include F1 for help, Space/Backspace for preset navigation and L
for the preset list. projectM uses Space/N for next preset, Backspace for
previous and L for lock/unlock. The original Windows preset editor is not
part of projectM. Presets/textures belong in the package's asset directories;
three suite-authored projectM presets are included.

## Validation and limits

Actual Windows VLC menu Open delivers decoded audio to the original engine;
owner-scoped CLI closure passes. Linux's actual menu-launched projectM runs
956 frames with decoded audio and bounded window/key checks. The remaining
Windows GUI/key checks, real Winamp UI, media matrix, final-source artifacts
and final Mac capture/stability work stay on the Addon checklist. Mac arm64
rebuilds, actual VLC/projectM Open/Close and PCM from ten generated formats,
window restoration and key fixtures pass.
The projectM patch restores its own 4.1.8 library metadata; both before and
after modules load, so this is not a claimed crash fix.

[STABILITY.md](STABILITY.md) records confirmed allocation and Unicode-bound
faults and fixes. Prior sanitizer/resize/init/quit/unload results retain their
original scope. They do not prove absence of every leak or explain historical
whole-system crashes.

Native build targets are `VisualizerNative`, `Visualizers`, `VisualizersCtl`
and `vis_milkdrop64`. projectM inputs are pinned in `dependencies.lock.json`.

## Build and package

Use this repository's pinned source SDK; an Addon checkout is not required.
See [sdk/NATIVE-RUNTIME.md](sdk/NATIVE-RUNTIME.md) for dependency setup,
Dawn provenance, build directories and platform packaging arguments.

Windows uses x64 MSVC, CMake, Python 3.11+, Git, NSIS and the pinned Dawn/SDL
inputs. Configure with `-DVIS_BUILD_DAWN_RUNTIME=ON`, the source-built
`Dawn_DIR` and `SDL2_DIR`. With those dependencies prepared:

For a public source checkout, obtain `Microsoft.DXSDK.D3DX` **9.29.952.8**
from [NuGet](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX/9.29.952.8).
Copy the package's native `include` and `release` directories into
`vendor/d3dx` so `vendor/d3dx/release/lib/x64/d3dx9.lib` is present.
Place the x64 `D3DX9_31.dll` from the Microsoft DirectX legacy runtime in
`vendor/d3dx/release/bin/x64`. The [provenance file](vendor/d3dx/legacy-compiler-provenance.json)
gives the tested version and checksum. Linux and macOS use projectM and
can build without these Windows dependencies.

```powershell
python sdk/renderer-release.py --dawn-install sdk/build/dawn-install --sdl-sdk sdk/build/sdl-sdk
```

Windows packages include D3DX9_31.dll for MilkDrop's original shader compiler,
D3DX9_43.dll and D3DCompiler_43.dll. The helpers reside beside the renderer;
the original shader flags are preserved. Runtime references are released after
their renderer/configuration owners finish.

The builder creates a native standalone runtime, plugin, notices and matching
x86/x64 Winamp bridges. It audits imports and refuses occupied staging/output
paths. `--package-only` accepts explicit existing build directories. Final
artifacts must come from the final committed source and pass the release gates.

Linux x86_64 and macOS arm64 archives use `sdk/renderer-release-native.py`;
supply host/runtime/module paths as documented in the SDK guide. Mac builds
also require `--sdl-library`; libraries are relocated and staging copies receive
locally verified ad-hoc signatures. VLC-only installation adds just the plugin
to the existing runtime. Final-source/platform QA remains open; Mac archives
are not Developer ID signed or notarized.

## Source archives and prior versions

From a clean committed checkout, run `python tools/source_release.py`.
It writes a fresh `releases/<VERSION>/source/` directory; `--output PATH`
selects another fresh location. Archives, source metadata and checksums bind to
one fixed Git commit. Nothing is published automatically.

The v0.4.0-rc.1 tag preserves the browser-based candidate; v0.3.0-rc.1 records
the earlier engine port. Use the matching tag and its README to reproduce
those packages. Their old validation does not qualify new native binaries.

## Licenses and attribution

MilkDrop derives from Nullsoft's BSD-licensed 2.25c source. Keep [LICENSE](LICENSE),
WDL/EEL, Microsoft D3DX redistribution terms and all dependency notices.
projectM/projectm-eval and their dependencies retain the LGPL, MIT and other
notices under `licenses/projectM`. Source is retained under `vendor/projectm`.
Original documentation: [MilkDrop](https://www.geisswerks.com/about_milkdrop.html).
The Addon's historical 0.2 installer detection remains unresolved; see its
SECURITY.md. It has not been classified as a false positive.
