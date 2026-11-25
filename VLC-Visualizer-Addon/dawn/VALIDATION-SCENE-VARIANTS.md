# Terrain, Beach and Objects validation

## Fullscreen VSync and coastal effects update

The Windows build in `build/coastal-update/` enables optional VSync on
fullscreen entry and switches back to the surface's windowed present mode
on exit. A compositor that exclusively offers FIFO still controls its timing.
The shared native host and original-engine hosts apply the fullscreen rule.
All three scene packages rendered 90 frames on the Windows hardware adapter
and switched Immediate → FIFO → Immediate. Terrain also passed with
Microsoft WARP, starting in fullscreen and switching FIFO → Immediate →
FIFO → Immediate. Each window restored its bounds after fullscreen.

Scene tests submit L through each packaged native plugin's key handler and
verify both badge colors in the bottom-right pixels, badge expiry, and
resumed mode cycling. The Beach controller verifies five progressively
closer impact targets, overlapping spray, and building growth and recovery.
The GPU renders all nine camera angles and captures the approaching surf.
The complete packaged INI includes the mountain, spray, building and tide
settings for the shared player settings editor.
The packages, checksums and preview are in `build/coastal-update/INSTALLERS.md`.

## Scene controls and Windows setup update

The four updated Windows downloads and their hashes are indexed in
`build/scene-controls-win/INSTALLERS.md`. The combined payload and the three
standalone payloads contain the same tested native host. The standalone
archives include x86 and x64 Winamp bridges. All six Configure callbacks
dispatch the intended scene to the shared settings entry point.

The native Windows settings test round-trips the VSync checkbox and a
multiline scene INI edit. Each packaged scene opens the production settings
entry point with its INI editor. Terrain, Beach and Objects presented 90
frames each with FIFO presentation on the hardware adapter. An explicit
Microsoft WARP run presented 90 Terrain frames with the same setting.
The initial forced-software run showed an adapter error because the pinned
Dawn Direct3D backend does not honor that request flag. The host now selects
WARP through the DXGI adapter identifier.

The GPU probe rendered nine Beach cameras, checked diagnostic rain spawns
above each enabled view, and exercised eight Terrain angles and their
transitions with saturated waveform history. It also checked manual mode
changes and Objects' startup selection against the enabled shape list.

The Windows setup controller passed disposable VLC 3.0.24 AMD64 trials for
stock patching, exact icon resource copying, byte-identical restoration,
previous-install recovery, stock refresh and independent renderer retention.
Modified files were preserved when recovery was refused. The process test
closed the player under the selected folder and left a same-name process in
another folder running. A disposable copy of the NSIS flow installed and
scanned all five default components with system registry actions disabled.

## Previous scene release

The 0.5.0-rc.1 downloads in `build/scene-variants-final/` use the committed
scene and integration sources. `INSTALLERS.md` links the combined and
standalone packages. `installer-index.json` records hashes and source archive
commits. The earlier window-recovery candidate and preserved releases retain
their separate paths and checksums.

## Scene behavior

- **Rhythm Terrain** has eight selectable cameras. The default height and
  wall-width pulses both jump to 200% and decay to 100% over 0.5 seconds.
  The controller probe verifies exact endpoints, a custom 0.75-second wall
  decay, angle gating and disabled effects.
- **Beach** follows waves from ocean to sand, with a procedural shore and
  independently enabled scenery. The probe verifies random wave launches at
  0.3–0.7-second intervals, retreat, the beat breaker and rock curl, separate
  low/high splash radii, rain toward the horizon and on scenery, and both
  beat-driven and 60-second palette rotation.
- **Objects** cycles through an armillary, gyroid, knot, prism and satellites.
  The probe verifies object selection and disabling, beat size and recovery,
  waveform rotation, and separate audio-band twist. It renders all five
  shapes through the actual native plugin ABI. The renderer uses original
  signed-distance fields inspired by the isosurface approach documented by
  [GPUCaster](https://www.geisswerks.com/gpucaster/tutorial.txt).

Each package contains its complete scene INI next to its shader. The host
reads a same-name profile override when opening the scene. Actual player
tests on Mint and Mac verify three valid overrides and three invalid-profile
diagnostics per host. Production VLC menu actions launch all three engines.
Each renderer presents 90 frames with 90 player-PCM windows from the generated
44.1-kHz stereo track. Both VLC interfaces report five installed visualizers
and display Visualizers between View and Help. The test uses VLC's dummy
output and generated media. The render probe also verifies nonflat image
output, scene changes, alpha, finite samples and resize on Vulkan/lavapipe and
Apple M1 Metal. The final targeted suites pass seven cases on Mint and six on
Mac. Both cover Terrain pixels, the native ABI and resource lifetime. Mac
also runs the Terrain controller reference comparison.

## Installable artifacts

| Combined suite | Download bytes | Staged payload bytes |
| --- | ---: | ---: |
| Windows x64 NSIS | 21,643,812 | 75,243,754 |
| Linux x64 install archive | 10,107,086 | 28,023,251 |
| macOS arm64 install archive | 5,728,231 | 18,661,854 |

The combined Windows installer is about 96.7% smaller than the preserved
654,552,819-byte Electron candidate. The two Unix archive formats include
`install.sh` and `uninstall.sh`. The Mac packages use ad-hoc signatures and
target macOS 11.0. The Windows installers are unsigned. Each combined
payload includes five visualizers with one shared Dawn runtime. All twelve
current scene and combined packages pass checksums and manifest/default INI
checks. Windows receives static dependency and architecture audits. The
Linux and Mac staging audits also verify relocatable runtime linkage and
catalog scanning. Installer execution and Defender testing remain outside
the active checklist.

The `linux-evidence/` and `macos-evidence/` subdirectories contain the pixel
and real VLC reports. The final PNG preview shows the actual render output.
The Windows AMD64 playback, menu and extended resource checks remain open,
along with Linux native HDR transport and release signoff.
