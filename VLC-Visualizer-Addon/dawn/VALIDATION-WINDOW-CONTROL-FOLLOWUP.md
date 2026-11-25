# Native candidate follow-up, November 19, 2026

The owner-local candidate includes the shared window-control recovery change.
Installer and Defender testing are outside this run's scope. Package compilation and
dependency audits are recorded separately from renderer and player execution.
[Previous candidate validation](VALIDATION-CANDIDATE-2026-11-19.md)

## Window-control recovery

A forcibly killed process used to leave a control record and pending request
files behind. A new window in the same suite could make Close or input selection
count both registrations. The registry now prunes dead registrations and their
request files for the same suite. New protocol-1 records carry the process start
identity so a recycled PID also qualifies as stale. Older live records retain
their established PID-based behavior. Requests still match the suite, renderer,
owner and per-registration token.

The regression starts real child processes in a private control directory. It
covers hard termination, stale Close and input request files, a mismatched start
identity for a live PID, legacy records, and suite, renderer and owner
isolation. The previous header reproduces the dead-registration count failure
on Windows and macOS. The updated regression passes on Windows, macOS and
Linux. The rebuilt Mac and Linux stages each pass five window cases, including
fullscreen restoration, the three close paths and injected device loss. Four
production HDR-off/on presentation cases pass on each platform. The full
current native CTest suites pass 27 cases on macOS and 26 on Linux.

The rebuilt Mac and Linux renderers also pass real forced exit and same-suite
reopen. Selective Close leaves the other renderer alive. Paired Close all
closes both windows, and three rapid reopen/close cycles release their
registrations.

The rebuilt Mac stage passes 60 actual AppKit/VLC checks with the opt-in test
menu module in a disposable suite. These cover menu placement, all four
engines consuming decoded PCM, Settings, Close all, pause/seek/stop/resume,
held-action rejection, Rescan persistence, and player shutdown. The 40 format
checks use the same ten hash-verified generated fixtures. The production
archives contain the separately built production module.
[AppKit/VLC method](VALIDATION-MAC-MENU.md)

An unattended 150-second Mac recording retest reports permission pending,
one picker opening and zero recording activations. The owner-present retry
passes on the same signed bundle. It records four picker openings, five
capture activations and 56,832 frames from the final CoreAudio stream with zero
drops. Each held-picker check also verifies zero drops on the active stream.
The same rebuilt bundle passes four further real CoreAudio cases through
Classic Geiss and projectM. Both selection and cancellation pass, with
343,552–347,648 published frames per case and zero FIFO or transport drops.

## Player and display boundaries

The installed and staged Winamp binaries available for testing both have PE
machine type `0x014c` (x86). The publisher's desktop Help page links a single
Windows download. Real x64 Winamp playback remains unqualified.
[Winamp Desktop Player](https://support.winamp.com/winamp-desktop-player-for-windows)

The Windows synthetic ABI probe publishes 48 kHz PCM on x86 and x64 for Geiss,
MilkDrop and Terrain, six passes in total. The probe launches a no-op test
process and checks the actual shared PCM transport. All six DLL hashes match
the staged Windows payloads.

The remaining real-player and desktop-resource runs require a suitable
owner-local Windows test environment.

Linux native HDR remains open. The exact pinned Vulkan swapchain selects an
sRGB presentation color space and rejects Dawn's surface-color-management
request. Mint offers the existing X11/lavapipe path for window tests. The
production behavior is explicit SDR fallback, with HDR Experimental and off by
default. The capability and presentation limits are recorded in the HDR
report. [HDR validation](VALIDATION-HDR.md)

## Isolated 30-minute Mint soak

All four engines run together on X11/lavapipe for 1,800 seconds and exit 0.
The rerun keeps the prior generated-audio profiles, render dimensions and
greater-than-15-fps floor. Build and test jobs begin after the measurements.
The soak uses the preceding staged binaries. The recovery change runs at
registration and request time, outside the render loop.

| Engine | Frames | Average fps | RSS after 120 s (KiB) | Sampled CPU (%) | File descriptors |
| --- | ---: | ---: | ---: | ---: | ---: |
| Terrain | 28,704 | 15.95 | 133,928 | 293–302 | 11 |
| Prism Flow | 47,873 | 26.60 | 228,688–239,416 | 98.7–122 | 11 |
| Classic Geiss | 51,458 | 28.59 | 128,832–129,064 | 32.3–32.8 | 8 |
| projectM | 45,324 | 25.18 | 147,724–153,240 | 104–106 | 8 |

Fifty-six post-warmup samples per renderer accompany the 173,359 frames.
CPU percentages span cores. This software Vulkan path has no dedicated GPU
memory measurement. The earlier loaded run's 14.3-fps failure remains
recorded. [Earlier soak](VALIDATION-CANDIDATE-2026-11-19.md)

## Release artifact provenance

| Project | Source commit |
| --- | --- |
| VLC Visualizer Addon | `4d740b446ae23044699fbf2469ba62ceecc616d2` |
| Prism Flow / Geiss | `7954f133f13d234fe94efc7d34290c8df709cd14` |
| MilkDrop 2 | `527a7e3eceffce81cde4f8a9edbebcb757d2271d` |
| Rhythm Terrain | `c6ea53ba40ff176ad3f4500adf2ef0075f3793a1` |
| Compatibility Catalog | `6042db0ff583bb0d67899cbf96ad624849e6e51a` |

The rebuilt kits and matching source archives are stored under
`VLC-Visualizer-Addon/build/candidate-20261119-window-recovery/`. Source
inputs were verified against the exact Git blobs before each incremental
build. `release-index.json` records the artifacts and intended tags for all
five unpublished releases. Checksums match every listed file. Previous
release artifacts retain their checksums.

| Combined package | Download (bytes) | Staged payload (bytes) |
| --- | ---: | ---: |
| Windows x64 | 21,539,231 | 74,278,656 |
| macOS arm64 | 5,525,063 | 17,982,089 |
| Linux x86_64 | 9,840,438 | 27,344,396 |

The Windows download is 96.7% smaller than the preserved 654,552,819-byte
Electron candidate. `candidate-inventory.json` includes individual archive
sizes, expanded file sizes and component totals. The Mac archives use local
ad-hoc signatures and target macOS 11.0. The Windows drafts are unsigned.
