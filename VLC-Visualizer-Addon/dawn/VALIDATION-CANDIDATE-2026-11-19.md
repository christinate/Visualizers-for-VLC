# Native candidate validation, 2026-11-19

The owner removed installer testing from the current scope. This run builds
and audits candidate packages and exercises staged applications. The installer
results linked from earlier reports describe their original artifacts.

## Mac recording and hands-on window pass

SSH to macbook1 was restored. The host ran macOS 26.6.2 arm64 on an Apple M1.
With the owner's permission, the signed production app identity
`org.vlcvisualizers.native` captured 3,961,856 frames from the built-in
microphone with zero drops. Terrain presented 5,389 frames and exited at its
90-second limit. Recording checks retained counts and diagnostics.

Classic Geiss and projectM each passed actual CoreAudio reselection and
cancellation. Each ran three picker operations and closed its capture helper
on owner exit. The four runs published 342,528–348,160 frames each with zero
FIFO or transport drops. With Classic Geiss, startup selection by the exact
stable ID, exact device label, startup picker, and bare `--line-in` also
passed. Ten concurrent requests reused one helper and opened one picker.
The original helper continued publishing while the picker was open and
removed its control record, capture lock and PCM transport on owner exit.

The owner operated four staged windows. Terrain logged two fullscreen toggles,
one manual picker opening and Escape closure. Prism Flow logged two toggles
and right-click closure. Classic and projectM both rendered and exited 0
after the hands-on sequence. Earlier AppKit/VLC tests cover real handler
delivery and the generated media matrix. [Mac menu and media validation](VALIDATION-MAC-MENU.md)

The Terrain window exposed a reproducible capture problem. It dropped 203,264
frames during the hands-on session. The modal picker paused the render loop,
which was the only consumer of the recording FIFO. The shared runtime now
consumes PCM on each modal poll. The existing reselection fixture holds a
live dialog for 30 polls, exceeding FIFO capacity, and checks that the active
stream loses no frames. All four native SDK copies match 133 indexed sources.
The rebuild has the same bundle identifier and needs its own macOS microphone
grant. The retained failed attempts reported permission pending and zero
recording frames.

The rebuilt Mint app passed the 30-poll regression with four picker openings,
five recording activations and 336,896 frames with zero drops. The external
test harness expected a temporary monitor, but PulseAudio selected its existing
built-in output monitor. The app result covers the held picker on that monitor.
The isolated-device route assertion failed and the temporary module was removed.

## Numerical and native checks

- A clean Git archive exposed a Prism Flow shader provenance mismatch. The
  manifest held the CRLF hash for `webgpu_present.js`. The extraction checker
  now hashes the LF source, and all 17 generated WGSL modules verify with
  either checkout line ending. The shader bodies are unchanged.
- The rebuilt Mac Metal and Mint lavapipe/NVK probes each match all 24
  presentation cases and 36 scene frames against retained same-backend
  original-JavaScript fixtures, with zero differing components. The existing
  Windows D3D12 and Linux SwiftShader results and cross-backend differences
  retain their recorded scope. [Preserved reference results](../../Geiss64/native/prismflow/VALIDATION-NATIVE-REFERENCE.md)
- The rebuilt native suites pass 26 Mac tests and 25 Linux tests. Four
  targeted settings, PCM formats, FIFO and capture-lifetime tests also pass
  under ASan/UBSan on both hosts. Linux leak detection is enabled. The Apple
  runtime previously rejected the leak-detection option, so its four cases
  run with `detect_leaks=0`. System driver libraries are outside the sanitizer
  scope. [Earlier sanitizer and Mac scope](VALIDATION-MAC-RETURN.md)
- Mint's staged production dispatch passes generated PCM and ownership tests
  for the modern and original engines. Mint's current X11 window path uses
  lavapipe. NVK passes same-backend offscreen comparisons. The existing NVK
  64-byte allocation finding and unsupported hardware presentation in this
  X11 session remain open. [Linux live menu evidence](VALIDATION-LIVE-MENU.md)
- The Mac's final staged runtime passes all five window cases, including exact
  fullscreen bounds restoration and the expected device-loss exit. All four
  Terrain/Prism Flow HDR-off/on cases pass. The built-in display reports EDR
  headroom of about 1.67–2.0 in the opt-in runs, and the app selects float16
  Metal EDR. The earlier SDR-only assertion failed when the display reported
  active headroom. Physical brightness and color remain unmeasured.

## Fault review

The legacy buffer ownership and capacity fixes, PulseAudio removal hang,
Terrain diagonal, malformed catalog diagnostics, macOS alert exception,
recursive Windows launcher and MilkDrop compiler lifetime defects each have
bounded evidence in their corresponding reports. The current shader hash
and modal capture fixes address the two failures observed in this run.
The outstanding NVK allocation finding remains open. The historical system
crashes remain unexplained. The measured cases bound the stability results.
[Stability history](../STABILITY.md) · [Catalog checks](VALIDATION-CATALOG-SAFETY.md) · [Windows runtime evidence](VALIDATION-WINDOWS-RETURN.md)

A concurrent Linux window fixture timed out and was killed. Its suite
control record remained behind. The next same-suite fullscreen Close check
counted the stale record and failed its one-window assertion. The production
request code does not filter dead records before counting them. Abrupt-exit
recovery remains open under D62. All five rebuilt window cases pass with
a fresh suite path. The two dead records from the interrupted test copies
were removed. The serial Linux HDR fallback and all 46 catalog cases pass
on the rebuilt suite.

## Bounded concurrent soaks

The final Mac package's four engines run together for 900 seconds with
generated PCM. All exit 0 after 105,296 total frames. The earlier run ended
early while the interactive test was also running. Its report is retained.
The second run follows the manual session and capture attempts.

| Mac renderer | Frames | RSS after 120 s (KiB) | Sampled CPU (%) | Open file descriptors |
| --- | ---: | ---: | ---: | ---: |
| Terrain | 26,945 | 33,376–46,624 | 6.0–7.9 | 7 |
| Prism Flow | 26,925 | 37,392–65,568 | 8.6–12.5 | 7 |
| Classic Geiss | 25,708 | 34,032–47,824 | 1.2–22.9 | 8 |
| projectM | 25,718 | 43,152–52,528 | 10.8–20.0 | 8 |

CPU/RSS samples end at about 875 seconds. The owner-local `vmmap` snapshots
also record IOAccelerator and IOSurface mappings after warm-up. These are
process mapping snapshots. GPU budget and peak usage remain unmeasured.

All four Mint engines run to the 1,800-second duration, exit 0 and present
164,576 frames on the generated-audio staging build made before the picker
fix. Terrain averages 14.3 fps, below the harness's greater-than-15-fps
floor. The test result remains failed. Compilation and other renderer tests
ran concurrently. NVK offscreen comparisons use the NVIDIA GeForce GTX
1660 Ti. Actual Mint window rendering in this run uses Mesa lavapipe.

| Mint renderer | Frames | RSS after 120 s (KiB) | Sampled CPU (%) | Open file descriptors |
| --- | ---: | ---: | ---: | ---: |
| Terrain | 25,754 | 133,588–133,596 | 260–300 | 11 |
| Prism Flow | 44,548 | 220,076–229,192 | 100–111 | 11 |
| Classic Geiss | 50,786 | 126,504–127,148 | 32.7–32.9 | 8 |
| projectM | 43,488 | 145,732–150,584 | 98.2–107 | 8 |

CPU values can exceed 100% on multiple cores. File-descriptor samples cover
the later part of the run. The window path uses software Vulkan, with no
dedicated GPU memory measurement.

The rebuilt Mint suite passes a separate 120-second run with the same
throughput floor and 11,632 frames. Terrain presents 1,889 frames, Prism Flow
3,243, Classic Geiss 3,429 and projectM 3,071. The 30-minute qualification
retains its original result.

## Candidate builds and package sizes

| Project | Git source used for the build and matching source ZIP |
| --- | --- |
| VLC Visualizer Addon | `1cfbf6c89b35713255d3e8ee9675d2b53f15d9db` |
| Prism Flow / Geiss | `ee51aeb01d7f4b9e8c7286ccdc94bfae10bda335` |
| MilkDrop 2 | `42e5dd446d84f94c75e87b9a979521718275040d` |
| Rhythm Terrain | `60b820d0e59b0b1769d6f7049447ed5448bb30ad` |
| Compatibility Catalog | `6042db0ff583bb0d67899cbf96ad624849e6e51a` |

These are the exact source snapshots in the draft release index. Subsequent
validation and changelog commits record results. The package audits verify
payload hashes, access modes and symlink targets, dependency imports, notice
files and the pinned shared Dawn input. Combined packages contain one Dawn
runtime and plugin-only renderer components. The platform audits record PE
imports, Mach-O library metadata and ELF dependencies.

| Product | Windows AMD64 download / staged | macOS arm64 download / staged | Linux x86_64 download / staged |
| --- | ---: | ---: | ---: |
| Combined Addon | 21,541,201 / 74,271,488 | 5,523,727 / 17,957,881 | 9,832,473 / 27,324,092 |
| Prism Flow | 5,371,779 / 18,531,019 | 4,821,790 / 15,598,947 | 9,027,883 / 25,149,759 |
| MilkDrop | 8,244,135 / 27,423,856 | 4,901,840 / 15,988,957 | 9,161,445 / 25,559,905 |
| Terrain | 5,111,218 / 17,520,031 | 4,515,912 / 14,748,220 | 8,615,588 / 24,099,379 |

Sizes are bytes. The Unix rows show the suite-builder variants. The artifact
inventory records the independent renderer builds and full expanded archive
sizes. Staged bytes measure logical temporary package payloads. The combined
Windows download is 96.7% smaller than the preserved 654,552,819-byte Electron
candidate. All 62 recorded historical artifacts/checksum files and tags still
verify.

Windows combines the current MSVC-built host and modules with the retained
Qt 6.8.3 deployment. Both Windows bridge architectures compile. NSIS 3.12
builds the unsigned installers. The first combined packager run exceeded
NSIS's long-path handling for a Qt license filename. A shorter local staging
path includes the full notices tree and passes the 37-binary PE import audit.
Mac archives pass relative-library, arm64, minimum macOS 11.0 and strict
ad-hoc signature checks. Linux x86_64 archives pass ELF/transitive dependency
audits. Intel Mac, older macOS execution, real x64 Winamp, and Linux native
HDR transport remain open. HDR stays Experimental and off by default.

All five source helpers pass 45 isolated Git archive checks on the workstation.
All three renderer repositories also produce fresh packages using their own
SDK on Mac and Linux. The release drafts select those independently built
renderer archives and record their distinct checksums.

Owner-local artifacts and evidence are in
`VLC-Visualizer-Addon/build/candidate-20261119-final/`. The five draft releases
record intended tags, artifact paths and SHA-256 checksums. Publication,
release tags and remote URLs remain pending.
