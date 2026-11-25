# Prism Flow 64-bit

Native candidate: **0.5.0-rc.1**, on `feature/dawn-native`. Final releases are
pending validation. Each project keeps its own Git history, version and release
artifacts. Prior tags and packages remain preserved; no remote or release URL
is invented. The Addon repository's `todo.md` records the remaining gates.
Mac return validation resumed on 2026-11-17; remaining Mac gates stay open.
Fresh arm64 dependencies/binary drafts target macOS 11.0; execution is tested
on macOS 26.6.2 only. Older macOS and Intel Macs remain unqualified. Preserved
earlier drafts retain their macOS 26+ requirement.

Project 1 provides **Prism Flow**, the native Dawn renderer derived from the
Geiss HDR engine, and **Geiss Classic**, the compatibility CPU renderer.
The repository name and package ID `geiss` remain for upgrade compatibility.
“Geiss” identifies the original project/Classic engine and attribution;
Prism Flow is this modern renderer's name.

Prism Flow retains RGBA16F feedback, four blended dynamic motions/eight
transition slots, preset history, waveform response, keyboard controls and
1×/2× supersampling. Supersampling falls back to 1× above the internal-pixel
limit. Settings choose modern or Classic mode. Windows retains the original
Geiss 4.30 engine; the portable Classic engine serves macOS/Linux.

HDR is **Experimental**, opt-in and off by default. Windows D3D12 checks the
actual display/OS state, float16 surface and renderer capability before scRGB;
otherwise it presents SDR. Mac Metal has Experimental EDR with SDR fallback at headroom 1.0;
Linux native HDR transport remains open.
Physical HDR appearance has not been tested; no display is available.

## Use

The Windows installer offers standalone, VLC and Winamp components. VLC needs
the compatible Addon first. Winamp loads a bridge matching its x86/x64
architecture; rendering stays in a separate 64-bit process. VLC-only installs
use the Addon's shared runtime instead of installing another Dawn copy.

Standalone mode opens an audio-input selector. Ctrl-L reopens it (Command-L
on macOS). A default input, exact device ID or exact case-insensitive label
can be selected at startup:

```text
Visualizers.exe --visualizer geiss --line-in default
Visualizers.exe --visualizer geiss --line-in "Recording device name"
Visualizers.exe --settings geiss
```

On Unix use `Visualizers` without `.exe`. Player-owned windows consume their
player's decoded PCM; source selection belongs to standalone mode. The
visualizer does not save recordings.

Portable Classic keys include Space/N for a new map, L for lock, W for waveform,
I for color shifting and Q/U/A/E/Y/G/D for effects. The original Windows
Classic key handling is retained. Modern controller/reference details are in
[native/prismflow/README.md](native/prismflow/README.md).

Windows open borderless at the configured size. Double-click toggles fullscreen
and restores the windowed bounds; the saved fullscreen preference controls
startup. Right-click or Escape closes the window. VLC provides per-visualizer
Open, Settings and Close, plus Close all and Rescan commands. Display changes
apply on the next launch. Uninstall retains user settings and refuses changed
owned files until they are repaired.

## Validation and limits

Original JavaScript and C++ match all 24 presentation cases and 36 scene frames
on each tested D3D12/NVK/lavapipe/SwiftShader/Metal backend. See
[native/prismflow/VALIDATION-NATIVE-REFERENCE.md](native/prismflow/VALIDATION-NATIVE-REFERENCE.md)
for exact scope and retained cross-backend findings. The reference binding and
Node are development tools, excluded from production payloads. Metal passes
24 presentation cases and 36 scene frames exactly on Apple M1.

Actual VLC menu Open/Close and decoded audio pass bounded Windows, Linux and
Mac checks. Mac also passes ten generated formats through both modern and
Classic engines. Real Winamp UI, remaining media checks, final-source soaks and final
installers remain open. Finite tests do not identify the old whole-computer
crash or establish universal leak freedom. See [STABILITY.md](STABILITY.md).

Native build targets are `VisualizerNative`, `prismflow_native`, `Visualizers`
and `VisualizersCtl`; retain `vis_geiss64` for Classic.

## Build and package

Use this repository's pinned source SDK; an Addon checkout is not required.
See [sdk/NATIVE-RUNTIME.md](sdk/NATIVE-RUNTIME.md) for dependency setup,
Dawn provenance, build directories and platform packaging arguments.

Windows uses x64 MSVC, CMake, Python 3.11+, Git, NSIS and the pinned Dawn/SDL
inputs. Configure with `-DVIS_BUILD_DAWN_RUNTIME=ON`, the source-built
`Dawn_DIR` and `SDL2_DIR`. With those dependencies prepared:

```powershell
python sdk/renderer-release.py --dawn-install sdk/build/dawn-install --sdl-sdk sdk/build/sdl-sdk
```

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

## Licenses and upstream

Classic Geiss is BSD-3-Clause. The Geiss HDR-derived assets retain their
Apache-2.0 LICENSE, NOTICE and output permissions under `assets/prismflow`.
Keep the shared SDK/runtime and dependency notices as well as [LICENSE](LICENSE).
Original source: [geissomatik/geiss](https://github.com/geissomatik/geiss/).
The Addon's historical 0.2 installer detection remains unresolved; see its
SECURITY.md. It has not been classified as a false positive.
