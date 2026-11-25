# Build the optional Windows DualSubs component

The combined installer can install the DualSubs v0.2.0 overlay for
VLC 3.0.24 AMD64. The add-on wraps the standard VLC executable and loads
a Qt backend with both menus when DualSubs is selected. The same transaction
tracks the launcher, Qt menu, VLC libraries, subtitle codecs, and selected
visualizers. Uninstall restores the verified originals.

Download the [DualSubs v0.2.0 Windows overlay installer](https://github.com/christinate/DualSubs-for-VLC/releases/tag/v0.2.0).
Clone its [source repository](https://github.com/christinate/DualSubs-for-VLC)
at tag `v0.2.0`. Use `menus.cpp` and `menus.hpp` from
`modules/gui/qt` in the [VLC 3.0.24 source release](https://download.videolan.org/pub/videolan/vlc/3.0.24/).
These files are the merge base for the two menus.

From this project directory, using 7-Zip, Python, Git, CMake, Ninja, and the
MinGW/Qt tools documented in [build_qt.py](../../tools/build_qt.py), run:

```powershell
python tools/prepare_dualsubs.py --installer PATH_TO_DUALSUBS_EXE `
    --dualsubs-source PATH_TO_DUALSUBS_CHECKOUT --vlc-menus PATH_TO_STOCK_QT_SOURCES `
    --output build/dualsubs-stage
python tools/build_qt.py --source build/dualsubs-stage/qt --vlc build/dualsubs-stage/vlc `
    --build-dir build/qt-dualsubs
```

The preparation script verifies the pinned release and stock source hashes.
It extracts the overlay data from the installer and merges the menu source
from commit `b3b42d9bc6f1fa3408c6bcc8cfde58fdfb87e07b` with the vendored
visualizer Qt source.

Build the regular Qt backend and native add-on first. Add these arguments to
your Windows combined installer build:

```powershell
python tools/release.py --package-only --host-build PATH_TO_HOST_BIN `
    --native-build PATH_TO_NATIVE_BIN --geiss-build PATH_TO_GEISS_BIN `
    --terrain-build PATH_TO_SCENE_BIN --dawn-install PATH_TO_DAWN `
    --sdl-sdk PATH_TO_SDL_SDK --stage NEW_STAGE --output NEW_OUTPUT `
    --dualsubs-stage build/dualsubs-stage --dualsubs-qt-module build/qt-dualsubs/libqt_plugin.dll
```

The checkbox starts off. Select it in the components page or use
`/WITHDUALSUBS` in silent setup. If VLC already has the upstream standalone
overlay, uninstall it before running the combined installer. The combined
release includes the DualSubs license and source provenance.

To exercise install, selection changes, and restore on disposable copies of
original VLC files:

```powershell
python tools/test_dualsubs_transaction.py --payload NEW_STAGE/payload `
    --stock-vlc PATH_TO_OFFICIAL_VLC_3_0_24_AMD64 --report build/dualsubs-test.json
```
