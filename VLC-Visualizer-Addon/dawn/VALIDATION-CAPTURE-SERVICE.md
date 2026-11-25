# Native recording helper for original engines

Measured 2026-11-16 on Windows AMD64 and Linux x86_64. Mac source is present
but rebuild, permission and CoreAudio checks are deferred in todo.md M02-M04
while macbook1 is offline. The latest Mac permission retry remains pending.

## Implementation and ownership

Original Geiss Classic and MilkDrop/projectM now launch `VisualizerNative
--capture-for <host-pid>` for standalone recording. Modern API 4 windows keep
direct native capture. VLC/Winamp windows retain player PCM ownership and do
not start this helper. Actual recording checks that its owner executable is
this suite's standalone Visualizers. Only an explicit generated-PCM/self-test
fixture permits another test owner; that fixture never opens a real device.

The helper owns one source lease per host PID, a bounded FIFO and one pending
IPC batch. Repeated selection requests launch a short-lived process that asks
the existing helper to show its native dialog, without creating another
writer/device. Requests match suite, owner, capture ID and nonce. Owner/menu
closure stays responsive during dialogs. Windows requests foreground access
only for the selected helper. Physical focus/keyboard checks remain D62.

`AudioCapture::drain` exports each queued float stereo frame once. It does
not turn overlapping 2,048-frame display histories into a PCM stream. It
supports partial reads, retains bounded SPSC ownership and discards stale
input. Dialog timers continue draining capture. Lock contention retains one
batch for retry. The existing legacy transport/analyzer is unchanged,
including its final signed 8-bit waveform ABI.

A replacement source opens before the old stream stops; cancel retains the
old source. Input changes clear old pending transport/history. On shutdown,
capture stops before transport/control cleanup, which precedes source-lease
release. POSIX IPC cleanup runs before releasing the lease, preventing late
global cleanup from unlinking a replacement helper's transport. Reports replace
bounded files; normal status updates are throttled to ten seconds. No audio
recordings are saved. The existing transient PCM transport is used and removed
on clean shutdown; reports contain counts/device metadata, not samples.

## Observed checks

`tools/check_capture_service.py` passes four cases per source type/platform:
Geiss Classic and MilkDrop/projectM each reselect three times, then each
cancel three reselections. Both generated PCM and actual Windows WASAPI /
Linux PulseAudio recording pass. One helper PID persists. Reselection records
four activations; cancellation retains one. Owner exit releases capture,
nonce records/source locks and POSIX PCM files.

Windows app-owned tests invoke the normal source-selection callback. Linux
posts SDL Ctrl-L events inside its process. No desktop input is injected.
This is separate from physical keyboard and real VLC/Winamp playback QA.

| Real recording case | Captured frames | Published frames | Largest drain |
| --- | ---: | ---: | ---: |
| Windows Geiss reselect | 329,728 | 325,632 | 3,584 |
| Windows Geiss cancel | 353,792 | 352,768 | 3,584 |
| Windows MilkDrop reselect | 360,960 | 357,888 | 4,096 |
| Windows MilkDrop cancel | 360,960 | 360,448 | 4,096 |
| Linux Geiss reselect | 350,720 | 343,552 | 3,072 |
| Linux Geiss cancel | 350,208 | 349,696 | 3,072 |
| Linux projectM reselect | 365,056 | 356,864 | 3,584 |
| Linux projectM cancel | 364,032 | 363,520 | 5,120 |

All eight report zero FIFO-overflow and transport-drop counters. Some Windows
cases record successful transport retries. Captured/published totals differ:
input replacement and shutdown discard queued tails. This is not an
uninterrupted/zero-discard or long-duration claim.

`tools/check_capture_service_control.py` passes separately on Windows/Linux:
wrong nonce ignored, ten concurrent requests retain one writer/control record,
menu close, forty invalid-input initialization/IPC/lock cleanup cases,
startup cancellation without an active stream, and rejection of actual
recording for an unrelated held test-owner executable.

A new direct-helper CLI regression reproduced bare `--line-in` failing instead
of selecting default. Production parsing now normalizes it before dispatch;
the corrected case passes on Windows and sanitizer Linux. The failed Windows
run remains under `build/native-capture-service-bare-before`.

The FIFO regression checks exact order of 12,000 queued frames across
333-frame reads, no hidden history-size limit, buffer bounds, stale discard,
recovery and history consistency. It adds 640,000 exact concurrent streaming
frames to the existing 1,280,000-frame concurrent history fixture. Both pass
Windows/Linux.

Linux ASan/UBSan instrument the helper and both C++ and miniaudio C code.
Unsuppressed leak detection passes the core stream regression, helper control /
forty initialization failures, and all four real PulseAudio cases. Final
diagnostic-file inspection finds zero sanitizer reports. Original renderers,
shared Dawn and system libraries are not fully instrumented by that run.
The separately recorded NVK allocation failure remains unresolved: these
helper checks do not create a Dawn GPU device or clear that finding.

After removing frontend browser launch paths, the twelve-case production
dispatch/player-ownership matrix passes again on Windows/Linux. Old browser
packages need a native update; installed Geiss Classic can still be selected
explicitly. Canonical hosts no longer launch Electron for rendering, settings
or capture. Browser reference/oracle sources and old packaging/N-API paths
still await D45/D50/D71 cleanup and final payload audit.

## Evidence and remaining gates

Windows evidence is under local `build/native-capture-service-*windows*` and
`build/native-dispatch-after-capture-windows`. Linux reports use corresponding
`~/vlc-visualizers-work/native-capture-service-*-linux` directories,
`native-capture-service-asan-check` and `native-dispatch-after-capture-linux`.
Compact copies stay in ignored local build directories. No application samples
or diagnostic reports are uploaded to third parties.

D45 remains open for Mac verification and complete production packaging/N-API
removal. Split the production runner's module-only source from workbench build
dependencies before packaging independent renderer projects. SDK snapshots,
installers, exact final Defender checks, real players, extended soaks,
source reconnection and final artifacts remain release gates. Prior candidates
and tags are preserved; no Defender detection is declared resolved.
