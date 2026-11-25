# Visualizers for VLC

Native audio visualizers for VLC, standalone audio inputs, and Windows Winamp.
The combined installer installs a shared runtime and the visualizers you select.
Each visualizer also has its own download and source entry point.

## Downloads

| Component | Source and build instructions | Release |
| --- | --- | --- |
| Combined VLC add-on | [VLC-Visualizer-Addon](VLC-Visualizer-Addon/README.md) | [Full suite](https://github.com/christinate/Visualizers-for-VLC/releases/tag/v0.5.0-rc.1) |
| Prism Flow and Geiss Classic | [Geiss64](Geiss64/README.md) | [Prism Flow](https://github.com/christinate/Visualizers-for-VLC/releases/tag/prismflow64-v0.5.0-rc.1) |
| MilkDrop 2 | [MilkDrop2-64](MilkDrop2-64/README.md) | [MilkDrop 2](https://github.com/christinate/Visualizers-for-VLC/releases/tag/milkdrop2-64-v0.5.0-rc.1) |
| Rhythm Terrain | [Terrain64](Terrain64/README.md) | [Rhythm Terrain](https://github.com/christinate/Visualizers-for-VLC/releases/tag/terrain64-v0.5.0-rc.1) |
| Beach | [Beach](Beach/README.md) | [Beach](https://github.com/christinate/Visualizers-for-VLC/releases/tag/beach-v0.5.0-rc.1) |
| Objects | [Objects](Objects/README.md) | [Objects](https://github.com/christinate/Visualizers-for-VLC/releases/tag/objects-v0.5.0-rc.1) |
| UnderSea | [UnderSea](UnderSea/README.md) | [UnderSea](https://github.com/christinate/Visualizers-for-VLC/releases/tag/undersea-v0.5.0-rc.1) |
| CoralCanyon | [CoralCanyon](CoralCanyon/README.md) | [CoralCanyon](https://github.com/christinate/Visualizers-for-VLC/releases/tag/coralcanyon-v0.5.0-rc.1) |

Packages target Windows x64, Linux x64, and Apple Silicon. Windows VLC
integration requires VideoLAN VLC 3.0.24 AMD64. The Linux and macOS downloads
are archives containing `install.sh` and `uninstall.sh`. Run the scripts with
the same installation prefix. macOS binaries are ad-hoc signed.

### DualSubs for VLC

Select **DualSubs for VLC** in the full Windows installer to add two-track
text subtitles alongside the Visualizers menu. The checkbox starts off.
Use a stock VideoLAN VLC 3.0.24 AMD64 installation. If the separate DualSubs
overlay is installed, uninstall it first so the combined installer can back
up the original VLC files. For silent setup, use `/WITHDUALSUBS`.

Close VLC before removing the add-on from Windows Installed Apps. The
uninstaller verifies file ownership and restores the original VLC core, subtitle
plugins, menu, and launcher. Standalone renderer installers have separate
uninstallers. Remove any VLC-only standalone renderer packages before removing
their shared VLC add-on. The upstream DualSubs project provides separate
[Linux and macOS packages](https://github.com/christinate/DualSubs-for-VLC/releases/tag/v0.2.0).

## Build from source

### Shared tools and dependencies

Use Git, Python 3.11 or newer, CMake 3.24 or newer, and a native C++ compiler.
Use Windows x64 MSVC, Ninja, and NSIS for Windows packages. Build the pinned
Dawn package and SDL inputs before configuring a native target.

```text
python VLC-Visualizer-Addon/tools/build_dawn.py
```

On Windows run `python VLC-Visualizer-Addon/tools/prepare_windows_sdl.py`.
On Linux use SDL2 and Qt development libraries. On Apple Silicon use
[`build_macos_sdl.py`](VLC-Visualizer-Addon/tools/build_macos_sdl.py) for the
bundled SDL2 dylib. Set `Dawn_DIR` and `SDL2_DIR` to the resulting CMake
package directories. The [native runtime guide](VLC-Visualizer-Addon/sdk/NATIVE-RUNTIME.md)
lists the packaging inputs and platform layout.

### Combined VLC add-on

```text
cmake -S VLC-Visualizer-Addon -B build/addon -DVIS_BUILD_DAWN_RUNTIME=ON -DDawn_DIR=/path/to/Dawn -DSDL2_DIR=/path/to/SDL2
cmake --build build/addon --config Release
```

On Windows build the inline VLC menu with
`python VLC-Visualizer-Addon/tools/build_qt.py --vlc PATH_TO_VLC_3_0_24`.
See that script for the pinned MinGW and Qt versions. Build the legacy
MilkDrop Windows module separately before creating the combined package.
Run `VLC-Visualizer-Addon/tools/release.py --help` for the explicit host,
module, staging and dependency paths. Use
[`release_native.py`](VLC-Visualizer-Addon/tools/release_native.py) for Linux or
[`release_native_macos.py`](VLC-Visualizer-Addon/tools/release_native_macos.py) for
macOS. For the optional Windows overlay, follow the
[DualSubs build instructions](VLC-Visualizer-Addon/integration/qt/DUALSUBS.md).

### Build an individual visualizer

Each folder in the downloads table can configure its own build. It uses the
same pinned Dawn and SDL dependencies.

```text
cmake -S COMPONENT_FOLDER -B build/component -DVIS_BUILD_DAWN_RUNTIME=ON -DDawn_DIR=/path/to/Dawn -DSDL2_DIR=/path/to/SDL2
cmake --build build/component --config Release --target MODULE_TARGET
```

| Folder | Build target |
| --- | --- |
| `Geiss64` | `prismflow_native`; build `vis_geiss64` for Classic Geiss as well |
| `MilkDrop2-64` | `vis_milkdrop64`; Windows uses the Microsoft DXSDK.D3DX dependency, Unix uses vendored projectM |
| `Terrain64` | `terrain_native` |
| `Beach` | `beach_native` |
| `Objects` | `objects_native` |
| `UnderSea` | `undersea_native` |
| `CoralCanyon` | `coralcanyon_native` |

Build `VisualizerNative`, `Visualizers`, and `VisualizersCtl` when packaging
a standalone runtime. On Windows run the renderer's
`sdk/renderer-release.py`. For the five scenes that use shared Terrain code,
run `Terrain64/sdk/renderer-release.py --scene ID`. Linux and macOS use
`sdk/renderer-release-native.py` with the module and platform dependency paths.
The shared renderer folders include examples for their individual package
commands.

## Licenses

The root license covers the repository-level material. Each component
contains its own license and dependency notices. The Windows DualSubs
component uses the published DualSubs patch for VLC 3.0.24 and a merged Qt
menu. Its [corresponding source instructions](VLC-Visualizer-Addon/integration/qt/DUALSUBS.md)
identify the exact upstream inputs.
