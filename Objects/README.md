# Objects

[Release downloads](https://github.com/christinate/Visualizers-for-VLC/releases/tag/objects-v0.5.0-rc.1) include standalone packages and a VLC plugin option. This build entry point uses the [shared native renderer](../Terrain64/native/) and [default settings](../Terrain64/assets/objects/objects.ini).

## Build independently

Run these commands from the repository root with the [pinned native dependencies](../README.md#shared-tools-and-dependencies).

```text
cmake -S Objects -B build/objects -DDawn_DIR=/path/to/Dawn -DSDL2_DIR=/path/to/SDL2
cmake --build build/objects --config Release --target objects_native
```

Build `VisualizerNative`, `Visualizers`, and `VisualizersCtl` as well if
you need to package a standalone runtime.

```text
python Terrain64/sdk/renderer-release.py --scene objects --help
python Terrain64/sdk/renderer-release-native.py --scene objects --help
```

The first command lists the Windows installer inputs. Its `--package-only`
option accepts the already compiled binaries. The second lists the Linux
and macOS archive inputs.
