# Mac VLC menu and media validation — 2026-11-17

All 60 bounded checks pass on macbook1: Apple M1 arm64, macOS 26.6.2
(25G83), installed VLC 3.0.24. Tests use generated media, dummy audio output
and real VLC decoding. They neither open the microphone nor save recordings.
Final-source release, manual input, real capture, other platforms and extended
stability gates remain in [todo.md](../todo.md).

## Observed behavior

| Check | Result |
| --- | --- |
| Menu position and wiring | One Visualizers menu directly after View and before Help; three entries with Open/Settings/Close |
| Open/Close | Terrain, Prism Flow, projectM and Geiss Classic render nonzero actual VLC PCM and close through their menu action |
| Settings | All three actual Settings actions save native preferences and retain unknown custom values |
| Close all | Paired Terrain/Prism Flow windows close and release their control records |
| Playback changes | Pause, seek, stop and resume retain the renderer; locked IPC-header observations verify transport stops/resumes |
| Stale actions | Open and Settings reject malformed JSON, invalid UTF-8 JSON, NUL/traversing paths, wrong API and real asset/package symlink escapes |
| Missing module | Stale action warns without launching; Rescan removes the entry; restore/rescan adds it again |
| Persistence | Three entries survive a real VLC restart without another scan |
| Media compatibility | Ten generated formats through all four engines: 40 passes |
| Owner exit | Quitting VLC closes its renderer and removes IPC/control records |

The media fixtures cover mono 8 kHz unsigned 8-bit WAV, stereo 44.1 kHz
16-bit WAV, 48 kHz 24-bit WAV, 96 kHz float WAV, 5.1 WAV, FLAC, MP3, AAC,
Opus and Vorbis. Hashes and source stream metadata accompany each result.
Every renderer presents at least 30 frames after a bounded initialization
interval and receives nonzero player PCM. This is decode/transport/rendering
qualification, not an assertion of accurate beat counts for every track.
For example, the 5.1 fixture reports zero detected beats despite nonzero PCM;
AAC reports one in this run. These observations are retained without inferring
a decoder or beat-detector fault from them.

## Actual actions and test scope

`tools/check_macos_menu_actions.py` drives a private opt-in module compiled
with `VIS_NATIVE_MENU_TEST_CONTROL=ON`. Regular actions dispatch through real
NSMenu items; held stale actions use NSApplication's registered target/action
delivery. Native Settings uses its explicit automated editor fixture. These
checks exercise the production handlers inside real VLC, rather than proving
manual pointer/keyboard input or macOS accessibility delivery.

The test protocol is PID/sequence scoped and only enabled with both test
environment variables. Default production builds have this feature OFF.
The release audit rejects a module containing its command-file marker. A
negative test with the actual instrumented module passes. The 12-binary
production archive audit also passes with no fixture marker. The instrumented
module is used only in a copied private suite, never a release artifact.

VLC's native `oldrc` interface owns the Unix socket. Its stdin stays open and
`--rc-fake-tty` enables that interface. State assertions use its actual numeric
play/pause/stop replies; the complete RC transcript is retained. Earlier
generic-Lua-RC, fake-tty, cold-start timing, Settings report-path and English
state-name harness failures remain preserved separately.

## Confirmed diagnostic fault

A focused Cocoa reproduction confirms that malformed UTF-8 in a JSON parse
exception converts to a nil NSString. Passing it to NSAlert throws
NSInvalidArgumentException. Menu diagnostics now pass through the bounded,
valid-UTF-8 diagnostic formatter before conversion. The malformed-file held
Open/Settings cases now warn without launching or terminating VLC.

Native/legacy error messages use the same diagnostic policy. Two separate
production tests reject malformed-UTF-8 settings files during Settings and
renderer startup with exit 1, valid bounded UTF-8 JSON failure reports and
byte-identical original settings. No recording begins. This is a reproduced
error-path fix, not an explanation for historical whole-system crashes.

## Source and evidence

The latest complete run uses these committed sources:

| Repository | Commit |
| --- | --- |
| Addon | `f0d24a6010d2ed6f81bac20d7e9137e2488a8d3f` |
| Prism Flow/Geiss | `544b754cfd81246d29e4b097acf7c955e0046587` |
| MilkDrop | `006da0da1247ad294e2f6161f86abb572d900b15` |
| Terrain | `fcc3fb440ab560cd12d0185fb08dc62fec43f333` |

On macbook1, relative to
`~/vlc-visualizers-work/mac-return-20261117-v1/`:

- `macos11-runtime-v1/menu-actions-v1/{result.json,*.vlc.log,suite/}`:
  the 60-case run and per-action reports; no remaining scoped processes.
- `mac-alert-utf8-probe-v1/result.json`: before-fix Cocoa reproduction.
- `macos11-runtime-v1/invalid-utf8-profile-v1/result.json`: two production
  error-report regressions.
- `menu-fixture-packaging-guard-v1/result.json`: release-module refusal.
- `mac-menu-actions-v1` through `v5`: retained harness failures;
  `mac-menu-actions-v6`: earlier 57-case pass using the older dependency build.

The verified owner-local Windows copy is under
`build/mac-return-evidence-20261117-v1/evidence-summary-v3/`. Its 26-file
diagnostic archive SHA-256 is
`a02df13ba177cda7330e34e8e2686b1c5116d2c15c29c1bf1c8218228d6094e4`.
Raw frames/media stay on the owner's machines. The production build and
dependency changes are detailed in
[Mac deployment compatibility](VALIDATION-MAC-COMPATIBILITY.md).
