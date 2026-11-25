# AMD64 VLC validation, November 19, 2026

This pass prioritizes VLC on x86-64. Earlier Winamp results remain in the
[window-control follow-up](VALIDATION-WINDOW-CONTROL-FOLLOWUP.md) and
[Windows return report](VALIDATION-WINDOWS-RETURN.md). The current scope uses
staged payloads and generated media.

## Mint playback and decoded audio

The Mint player is VLC 3.0.20, verified as ELF64 AMD64. It loads the Qt
Visualizers interface and PCM bridge from the current combined release archive
with SHA-256
`4e3edbf2fba5da278e2a42d357cbc81119b8bee942238c174dc7ebdc754d0360`.
The extracted package runs with a disposable profile, nested X11 display,
lavapipe, and a network namespace with only loopback. VLC uses its dummy
output. Each file is checked against its generated fixture SHA-256.

| Engine | Fixtures passed | Measured PCM peak range |
| --- | ---: | ---: |
| Rhythm Terrain | 10 | 0.283–0.508 |
| Prism Flow | 10 | 0.283–0.500 |
| Classic Geiss | 10 | 0.281–0.500 |
| projectM | 10 | 0.281–0.500 |

The 40 combinations cover WAV (8-bit mono, 16- and 24-bit stereo, 32-bit
float stereo, and 5.1), FLAC, MP3, AAC, Opus and Vorbis. Source sample rates
range from 8 to 96 kHz. The actual Qt Open action launches each engine.
Every player instance exits normally. Rhythm Terrain and Prism Flow present
90 frames per combination and report nonzero player PCM without initiating
capture. Classic Geiss and projectM report nonzero decoded-audio peak,
successful fullscreen restoration and a successful test key round trip.
The menu appears between View and Help in every combination.

## Mint Qt menu and playback lifecycle

Pointer and keyboard events were sent to the disposable VLC window on the
nested X display. Terrain Settings opened and canceled, individual Close
left Prism Flow active, and Close all closed the paired renderers. The
paired windows received PCM from the same VLC process.

Seven stale package conditions were tested through both the held Open and
Settings actions. The actual Qt checks rejected malformed JSON, invalid UTF-8,
an embedded NUL, parent traversal, an incompatible API, an assets symlink
escape and a missing module. Rescan removed and restored the disposable Terrain
entry. A new player instance kept the three-entry cache byte-for-byte.

The real VLC remote-control interface paused, resumed, sought, stopped and
restarted the local WAV. The suite verified the PCM write counters stopped
advancing during pause and stop and advanced again after playback resumed.
The existing Terrain renderer remained active across those transitions. VLC
quit with code 0 and closed its owned renderer with `player-exit`. The
PCM mapping and renderer control records were removed.

## Windows AMD64 architecture

The pinned VLC 3.0.24 archive and current Windows payload ZIP match their
recorded hashes. Four stock entries (VLC, libVLC, libVLC core and the Qt
interface) and all 37 executable or DLL entries in the candidate ZIP have
the PE32+ optional header and machine type `0x8664`. This includes the
modified VLC Qt interface, PCM bridge, launcher, native host and engine modules.
The current Windows playback matrix still requires an isolated Windows x64
session for owner-local execution. Earlier Windows results qualify VLC AMD64
with stereo 44.1-kHz WAV across the four engines.
[Windows return report](VALIDATION-WINDOWS-RETURN.md)

Owner-local reports are under `build/vlc-amd64-20261119/`.
`linux-amd64-media.json` holds the 40 cases and fixture hashes.
`linux-amd64-menu-lifecycle.json` holds the 13 action and lifecycle checks.
`windows-amd64-architecture.json` holds PE type and SHA-256 per entry.
