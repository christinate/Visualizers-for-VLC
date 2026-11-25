# Beach

[Release downloads](https://github.com/christinate/Visualizers-for-VLC/releases/tag/beach-v0.5.0-rc.1) include standalone packages and a VLC plugin option. This build entry point uses the [shared native renderer](../Terrain64/native/) and [default settings](../Terrain64/assets/beach/beach.ini).

## Build independently

Run these commands from the repository root with the [pinned native dependencies](../README.md#shared-tools-and-dependencies).

```text
cmake -S Beach -B build/beach -DDawn_DIR=/path/to/Dawn -DSDL2_DIR=/path/to/SDL2
cmake --build build/beach --config Release --target beach_native
```

Build `VisualizerNative`, `Visualizers`, and `VisualizersCtl` as well if
you need to package a standalone runtime.

```text
python Terrain64/sdk/renderer-release.py --scene beach --help
python Terrain64/sdk/renderer-release-native.py --scene beach --help
```

The first command lists the Windows installer inputs. Its `--package-only`
option accepts the already compiled binaries. The second lists the Linux
and macOS archive inputs.
