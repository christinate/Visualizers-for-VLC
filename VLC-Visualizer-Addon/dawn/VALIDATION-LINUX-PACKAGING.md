# Native Linux packaging validation

Development candidate `0.5.0-rc.1`, checked 2026-11-16 on owner machine `mint`.
Mac access, builds and validation remain deferred under M01-M09.

## Package behavior

Production Unix packaging no longer imports or calls the browser packager.
The Linux builder stages explicit native frontends, VisualizerNative, one
source-built shared Dawn library, API 4 renderer modules/WGSL and the original
Classic/projectM compatibility modules. ProjectM SONAME symlinks are preserved.
The combined archive contains the runtime once; independent archives include
their own runtime/plugin and support `--mode vlc` for plugin-only installation.
The system VLC installation is unchanged; its launcher enables the addon.

`sdk/renderer-release-native.py` packages an individual renderer checkout
without reading sibling renderer repositories. All four shared SDK snapshots
match 127 indexed source hashes. Updated native control tools and frontends
compile in all three independent renderer roots. Independent packaging passes
source/dependency/scan checks for Prism Flow, MilkDrop and Terrain.

Schema-2 user-directory installs verify file hashes, modes, link text and
component ownership. A per-suite installer lock serializes cooperating
installers. All selected sources/originals are prepared before publication;
new destinations are published without replacement. Replaced directory
entries are captured and verified before discarding the duplicate. Uninstall
checks every selected owned component before removing anything and leaves
independently installed plugins alone. Verified snapshots/backups remain for
repair. Original preferences/additional presets are retained.

The control helper uses OpenSSL 3 on Linux; its Mac source uses CommonCrypto
and remains uncompiled/unverified during the deferral. Older install-generation
removal remains available through the original archive; legacy browser
installation is not a production path in the new builder.

## Reproduced findings and fixes

- The first extracted archive failed source verification: a build file recorded
  as mode `775` was extracted as `755` by Python's data filter, while its SHA-256
  stayed identical. Packaging now canonicalizes executable/data permissions
  to `755`/`644` before recording them. The rebuilt archive matrix passes.
- A controlled concurrent write to a pre-existing README after preflight was
  overwritten by the first replacement implementation. The before-fix fixture
  reached this case and left the package README instead of the concurrent edit.
  Publication now captures/verifies the actual directory entry and restores
  changed data on rejection. The same fixture rejects the operation, retains
  the concurrent edit and rolls back earlier publications.
- Linux frontend/VLC-module build search paths contained a private development
  SDL directory. Their release RPATHs are now relative to the installed suite;
  dependency audits pass without LD_LIBRARY_PATH/LD_PRELOAD assistance.

## Observed checks

- **18 actual archive checks:** all eight combined plugin selections and exact
  original README restoration; whole-package modified-file refusal; three
  independent standalone installs; occupied-component refusal; changed projectM
  symlink refusal; three plugin-only VLC installs without replacing the shared
  runtime; addon removal retaining an independent plugin; invalid opt-outs.
- **14 native transaction cases:** SHA-256 known vector, modified/link refusal,
  exact restoration, eight controlled publication faults, a competing identical
  new destination and the concurrent-original edit regression. The same cases
  pass GCC ASan/UBSan with leak detection enabled. System OpenSSL and graphics
  drivers are outside this instrumentation scope.
- **12 staged plus 12 installed dispatch cases:** modern module rendering,
  native settings, original compatibility engines, fresh manifests over stale
  cache, changed/missing packages, generated player PCM isolation and owner-exit
  cleanup. Linux window checks use lavapipe on the current Xorg session. They
  do not establish hardware NVK presentation or real VLC playback.
- Static ELF/transitive `ldd` audits pass for all four package variants. The
  shared Dawn artifact matches its pinned source-build provenance. System
  library paths/hashes are recorded; dynamic driver loads need runtime checks.

Existing NVK numerical and 64-byte driver-allocation findings remain recorded
and unsuppressed. These packaging checks do not clear them or prove indefinite
stability, physical HDR, real Winamp UI or complete player integration.

## Exact aggregated V3 archive sizes

| Archive | Bytes |
| --- | ---: |
| Combined addon | 9,792,443 |
| Prism Flow | 8,984,146 |
| MilkDrop | 9,120,612 |
| Terrain | 8,574,075 |

These are draft download sizes. Installed bytes also include verification
snapshots/backups; complete installed/temporary allocation and final-source
provenance remain D55/D72-D74 work. Exact archive/audit SHA-256 values are in
the candidate's separate `SHA256SUMS.json`; no prior checksum file is replaced.

Owner-local evidence is under `build/native-linux-packaging-evidence`; current
archives also remain on mint under `native-candidate-0.5-linux-v3`. The source,
installed SDK, reports and prior releases were retained during disk cleanup.
Only verified generated object files and expanded test/staging copies were
removed; current archives were copied back locally with matching hashes.

Remaining release gates include deferred Mac construction/signing/framework
checks, actual VLC/Winamp playback, HDR capability/fallback, unresolved GPU
findings, extended soaks, source/license review and final Git release metadata.
