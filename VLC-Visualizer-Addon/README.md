# VLC Visualizer Addon

Native candidate: **0.5.0-rc.1**, on `feature/dawn-native`. Final releases are
pending the checks in [todo.md](todo.md). Prior tags/artifacts remain preserved;
nothing has been published. Repository/release URLs await the owner.

The Addon adds **Visualizers** inline between View and Help in VLC's menu.
Each installed package has Open, Settings and Close; the menu also provides
Close all and Rescan. Rendering stays in separate 64-bit processes, fed by
VLC's decoded PCM. Packages are listed in a persistent JSON cache and checked
again before launch. Removing a module while a menu is open produces an error
instead of loading a stale path.

The combined Windows NSIS installer selects Prism Flow/Geiss Classic,
MilkDrop 2, Rhythm Terrain, Beach, Objects, UnderSea, and CoralCanyon by default. Each can
be deselected. The installer contains one shared native Dawn runtime and
plugin modules. The current package sizes are recorded in [RELEASES.md](RELEASES.md).
Select **DualSubs for VLC** to add the two-track subtitle menu to the Windows
installation. The checkbox starts off and requires original VLC 3.0.24 AMD64.
Uninstall the separately installed DualSubs overlay before running the combined
installer. See the [DualSubs build and testing instructions](integration/qt/DUALSUBS.md).

## Runtime and platforms

| Target | Current implementation | Validation status |
| --- | --- | --- |
| Windows AMD64, VLC 3.0.24 | Reversible launcher/private Qt interface; D3D12 modern renderers; original Classic/MilkDrop engines | Bounded actual menu/PCM checks and draft NSIS tests pass; final gates open |
| Linux x86_64, Mint VLC 3.0.20/Qt 5 | Inline Qt module; Vulkan modern renderers, Classic/projectM | Actual menu/PCM and lavapipe checks pass; broader matrix open |
| macOS arm64 | Cocoa integration, Metal, Classic/projectM | 60 actual menu/media checks, exact JS parity and bounded archive checks pass; capture/final gates open |

Intel Macs, Linux ARM, other distributions and Wayland are not qualified by
these checks. MilkDrop's original D3D9 engine is Windows-only. See
[dawn/README.md](dawn/README.md) and [dawn/NATIVE-API.md](dawn/NATIVE-API.md) for
architecture and ownership. Other VLC versions need separate matched builds
and validation; a 64-bit CPU alone does not establish compatibility.
Fresh Mac dependencies and binary drafts now target **macOS 11.0**, with that
floor verified in all packaged binaries and app metadata. Execution is tested
only on macOS 26.6.2; older macOS releases still need qualification. Preserved
earlier drafts retain their macOS 26+ requirement. See
[Mac deployment checks](dawn/VALIDATION-MAC-COMPATIBILITY.md).

Mint window checks use lavapipe. NVK window-surface support on that Xorg
session and an unsuppressed 64-byte NVK/Dawn allocation finding remain open;
offscreen pixel parity does not resolve either finding.

HDR is **Experimental**, opt-in and disabled by default. Native Windows
presentation checks the actual display/OS state, renderer and float16 surface
before scRGB, with SDR fallback. Original MilkDrop uses SDR expansion;
Metal uses Experimental EDR with explicit SDR fallback at headroom 1.0.
On Linux, the patched Dawn runtime uses extended-linear sRGB when the
Wayland compositor advertises the matching float16 Vulkan surface format.
Other Linux surfaces use SDR. The native transport was verified with a
headless Weston compositor. Physical panel output remains unmeasured.

## Install, scan and remove

Final native installers are not published. Development packages must retain
their QA status and exact hashes. Close the selected players before changing
their files. Windows supports the matched VLC 3.0.24 AMD64 installation and
uses a backup journal for exact restoration. Modified owned files prevent
uninstall until repaired. Settings are stored outside the installation.

The updated Windows setup recovers earlier add-on transactions when their files
still match the backup journal. It leaves independent renderer installations
in place. If VLC is running from the selected folder, setup asks permission to
close it before patching. The Windows launcher takes its icon from the backed-up
VLC executable.

After installing or removing a package, scan with the patched Windows launcher:

```powershell
& 'C:/Program Files/VideoLAN/VLC/vlc.exe' --scan-visualizers
```

The native 0.5 Windows launcher also supports the read-only
`vlc.exe --verify-original`, checking its backup identity/VLC metadata and
matching core version without opening the player. Older candidates lack this
flag. A wrapper copied as its own backup is refused before recursive launches.

Linux archives provide `install.sh`; use the package's documented `--prefix`
and component options. Start VLC through the installed `VlcVisualizerLauncher`
to activate this suite. Its `--scan-visualizers` command uses the same catalog.
Mac arm64 development archives use the same installation modes, with relocated
libraries and locally verified ad-hoc signatures. They are not notarized or a
final release. The original signed VLC app is preserved.
Third-party package/installer guidance lives in the separate Catalog project.

Windows open borderless at the saved size. Double-click toggles fullscreen
and restores windowed bounds; per-visualizer settings can start fullscreen.
Enable **VSync in fullscreen** in the visualizer's Settings and reopen it.
The setting applies whenever the window enters fullscreen. With a focused Terrain or Beach window,
**Space** advances to the next enabled camera. In Objects it advances to the
next enabled shape. In UnderSea, **Space** previews one timed event and **C**
switches the camera. Either key switches glides in CoralCanyon. Press **L**
to lock or unlock the current camera or
shape. A small lock or unlock badge fades out in the bottom-right corner.

VLC's **Visualizers → Settings** and Winamp's visualization plugin
**Configure** button open the same settings dialog. Terrain, Beach, Objects,
UnderSea, and CoralCanyon show their complete scene INI, including values supplied by the current
profile. The dialog saves scene changes to the per-user INI.

Escape/right-click closes the renderer. Standalone renderer packages provide
native audio-input selection and Ctrl-L/Command-L reselection; `--line-in`
accepts default, a device ID or an exact label. Player-owned windows use player
PCM and do not start recording. Visualizers do not save recordings.

## Build and package

Use x64 MSVC/CMake on Windows, the pinned source-built Dawn/SDL dependencies,
Python 3.11+, Git, NSIS and the matched MinGW/Qt interface toolchain. Configure
`-DVIS_BUILD_DAWN_RUNTIME=ON` with `Dawn_DIR`/`SDL2_DIR`; build `Visualizers`,
`VisualizersCtl`, `VlcVisualizerLauncher`, `VisualizerNative`, `qt_bootstrap`
and `vlc_visualizers`. `tools/build_qt.py` builds the separate inline Qt backend.

`tools/release.py` builds/stages the combined Windows package.
`tools/release_native.py --build-root PATH --suite NEW-PATH --output NEW-PATH`
packages independently built Linux inputs. Both use fresh paths, explicit
runtime plans, dependency/import audits and checksum output. See
[sdk/NATIVE-RUNTIME.md](sdk/NATIVE-RUNTIME.md) for the native build/package
contract. Each renderer repository also builds independently from its own SDK.
`tools/release_native_macos.py` stages/builds arm64 Mac archives from explicit
native inputs; `--dawn-install` and `--sdl-library` are required. Final-source,
capture and complete player/stability QA remain release gates.

From a clean committed checkout, `python tools/source_release.py` writes a fresh
`releases/<VERSION>/source/`; `--output PATH` selects another fresh directory.
Source archives and metadata bind to one commit and preserve prior outputs.
Remotes, final tags, source/binary artifacts and checksums remain release gates.

## Validation and local security evidence

- [Real VLC menus and PCM](dawn/VALIDATION-LIVE-MENU.md): actual Windows/Linux
  actions, projectM, a 3 h 20 min Terrain run and scoped resource observations.
- [Mac menus and media](dawn/VALIDATION-MAC-MENU.md): 60 actual AppKit/VLC
  checks, including 40 generated format/engine combinations and stale actions.
- [Guarded Windows draft](dawn/VALIDATION-PACKAGING-GUARD.md): 14 NSIS checks,
  eight installed backup-verification checks, local-only Defender scans and
  verified test-machine cleanup/restoration for one exact installer hash.
- [Catalog safety](dawn/VALIDATION-CATALOG-SAFETY.md): confirmed embedded-NUL
  and invalid-Unicode defects fixed, with bounded scanner/runtime regressions.
- [Original launcher guard](dawn/VALIDATION-LAUNCHER-GUARD.md): distinct backup,
  VLC/core metadata checks and rejected recursive-wrapper fixtures.

Ten generated audio-format fixtures pass through all four engines in actual
Mac VLC; Windows/Linux media and Winamp UI checks remain open. Final-source,
cross-platform and remaining soak
checks stay open. [STABILITY.md](STABILITY.md) records scope and confirmed faults.
Finite runs do not explain the historical whole-system crashes or establish
universal leak freedom.

The historical **0.2.0-rc.1** installer detection remains unresolved; see
[SECURITY.md](SECURITY.md). A working installation, generic executable-creation
rule or later candidate scan does not determine that alert to be a false positive.

Tools made no explicit external analysis upload. vpntest1's guarded-installer
analysis mode was verified local-only before transfer and restored after
candidate removal. Desktop reporting was not checked before earlier tests;
a later read finds automatic cloud/sample reporting enabled. Earlier automatic
transmissions are unverified, and further desktop binary tests wait for the
owner's privacy-mode choice. Do not extend vpntest1's assurance to those runs.
Current status is recorded in [release-status.json](release-status.json).

## History and licenses

The v0.4.0-rc.1 tag preserves the browser candidate; v0.3.0-rc.1 preserves the
earlier ports. Use their tagged READMEs to reproduce those packages. Their
results retain their original scope. No final native Mac release or new
platform qualification is implied by an older artifact.

Keep [LICENSE](LICENSE), all `licenses` notices and corresponding dependency
sources. The Addon/VLC interface is GPL-2.0-or-later, Qt is dynamically linked,
SDL2 uses zlib terms, and projectM/other dependencies retain their own notices.
- [Mac return validation](dawn/VALIDATION-MAC-RETURN.md): exact Metal/JavaScript
  parity, EDR fallback, actual VLC PCM and 18 native archive ownership checks.
