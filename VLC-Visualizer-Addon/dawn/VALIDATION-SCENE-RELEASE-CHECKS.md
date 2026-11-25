# Scene release follow-up

The three-platform `0.5.0-rc.1` scene build is recorded in
`build/scene-variants-final/release-index.json`. Its scene-control and render
checks are in [scene validation](VALIDATION-SCENE-VARIANTS.md). The
beat-polling rebuild has a separate source and package index.

## Real VLC playback and window handling

Mint AMD64 and macOS arm64 each passed 60 format/engine combinations through
real VLC. On each host, ten generated formats were played through Terrain,
Prism Flow, Geiss Classic,
projectM, Beach and Objects. The PCM checks covered mono, stereo, 5.1 and
rates from 8 to 96 kHz, as well as WAV, FLAC, MP3, AAC, Opus and Vorbis.
The four Dawn engines each presented 90 frames per format. Classic and
projectM produced the expected player-audio traces. The renderer obtained
decoded PCM from VLC's bridge throughout the checks.

The same Mint suite passed 16 actual Qt menu and lifecycle groups. The tests
covered menu placement, Settings for Terrain, Beach and Objects, selective
Close, Close all for two renderer pairs, 14 stale Open/Settings rejections
from seven invalid packages, Rescan and cache persistence, pause/seek/stop and
resume, and VLC shutdown with renderer exit and PCM cleanup. Mint ran the
checks in a private nested X display and network namespace.

Owner-local machine-readable evidence is under `build/scene-variants-final/`.

- `linux-evidence/current-vlc-media-matrix.json`
- `linux-evidence/current-vlc-qt-menu.json`
- `linux-evidence/current-wayland-hdr.json`
- `macos-evidence/current-vlc-media-matrix.json`

The Mac's main volume failed test writes during the first run. The matching
package passed the full rerun using a disposable 64 MiB RAM volume for log
and profile writes. The playback fixtures and suite remained on the Mac.

## Real VLC resource measurement

The Mint package completed 1,800 seconds with one real VLC instance supplying
the same generated WAV to four renderers. VLC and every renderer exited
normally and released all owner records. The run presented 174,391 frames.
The evidence includes 240 samples of CPU, RSS, PSS, private memory and
descriptors. Post-warm-up observations began at 120 seconds.

| Renderer | Presented frames | Average fps | Private memory after warm-up (KiB) | Descriptors |
| --- | ---: | ---: | ---: | ---: |
| Terrain | 35,598 | 19.777 | 70,736 | 13 |
| Prism Flow | 43,698 | 24.277 | 135,684–150,104 | 13 |
| Geiss Classic | 51,137 | 28.409 | 48,096–48,784 | 9 |
| projectM | 43,958 | 24.421 | 82,960–83,272 | 9 |

Prism Flow's private memory fluctuated within the sampled range. The software
Vulkan session exposed no usable DRM GPU-memory counters. See
`linux-evidence/current-vlc-soak.json`. The Mac resource run lost SSH after
more than six minutes. Its terminal report was not recovered. The earlier
completed Mac resource sample remains in the prior candidate report.

## Beat-count baseline

The packaged Terrain payload registered 13 of 16 scheduled bass kicks in a
generated 120 BPM WAV played by real AMD64 VLC. Terrain presented 266 frames
at about 19 fps and received PCM in all 266. The count gate failed. The
120 BPM input file, expected onset times and measured count are retained in
`linux-evidence/current-vlc-120bpm-fixture.wav` and
`linux-evidence/current-vlc-beat-baseline.json`.

The player reader was sampling the latest 2,048 frames once per presented
frame. A model using the same WAV and detector found 13 kicks when 4,096-frame
commits replaced the snapshot every cycle, and 16 when it walked the available
history at a 512-frame hop. The commit size is a model input. See
`linux-evidence/beat-window-model.json`.

The first follow-up analyzed successive 512-sample hops from retained player
PCM. Its deterministic IPC test and Linux ASan/UBSan run passed. Real VLC
registered 16/16 kicks for Terrain at 120 BPM and Beach at 90 BPM, and zero
in silence. Objects registered 14–15/16 across repeated runs at 150 BPM.
Steady VLC playback advanced the bridge by 2,205 frames about every 50 ms.
Two Objects traces recorded graphics startup stalls of 2.90 and 3.15 seconds
between reads, enough for VLC to overwrite the early kicks in the audio ring.
See `linux-evidence/beat-history-real-vlc.json` and
`linux-evidence/objects-renderer-beat-trace-{1,2}.log`.

The corrected host polls VLC's PCM on a dedicated thread and analyzes the
retained windows while the graphics thread is busy. The development and
rebuilt Linux suite each counted all 16 scheduled kicks for Terrain at
120 BPM, Beach at 90 BPM and Objects at 150 BPM, and zero in silence. The
IPC regression and Linux ASan/UBSan rerun verified beat delivery after a
stalled graphics consumer and the reader's shutdown. See
`linux-evidence/beat-polling-real-vlc.json` and
`build/scene-beat-polling/linux-evidence/rebuilt-vlc-beats.json`.

## Linux Wayland presentation

The earlier exact Terrain payload rendered on nested Muffin Wayland with Experimental
HDR off and requested. It selected SDR in both cases. Vulkan exposed
`VK_EXT_swapchain_colorspace` and `R16G16B16A16_SFLOAT`, with sRGB nonlinear and
pass-through color spaces. The surface exposed no extended-linear, PQ or HLG
pair. This established the SDR baseline for the pinned Dawn Vulkan presentation
path. See [HDR validation](VALIDATION-HDR.md).

A separate headless Weston 16.0.0 run on Mint enumerates
`wp_color_manager_v1` and the exact Vulkan pairing
`R16G16B16A16_SFLOAT` with `EXTENDED_SRGB_LINEAR_EXT`. Weston and
`vulkaninfo` run in network-isolated test processes with software rendering.
The compositor runs from a temporary Alpine root. The Wayland capability
report is `build/wayland-hdr-experiment/weston-capabilities.json` with
SHA-256 `070e5f7ebdeeb4c21b6a713a0635041c82f48245465bb7cb9dda15fe68f517d9`.
On the same compositor, the unmodified pinned Dawn library presents an
SDR frame. It returns `SurfaceColorManagement unsupported` when the probe
requests extended-linear sRGB. The control report is
`build/wayland-hdr-experiment/weston-unmodified-dawn.json` with SHA-256
`e90cb156262544ba1b9807b1e19057c4b0956b333c44f0cecfe98634bf90fd4c`.
[Weston headless backend](https://wayland.pages.freedesktop.org/weston/toc/running-weston.html)

The Linux source build now applies a checksum-pinned patch to Dawn. The
Wayland presenter starts in SDR, offers HDR only for the float16
extended-linear sRGB pair and preserves SDR for unsupported backends and
renderers. The patched library exports a capability marker so existing
unmodified SDKs continue to fall back to SDR.

The staged application presenter passed the HDR and SDR cases with the
embedded marker and no test shim loaded. It presented three frames per case.
With HDR enabled, the color-managed Weston run used RGBA16Float and sent an
image description through the Wayland color-management protocol. With the
protocol disabled, the same HDR request used the BGRA8 SDR surface. The
positive run establishes the Linux transport through a headless compositor.
It keeps physical monitor HDR status unspecified.
[Vulkan presentation color spaces](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html)

All six Linux packages include the exact seven-file Dawn source patch and
the matching Vulkan runtime. The staged presenter loads the patched library
with SHA-256
`e400e88f0ea928ecffc6aceb284e8a91d0c48541928e342f1bd011ba8f48f80f`.
The source patch and library are tied to the Addon, Terrain64, Prism Flow
and MilkDrop build inputs through the shared SDK.

The first packaged VLC check received PCM and presented 27 Terrain SDR
frames before the 30-frame threshold. The longer run passed SDR and
reported a lost HDR surface. Dawn logged `VK_ERROR_SURFACE_LOST_KHR`
while querying surface formats. FIFO also reproduced the error. The
Weston kiosk-shell process exited with status 139 before teardown in both
the HDR and SDR playback groups after opening a second window.

The same production archive passed all 12 real Mint VLC 3.0.24 playback cases
under Weston's desktop shell on the headless software-rendered backend.
Terrain, Beach, Objects and Prism Flow each passed in HDR with Experimental HDR
enabled and in SDR with it off. Each also passed with HDR requested and the
compositor color-management protocol disabled. The four HDR cases presented
linear RGBA16Float frames. The eight SDR cases used 8-bit surfaces. Every
case received nonzero decoded VLC PCM, presented at least 97 frames and closed
through VLC's Visualizers menu. Both compositor instances still accepted a
Wayland registry query after playback. The release uses the production
presenter and its normal present-mode selection. Physical display output was
outside the accepted test scope.

The current GPU probe also passes 45 presentation-policy cases and 50
conversion cases on Mesa's software Vulkan adapter. The VLC reports identify
the exact combined Linux archive by SHA-256
`a066bdd790a99c7324329b72f2001d502295926cdd7fd44ef1f4dc19927263c5`.

| Report under `build/scene-wayland-hdr/linux-evidence/` | Cases | Frames | SHA-256 |
| --- | ---: | ---: | --- |
| `vlc-wayland-hdr.json` | 8 | 3,172 | `fbe59f2ad83b41710a141237a697b1cd8d943bfe571ea8b437e66aebcbc36734` |
| `vlc-wayland-fallback.json` | 4 | 1,421 | `637df4788e20cbc33dbb6b3390b431ac1d2757b6b3f516422a7fc1697f59f199` |

## Release artifacts

The final release index verifies 18 platform packages, five source archives and
the portable catalog against 142 exact Git build inputs and three platform
ledgers. The final Mac builds use the four exact source ZIPs in a fresh tree.
The six Apple Silicon archives retain minimum macOS 11.0 and locally verified
ad-hoc signatures. Their verifier checks each arm64 Mach-O against its embedded
audit and the staged Dawn and SDL libraries against their build provenance.
The combined package on each platform ships the committed default Terrain,
Beach and Objects INI files. Five Metal native tests pass for scene variations,
PCM, presentation, Terrain and the plugin interface. The Apple M1 presentation
probe also passes 45 capability cases and 50 conversion cases.

| Combined 0.5.0-rc.1 package | Download bytes | Uncompressed archive file bytes |
| --- | ---: | ---: |
| Windows x64 installer | 21,647,389 | 75,254,740 |
| Linux x64 install archive | 10,119,965 | 28,579,799 |
| macOS arm64 install archive | 5,740,145 | 19,208,664 |

The Windows download is 96.7% smaller than the preserved 654,552,819-byte
Electron candidate. Expanded size sums regular file contents in each payload
without filesystem overhead. The current links and complete checksums are in
`build/scene-wayland-hdr/INSTALLERS.md` and `release-index.json`.

The five local `v0.5.0-rc.1` tags resolve to the exact archived source
commits. The packages and repository release drafts remain unpublished.

The earlier scene index verified 18 platform packages and the portable catalog across
five draft releases. It also verifies five source archives, 74 exact Git
inputs, and matching build ledgers on Windows, macOS and Linux. The standalone
Prism Flow and MilkDrop Windows installers were rebuilt against the current
shared SDK. Their checks were static. The Windows AMD64 VLC runtime matrix,
window lifecycle and resource coverage remain open. Intended release tags
await signoff. The artifacts and drafts remain owner-local and unpublished.

The beat-polling source snapshot has six Windows x64 NSIS installers, six
Linux x64 archives and five source archives in `build/scene-beat-polling/`.
Both build ledgers match 100 exact Git inputs. The installer index links all
packages, the portable catalog, source archives and five unpublished drafts.
All 241 paths in the staged Linux combined payload match its archive's
component hashes or symlink targets.
The rebuilt Linux payload repeats all 60 VLC format/engine cases and all 16
Qt menu and lifecycle groups. Both reports identify the combined archive by
SHA-256. The final Qt check confirms VLC shutdown closes its renderer and
releases the PCM mapping.

- `build/scene-beat-polling/linux-evidence/rebuilt-vlc-media.json`
- `build/scene-beat-polling/linux-evidence/rebuilt-vlc-menu.json`

The combined Windows build is 21,651,100 bytes, 96.7% below the reference
Electron download. Its staged payload is 75,251,434 bytes. The Linux combined
archive is 10,114,157 bytes. The prior macOS arm64 installers remain available
and the updated macOS builds await SSH access to macbook1. See
`build/scene-beat-polling/release-index.json`.

## Windows x64 offline validation attempt

All four selected stock VLC 3.0.24 binaries and all 39 executable or DLL
entries in the rebuilt combined payload have AMD64 PE32+ headers. The exact
archive hashes and the per-file results are in
`build/scene-beat-polling/windows-evidence/windows-amd64-architecture.json`.

A new workspace-local VirtualBox 7.0.18 guest used the existing Windows 11
Enterprise Evaluation x64 image with all eight network adapters disabled.
Windows PE booted but stalled before the desktop with both the original
configuration and a four-vCPU retry using local disk caching. The guest was
powered off after the second stall. The VM, boot logs, checked VLC payload,
ten fixture files in their hashed archive, and Windows runner are retained in
`build/offline-win64-qa/`. This leaves the Windows VLC runtime gates open.

## Offline Windows desktop on Mint

The owner-local Mint host booted the same Windows 11 Enterprise Evaluation
x64 ISO under QEMU 8.2.2 with KVM, four virtual CPUs, 6 GiB of RAM and a TPM.
The hypervisor ran in a separate network namespace without a virtual network
adapter. The attachment check found only the encrypted guest OS disk, UEFI
variables and a synthetic test-result volume writable by the guest. The
Windows image and the VLC input media were read-only. Windows reached the
desktop and copied all 44 inputs. The first harness invocation timed out in
its PowerShell route check before launching VLC. The retry uses Windows'
native route command to check both address families. The first attempt's
console log, staging log and hypervisor result are in
`build/scene-beat-polling/windows-evidence/kvm/evidence/`.

The retry verified the combined payload hash and 384 staged files, and found
no default route. Stock VLC 3.0.24 AMD64 played the stereo 44.1 kHz WAV. The
captured Windows VLC menu shows Visualizers between View and Help. Beach and
Objects each recorded 90 frames, 90 decoded PCM windows, a peak of 0.499939
and Escape closure on Microsoft's software Direct3D 12 adapter. The harness
did not independently match their recorded owner PIDs to its selected VLC
window. It timed out before qualifying Terrain, Prism Flow and Classic Geiss,
and recorded exit code 1 on the MilkDrop case. The six-case pilot remains
failed. Its original report and desktop capture are retained in the
`kvm/` evidence directory. A third pass bound each case to the exact VLC
child process created by its launcher. Prism Flow, Classic Geiss, MilkDrop,
Beach and Objects played the same WAV and exited normally. Prism Flow, Beach
and Objects matched their renderer owner to that child PID and each presented
90 frames. Classic Geiss and MilkDrop presented 785 and 514 frames. The
first Terrain case timed out waiting for the Qt menu report. The separate
interactive harness also timed out waiting for its newly staged VLC window
before it could invoke any controls. The third pass is in
`kvm/evidence-ownership-retry/`. The subsequent matrix allows more time
for the first VLC launch and records each engine and fixture independently.

The complete Windows matrix played all ten fixtures through all six
renderers. All 60 cases rendered frames, received decoded PCM, reported the
five installed packages in the Qt menu and exited VLC normally. The four
Dawn renderers matched their owner PIDs to the exact VLC child and used
Microsoft's software Direct3D 12 adapter. Fifty-nine cases passed every test.
Terrain with 48 kHz stereo Opus presented 74 frames and received 25 PCM
windows. Its 20-second duration expired before the 90-frame Escape check.
The matrix recorded 16,342 frames. The audio output was VLC's dummy device.
The report SHA-256 is
`04444c6d6a5e836366f47b0e99a266d6330a228dec546e9236094f93cb0592bf`.
The consecutive Qt action run opened its staged VLC window, then timed out
waiting for the Visualizers popup. Both results are in
`kvm/evidence-matrix/`.

The isolated Terrain/Opus retest again decoded PCM. It reached 60 frames
and 20 PCM windows before the duration limit. The desktop follow-up opened
the menu and launched Objects when the harness attempted to select Terrain.
That run did not exercise the intended stale Terrain action. With scheduled
WAV fixtures, Objects counted 16 of 16 kicks at 150 BPM, and Terrain
counted zero in silence. Beach counted 13 of 16 at 90 BPM. The 120 BPM
Terrain trial timed out waiting for VLC to exit before the harness extracted
its renderer result.

The 1,800-second Windows resource run collected 120 samples from Terrain,
Prism Flow, Classic Geiss and MilkDrop. The final Dawn result assertion
failed after sampling. Classic Geiss stayed at 190 handles and five threads.
MilkDrop ranged from 257 to 258 handles and six to seven threads. Terrain
finished with four fewer handles and the same thread count. Prism Flow
finished with the same handle count, one more thread and about 23.1 MiB more
private memory than at the first post-warmup sample. Both GPU counter
requests were unavailable on the virtual adapter. See
`kvm/evidence-soak/` for these reports.

The recovered individual soak reports identify the cause of the final
failure. Windows VLC rejected the harness option `--rc-fake-tty`. All four
renderers finished the requested duration and each recorded zero PCM.
Their frame counts were 16,661, 18,956, 35,945 and 20,274, respectively.
Use this run for rendering-resource measurements. The VLC playback check
failed. The recovered Terrain 120 BPM report also shows zero PCM in its
cold-launch trial.

Direct virtual mouse and keyboard input on the same Windows release opens
and cancels Terrain Settings, opens Terrain, and uses F11 to return to its
original 320 × 240 bounds. Selective Close leaves Beach running. Close all
closes Beach and Objects. A held Open action rejects a missing Terrain
module, and a held Settings action rejects a malformed Terrain manifest.
Rescan removes the unavailable renderer and restores it after repair. The
five-entry catalog remains present after a fresh VLC launch. VLC's actual
transport controls pause, seek from about 4 to 9 seconds, stop and resume
while Terrain stays open. Terrain closes after the owning VLC exits. The
desktop observations are retained in `kvm/manual-ui/`.

The follow-up mouse-driven suite passed all 12 held Open and Settings
rejections. It covered malformed JSON, invalid UTF-8, an incompatible API,
an embedded NUL, parent traversal and a missing module. Rescan, restart and
valid Terrain Settings also passed. The direct session's renderer reports
verify player-exit closure for Terrain and menu closure for Beach and
Objects with nonzero decoded PCM. The catalog and raw renderer reports are
in `kvm/evidence-raw-followup/`.

The corrected Windows harness starts VLC through its ordinary Qt entry
point and waits for decoded PCM to advance before timing. A two-minute
pilot passed. The 30-minute run then presented 102,036 frames with nonzero
PCM in all four renderers. The 120 process samples include CPU time, working
set, private memory, handles and threads. VLC exited normally, released the
PCM mapping and left zero renderer ownership records.

| Renderer | Presented frames | Average fps | Private memory after 120 s (MiB) | Handles | Threads |
| --- | ---: | ---: | ---: | ---: | ---: |
| Terrain | 11,978 | 6.65 | 27.17–28.27 | 281–285 | 8–10 |
| Prism Flow | 52,719 | 29.29 | 108.43–129.16 | 310 | 13–14 |
| Classic Geiss | 24,354 | 13.53 | 13.07–14.18 | 191–195 | 5–7 |
| MilkDrop | 12,985 | 7.21 | 27.86–29.99 | 258–262 | 7–9 |

The software adapter reported zero dedicated GPU memory and shared memory
in two processes at the warmup snapshot. The near-end GPU counter request
was unavailable. These timings describe the isolated, concurrent
software-rendering run. The 30-minute report SHA-256 is
`4d9954904c0f74d6d46a056b20a896a41e368117292de45550e54d34c8adc6b8`.

The PCM-confirmed VLC rhythm rerun counts 16 of 16 scheduled kicks for
Terrain at 120 BPM, Beach at 90 BPM and Objects at 150 BPM. Terrain counts
zero in silence. Each renderer is tied to its exact VLC process. The
pilot, soak and rhythm reports are in `kvm/evidence-qt-playback/`.

The Windows guest also played the exact 48 kHz stereo Opus fixture
through VLC's MMDevice output and an output-only virtual Intel HDA device.
The captured signal matched the fixture for 53 whole seconds at 48 kHz,
including its separate 220 Hz left and 275 Hz right tones and their 2 Hz
amplitude modulation. The output device was virtual. The first automation
run timed out after issuing F11 and Escape from its Windows process.
The fixture hash, VLC device log and signal measurements are retained in
`kvm/evidence-lifecycle-opus/`.

The virtual-keyboard rerun uses the same Opus fixture and ordinary VLC
menu launch. Terrain presents 222 frames and receives 72 decoded PCM
windows in 47.212 seconds. A real Escape event closes the renderer, and
VLC exits normally with its PCM mapping released. The virtual HDA signal
matches the fixture for 34 whole seconds. Both follow-ups verify all 384
individual staged files. The two earlier 20-second self-test timeouts
remain in the historical matrix. See `kvm/evidence-hid-opus/`.

The workstation's Realtek speakers were muted. Windows selected the unmuted
Realtek headphones as its default multimedia endpoint. The standalone VLC
CLI trial timed out before creating an audio session. The stock LibVLC
3.0.24 AMD64 library then played the full eight-second generated fixture
through the default Realtek headphones and exited with status zero. The
MMDevice peak meter identified that playback process by PID and recorded
127 active samples, 7 tone/silence transitions and a 0.014984 peak. The
fixture was observed at 7,903 ms before its end event. The test retains
session measurements and leaves the device volume and mute settings intact.
It verifies the physical endpoint through Windows' audio stack.
`build/physical-vlc-audio/realtek-result.json` has SHA-256
`67566f0bd034c66669e5741667fd31ec6e67fb994a6e0ec6effcf15af81ea3be`.
[Windows audio session process ID](https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nf-audiopolicy-iaudiosessioncontrol2-getprocessid)

The geometry run on that Windows payload reproduced one shared fault across
Terrain, Beach and Objects. Each window started at 320 × 240, resized to
380 × 280 at (180, 160), and entered 1280 × 800 fullscreen. On returning to
windowed mode, all three kept (180, 160) and reverted to 320 × 240. Both
actual key transitions were recorded. Forced-exit recovery, three rapid
open/close cycles, PCM reconnection after stop, and player shutdown passed.
The report is in `kvm/evidence-geometry/`. The Windows fullscreen fix
preserves the current Win32 rectangle and sizes the renderer to the live
client area. The existing native window exercise now tests an OS-driven
resize before toggling fullscreen.

The rebuilt Windows x64 payload passes all 12 confirmation groups with real
VLC 3.0.24 AMD64 and the software D3D12 adapter. Terrain, Beach and Objects
resize from 320 × 240 to 380 × 280 at (180, 160), enter 1280 × 800 fullscreen,
and restore the exact resized bounds after two keyboard transitions. Each
scene receives actual VLC PCM. Selective Close leaves the other renderer
running, and Close all releases two VLC-owned windows. Reopen after forced
exit, three rapid menu cycles, Qt pause/seek/stop, PCM reconnection and
player shutdown pass. The native OS-driven resize exercise restores bounds
with 90 rendered frames and four surface configurations.

The first rebuilt run restored the correct bounds and recorded three
surface configurations per VLC window. An extra four-configuration gate
marked those rows as failures. The confirmation requires measured window
bounds and at least three live surface configurations. The controlled
native regression still requires four. Both runs are retained under
`build/scene-window-geometry/windows-evidence/` in `evidence-fixed-geometry/`
and `evidence-fixed-final/`. The confirmation verifies all 384 staged files
for Addon commit `5051b7de161c3faa03bea24a3541f186f37e7509`. Its
`windows-lifecycle.json` has SHA-256
`bf24309d83c3ee5d42eaa21692890bd1a4f1785fe4aa0f4cd3e001ecce9b141d`.

The Windows and Linux x64 archives with the fullscreen fix are indexed in
`build/scene-window-geometry/release-index.json`. The combined Windows NSIS
installer is 21,650,632 bytes and the matching Linux archive is 10,113,987
bytes. Both are built from the new source snapshot. The index verifies
12 installers, the portable catalog, five source archives and 110 Git inputs
across the Windows and Linux ledgers. The Linux release passes its scene,
Terrain, native plugin and five native window cases. The matching macOS
build awaits SSH access to macbook1.

## Windows fullscreen-fix resource soak

The isolated Windows x64 guest verified all 384 staged files for Addon
commit `5051b7de161c3faa03bea24a3541f186f37e7509` and ran VLC 3.0.24 AMD64
for 30 minutes with four concurrent visualizers. It presented 43,140 frames,
published 83,161,575 cumulative PCM frames and collected 124 process samples.
All four renderers received decoded PCM. VLC exited normally, released its
PCM mapping and left zero renderer ownership records.

| Renderer | Presented frames | CPU seconds after 120 s | Private memory after 120 s (MiB) | Handles | Threads |
| --- | ---: | ---: | ---: | ---: | ---: |
| Terrain | 27,495 | 4,485.36 | 27.88–28.20 | 307 | 8–11 |
| Prism Flow | 2,383 | 192.55 | 95.34–113.96 | 282–284 | 9–10 |
| Classic Geiss | 8,298 | 110.22 | 14.02–14.19 | 191–195 | 6–8 |
| MilkDrop | 4,964 | 133.33 | 27.45–29.98 | 258–262 | 6–9 |

The first attempt verified the files and opened the four renderers. Spawning
PowerShell to capture the initial process metrics timed out. The corrected
harness reads CPU time, memory, handles and threads directly through the
Windows process APIs. Both GPU-memory counter queries were unavailable in
the corrected run. These timings reflect concurrent software D3D12 rendering
in the guest. A low-priority Linux compile shared the host during part of
the run.

Both attempts are retained in `build/scene-window-geometry/windows-evidence/`
under `evidence-fixed-soak-first/` and `evidence-fixed-soak-retry/`. The latter
`windows-soak.json` has SHA-256
`2f667aa2fc43d0780e79206594003901ea72e9d177026da7de7413e27f7ce864`.

## Rebuilt Linux resource and presentation checks

The exact beat-polling suite completed 1,800 seconds with real VLC. The four
renderers presented 175,116 frames. VLC and the renderers exited normally and
released all owner records. The 240 samples include CPU, RSS, PSS, private
memory and descriptors. Dedicated GPU-memory counters were unavailable on
this software Vulkan session.

| Renderer | Presented frames | Average fps | Private memory after 120 s (KiB) | Descriptors |
| --- | ---: | ---: | ---: | ---: |
| Terrain | 36,079 | 20.044 | 68,596 | 13 |
| Prism Flow | 44,076 | 24.487 | 136,140–157,060 | 13 |
| Classic Geiss | 51,155 | 28.419 | 46,688–47,212 | 9 |
| projectM | 43,806 | 24.337 | 82,244–82,632 | 9 |

Two snapshots 300.758 seconds apart recorded the same thread counts for the
four processes, respectively 24, 25, 19 and 19. Descriptor counts also matched
at both snapshots. See `build/scene-beat-polling/linux-evidence/rebuilt-vlc-soak.json`
and `soak-threads-sample-{1,2}.json` in the same directory.

The same archive passed eight nested Wayland display cases. Terrain, Prism
Flow, Beach and Objects each presented frames in SDR with Experimental HDR
disabled and requested. The compositor exposed no HDR surface color space.
See `build/scene-beat-polling/linux-evidence/rebuilt-wayland-hdr.json`.

Seven additional checks use actual input in the private VLC desktop. Terrain
and Objects toggle fullscreen with F11. Beach uses Alt+Enter. Each restores
its resized 400 × 280 bounds and records four surface configurations. A
forcibly exited VLC-owned Terrain window reopens and its stale registration
is pruned. Three rapid menu-driven open/close cycles pass with decoded PCM
and clean ownership records. The first quick-toggle run recorded two surface
configurations while restoring the window bounds. The passing rerun gives
each size time to render. Both records remain under `linux-evidence/` in the
rebuilt release directory as `first-live-window-run.json` and
`rebuilt-vlc-lifecycle.json`.

## Final Apple Silicon VLC validation

On macOS 26.6.2, VLC 3.0.24 arm64 played a generated stereo WAV into the
extracted production payload. All 245 component paths matched the manifest
before playback. The archive SHA-256 is
`0ba43349019021c6a32d6529c01485f41103bd3744cfaa9633557ea0324554ae`.
Terrain, Prism Flow, Classic Geiss and projectM ran concurrently for 30
minutes. Each renderer processed decoded VLC PCM and exited normally. VLC
exited cleanly with zero remaining owner records.

| Renderer | Presented frames | Average fps | Post-warm-up RSS (KiB) | Median CPU (%) | Descriptors |
| --- | ---: | ---: | ---: | ---: | ---: |
| Terrain | 53,888 | 29.938 | 26,576–34,176 | 8.10 | 9 |
| Prism Flow | 53,906 | 29.948 | 34,160–73,312 | 13.30 | 9 |
| Classic Geiss | 51,438 | 28.577 | 27,840–33,616 | 23.55 | 9 |
| projectM | 51,467 | 28.593 | 35,648–47,920 | 17.60 | 9 |

The 240 process samples include CPU and resident and virtual memory. Successful
`vmmap` samples for all four renderers at 120.746 and 1,777.994 seconds
retain the IOAccelerator and IOSurface mapping rows. Resident graphics
mappings went from 9,776 to 9,776 KiB for Terrain, from 13.2 to 13.8 MiB for
Prism Flow, from 6,784 to 6,832 KiB for Classic Geiss, and from 28.0 to 29.5
MiB for projectM. These are process mapping snapshots.

Beach and Objects passed a separate 55-second run with 1,608 and 1,610
presented frames, respectively. Both used VLC PCM and released their owner
records. The probe results and report hashes are in
`build/scene-wayland-hdr/macos-evidence/`.

| Report | SHA-256 |
| --- | --- |
| `macos-vlc-soak.json` | `81a21a82619a6fb956d78448ce45276746619305d3c30698d31ea6ab4ed5dc68` |
| `macos-vlc-beach-objects.json` | `a43a6e5642e816f2dc5cae2be0ff8fa519082d27aaa65696841c963841e6b3fb` |
