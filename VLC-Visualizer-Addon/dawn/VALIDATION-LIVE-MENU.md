# Real VLC menu and decoded-audio checks

Measured locally on 2026-11-16 with isolated suites/profiles. Windows uses
official VLC 3.0.24 AMD64 with our inline Qt 6 interface; Mint uses distro VLC
3.0.20 x86_64 and its Qt 5 interface with our menu module. The media is a
locally generated stereo 44.1 kHz PCM16 WAV, decoded by actual VLC and passed
through its audio-filter bridge. It is separate from generated-IPC fixtures.
No microphone was used and these tools made no external analysis upload. Mac
remains deferred. Desktop Defender reporting was not verified before these
runs; the later privacy observation below limits any local-only claim.

## Linux actual menu actions

AT-SPI selected only the recorded private VLC process, then activated its
observed menu actions. The menu order is Media, Playback, Audio, Video,
Subtitle, Tools, View, Visualizers, Help. All three installed visualizers have
Open, Settings and Close entries, plus Close all and Rescan.

- Terrain Open creates a native renderer owned by that VLC process. Its
  individual Close menu action exits normally after 591 frames / 50,512 ms,
  with 591 real player-PCM windows and peak 0.366180419921875.
- Terrain Settings opens the native Qt 6 dialog for the private profile.
  Its fullscreen option, camera/color controls and Experimental HDR label
  were inspected; Cancel closed it without saving changes.
- Prism Flow and Terrain opened simultaneously. A Terrain module was then
  temporarily renamed after the menu had already been populated. Clicking
  the stale Open action displayed the actual `Visualizer unavailable` dialog;
  the two existing renderers and VLC remained running.
- The real Rescan action persisted a two-entry cache and the next menu opening
  omitted Terrain. Close all still closed both existing renderers even though
  Terrain was absent from disk/cache. Prism Flow reports 5,287 frames and
  Terrain 1,895, each with `closed_by=menu`, nonzero decoded PCM and no recording.
- The private Terrain file was restored with its exact original hash. Actual
  Rescan restored all three entries in the menu and persisted catalog.
- A separately monitored VLC restart populated the three-entry Qt menu from
  that unchanged cache, with no rescan. It opened Terrain, consumed 90 real PCM
  windows, closed by Escape and exited VLC normally with code 0. Cache SHA-256
  stayed identical across the run.

Linux rendering uses lavapipe in the existing Xorg session; these checks do
not establish NVK window support or physical HDR. The several-minute paired
run is bounded integration evidence, not an extended memory/handle soak.
MilkDrop's projectM backend also passes a separate real-menu launch: 956 frames,
decoded PCM peak 0.367188, fullscreen/restore and key round-trip checks, host
exit 0 and VLC exit 0. Its cache hash remains unchanged. These window/key
checks use the existing self-test inside the actual menu-launched host; they
do not establish every interactive preset command. The full real-media format
matrix remains open; older ABI results are not substituted for that check.

## Windows observation and remaining input work

Computer Use inspected the actual private player: Visualizers is inline
between View and Help, all three entries are present, and Terrain's submenu
contains Open/Settings/Close. After the owner brought VLC forward, screenshot-
based popup clicks successfully opened Terrain. Its control record identifies
native PID 45692 and VLC owner PID 79612 in the private suite.

Prism Flow subsequently opens and closes through the same actual Windows VLC
menu. Its D3D12/RTX 3070 Ti report records 10,091 frames / 169,964 ms,
9,417 player-PCM windows, peak 0.396697998046875, `closed_by=menu` and orderly
renderer/presenter/context/window release. Recording stays false. A copy is
`build/native-vlc-ui-windows-v2/prism-vlc-menu-close-result.json`, SHA-256
`42b6e57eb94335169499512265e4b289800dd190ad737bbaf1e0b8f3b01f1238`.
This is bounded actual playback/closure evidence, not a long-running leak test.

Original Windows MilkDrop 2 opens from that menu and renders 2,388 frames with
nonzero actual player audio. Its legacy host logs `audio_peak=50` in signed-byte
waveform units, not normalized float units. Another foreground window blocks
the subsequent menu-close attempt, so the documented, owner-scoped CLI closes
only this test's MilkDrop instance. Its control record disappears and the host
writes its final frame/audio log. Evidence is
`build/native-vlc-ui-windows-v2/milkdrop-vlc-menu-open-cli-close-result.json`.
This establishes actual VLC playback and CLI closure; MilkDrop GUI closure and
interactive keys remain open.

Terrain's same menu-launched instance later closes through VLC after
12,023,309 ms (3 h 20 min), 718,474 frames and 717,559 real PCM windows.
Peak is 0.396697998046875, all four camera modes were active (mask 15),
`closed_by=menu`, recording false and renderer/presenter/context/window release
reported. The generated steady tones produced zero beat detections; this run
does not qualify musical beat response. The retained final report is
`build/native-vlc-ui-windows-v2/terrain-vlc-menu-close-result.json`, SHA-256
`dce62ec622d495e4b7d1fef7f9df785167601ad227801a80eb4c4c4067977f9b`.

Two resource snapshots, 393.414446 seconds apart after over three hours of
warm-up, have identical process creation time and runtime hash. The values
below stayed unchanged between these two samples:

| Counter | Both samples |
| --- | ---: |
| Private bytes | 198,103,040 |
| Working set bytes | 164,864,000 |
| Handles / threads | 550 / 21 |
| GPU process dedicated bytes, summed adapters | 37,085,184 |
| GPU process shared bytes, summed adapters | 53,526,528 |

CPU time increased 43.6875 seconds over that interval, averaging 11.1% of one
CPU core. This is bounded post-warm-up evidence, not continuous peak tracking
or a universal no-leaks claim. The resource summary and both raw snapshots are
retained beside the final report. The private VLC/wrapper/renderers subsequently
exit normally, with no owned process or control record remaining.

Computer Use then returned `app approval timed out` for VisualizerNative.exe.
Direct visualizer-window input and the remaining Windows menu matrix stay
open pending that app-control approval. The initial activation failures and
popup-coordinate limitations are tool/setup observations, not product passes
or renderer failures. The long-running Terrain window is now closed; direct
input can resume on a fresh instance when app-control approval is available.

The launcher guard independently passes 12 read-only cases, scan regression,
normal headless launch through actual VLC with code 0, and its AMD64/system-DLL
import audit. See VALIDATION-LAUNCHER-GUARD.md. This does not complete the final
installer/Defender matrix or real Winamp UI validation.

## Test tooling and evidence

`tools/live_menu_linux.py` performs one observed accessibility action per
invocation and records the target PID/path, helper hash and resulting tree.
Native settings require the exact private runtime executable/profile. Popup
dismissal checks both active X11 ownership and actual keyboard focus before
sending Escape. It is a Linux development helper, excluded from production
payloads; it needs PyGObject/AT-SPI, wmctrl, xprop, X11 and Xtst.

An accessibility Press does not always dismiss Qt's popup. The test therefore
uses Escape before reopening the menu so `aboutToShow` refreshes its contents.
Qt 6 objects with no action interface or several named actions are handled
explicitly. These are harness adaptations, not production menu changes.

`tools/generate_vlc_audio_fixtures.py --output <fresh-directory>` prepares
synthetic 120-bpm tones in ten formats: mono 8-kHz U8 WAV, stereo 44.1-kHz
S16 WAV, 48-kHz S24 WAV, 96-kHz float WAV, 5.1 WAV, 24-bit FLAC, MP3, AAC,
Opus and Vorbis. All ten are generated, probed and decode-checked with the
installed FFmpeg under `build/real-vlc-audio-fixtures-windows-v1/`
(23,814,160 media bytes). The manifest records exact hashes, commands and
actual channel/rate/format metadata. These are fixture checks, not VLC passes;
VLC/renderer execution remains pending the desktop privacy choice.

The old menu self-test's five-second cutoff stopped lavapipe at 56 frames
before its existing 90-frame closure action. Qt's test-only cutoff is now
20 seconds; the same 90-frame contract passes. Both Windows/Qt 6 and Linux/Qt 5
module builds pass. Mac AppKit's old cutoff remains a return task.

One cold Linux restart lacked an AT-SPI registration; its popup observation
was inconclusive. Its live process was checked before normal window closure.
The successful persistence check above uses fresh application-level reports.
A later disk-full capture attempt is retained separately. Cleanup removed
only a cold, byte-identical generated browser copy after comparing every file
and link to a retained expanded package; source, SDKs, releases and reports
remain. Captured root/occluded images were not used to assert menu contents.
Mint subsequently ran out of free space again; only the generated projectM
WAV changed among large workspace files in the preceding hour. Further Linux
work is deferred until sustained space is available. This is not attributed
to the renderer or used as a successful soak result.

Owner-local Addon evidence is under `build/native-vlc-live-evidence-v1/`,
including a hash-verified copy of the Linux reports. Mint originals are under
`~/vlc-visualizers-work/native-vlc-{menu,actions,persistence}-linux-*`.
The later projectM result and host log are copied alongside that archive;
the result SHA-256 is
`e86cab064e9ce0a0640e95d89226fd33cd0e3b6fe459c6673389c83d7dbb69f2`.
Its Mint originals are in `~/vlc-visualizers-work/native-vlc-projectm-linux-v1/`.
Windows uses `build/native-vlc-ui-windows-v2/`; launcher checks are under
`build/launcher-guard-windows-v3/`, `launcher-scan-regression-v1/` and
`launcher-real-vlc-headless-v1/`. Failed setup runs are retained separately.
These development checks leave D36/D42-D44 and final release gates open.

## Desktop Defender privacy follow-up

A subsequent read on 2026-11-17 finds this desktop's Defender in Normal mode,
with antivirus, real-time protection and behavior monitoring enabled, MAPS 2
and sample consent 1. Those settings permit automatic cloud reporting/samples.
They were not checked before the desktop runs above, so whether Defender
automatically transmitted data during them is unverified. No tool explicitly
uploaded a sample or report. Further unreleased-binary tests here stopped
pending the owner's choice about temporary local-only analysis mode.

The separate vpntest1 installer results have verified local-only settings
before transfer/execution and restoration after sample cleanup; see
VALIDATION-PACKAGING-GUARD.md. Do not extend that assurance to this desktop's
earlier tests.
