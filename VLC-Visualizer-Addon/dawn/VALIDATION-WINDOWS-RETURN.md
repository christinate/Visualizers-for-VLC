# Native Windows return checks

Checked 2026-11-17 on owner computer `vpntest1`: Windows 11, AMD Radeon R9 M360,
D3D12 driver 27.20.1034.6. These are local 0.5.0-rc.1 development candidates;
no tag, public release or repository remote was created.

## Confirmed fixes

A fresh combined installation copied its bundled plugins but did not create
`installed-visualizers.json`. VLC consequently showed zero entries. The NSIS
installer now scans after patching and before completing registration. If that
scan fails, it restores the original VLC files. The regression checks the
persisted catalog before any manual scan, so the test cannot hide this fault.

MilkDrop searched older DirectX helper versions while its package supplied
D3DX9_43.dll. Its existing shader flags also explicitly request the original
D3DX9_31 compiler. Loading only the newer helper reached shader initialization
but failed compilation; preloading the original compiler rendered successfully.
The package now includes the matching AMD64 D3DX9_31.dll and loads both helpers
from the renderer directory, retaining the original shader flags and behavior.

The loader now returns owned runtime leases. Configuration cleanup, normal
renderer destruction and partial initialization release them; the original
compiler stays loaded until the last runtime owner releases it. This replaces
the old fallback loads whose callers simply cleared their module handles.

## Observed checks

- All **23 native CTest cases pass** in the signed-in desktop session, in
  136.42 seconds. The 40-cycle GPU resource test needed about 106 seconds on
  this computer; its timeout changed from 60 to 180 seconds, with its cycles
  and assertions unchanged. Earlier failed SSH-session results are preserved.
- **Eight native window cases pass** across Prism Flow and Terrain: Escape,
  right-click, menu closure, fullscreen startup and window restoration. These
  exercise actual handlers with generated audio/events; they are not manual
  pointer/key observations.
- **Eight genuine-player cases pass:** Prism Flow, Terrain, original MilkDrop
  and Geiss Classic each receive nonzero decoded PCM from VLC 3.0.24 AMD64 and
  Winamp 5.9.2 x86. VLC's actual menu is between View and Help, with three
  installed entries. The fixture is generated stereo 44.1-kHz S16 WAV.
- Winamp's physical DirectSound output reports an error on this computer.
  These player tests use a private output plugin that consumes genuine decoder
  PCM at wall-clock speed without playing it. It generates no replacement PCM
  and is excluded from release payloads. Physical sound output is unqualified.
- **Six MilkDrop render/resize/unload cycles pass**, 120 frames each, with
  nonzero RGB and both D3DX9_43 and D3DX9_31 absent after every renderer unload.
  Six original Geiss cycles also pass. These bound specific lifetime behavior;
  they do not establish indefinite leak freedom or explain historical crashes.
- Earlier drafts pass **33 installer cases**. The first-install fix passes
  **18 further cases**, including all eight addon selections with an initial
  catalog, MilkDrop component combinations, both bridge architectures and
  modified-file refusal. A forced initial catalog-write failure restores all
  three original VLC files and preserves the pre-existing owner directory.
- The final compiler-inclusive candidates pass fresh default installations,
  the genuine-player cases above and normal removal of all four products.
  The complete selection matrix was not repeated for this final payload-only
  compiler addition. Both Winamp bridge architectures build; genuine x64
  Winamp UI remains a separate gate.

## Current artifacts

| Installer | Bytes | SHA-256 |
| --- | ---: | --- |
| Combined VLC Addon | 21,532,982 | `42b2e3d4ce7ca5fa43a124dc83759ebd3b7101a584c7639a7f405ee6babea718` |
| Prism Flow | 5,371,743 | `1c843d1a7f20eb2e9014e46b5b2e4c9198d4489d3422d5da78cb509034624dfb` |
| MilkDrop | 8,246,445 | `72778e7e9eb428f0b0394a93d0c8fa7d9b5b695b4734fd43c06e1e028aa9b197` |
| Terrain | 5,111,120 | `bf67d6042944c1bb14b9c684da9434cba76ce397e48e95e2ce90d2610daf2ef6` |

The combined download is about 96.7% smaller than the preserved 654,552,819-byte
Electron candidate. Its import/payload audit finds one shared Dawn runtime and
no Electron/Chromium/Node payload. This is download size, not peak disk usage.

Packaging inputs are Addon `ee6f23c`, MilkDrop `f384f72`, Geiss SDK snapshot
`e1c8f2d` and Terrain snapshot `ed14824`. The Geiss/Terrain C++ binaries retain
their earlier `55bc25d`/`f3b8b88` inputs; their later changes only synchronize
packaging Python/index files. The native Addon C++ build is from `728b052`,
reconfigured at `aa03107` without recompilation; later Addon changes affect
NSIS/packaging/docs. Qt's recorded source tree is unchanged. All four shared
SDK snapshots match 133 indexed hashes. Exact commands and artifact hashes are
retained in the private build records, without claiming final Git reconciliation.

## Retention and remaining work

With macbook1 offline, one current candidate set and private evidence are kept
on owner machine Mint in
`~/vlc-visualizers-work/windows-native-return-20261117-v1/release-and-evidence-v1.tar.gz`:
124,620,382 bytes, SHA-256
`68f1259c4927d72215cf6a2aaf0f71e975940501cad0270c4510d0091b8901c2`.
All 628 retained files were independently verified there, without running
executables or expanding another installation. Metadata-only evidence is also
in the workstation's `build/private-windows-vpntest1-v1/retained-evidence-v1/`.

All temporary desktop tasks, product registrations and private Windows test
copies were removed. Saved test-machine settings were restored; one current
public compiler/Qt/NSIS cache remains for future builds. Analysis stayed on
owner machines, with no external sample submission or further Defender study.

The remaining format/player/stability, license/source-artifact and final Git
release checks stay open in `todo.md`. Mac work is explicitly deferred while
offline. HDR remains Experimental, opt-in and off by default; no physical HDR
display qualification is claimed.
