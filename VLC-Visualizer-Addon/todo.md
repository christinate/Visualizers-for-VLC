# Native Dawn migration and Git release checklist

Canonical tracked copy: `VLC-Visualizer-Addon/todo.md`. The workspace-root
`todo.md` is kept identical for convenient access. Created 2026-11-15.

Target: replace Electron/Chromium/Node with native C++ windows, native audio
capture and Dawn rendering on Windows AMD64, macOS arm64 and Linux x86_64.
Keep the existing separate-process VLC/Winamp architecture and retain the
original Classic Geiss and MilkDrop/projectM engines. Preserve Git history
and tags, and keep the current sources, builds and packages for each platform.

Check an item only after its stated result is observed. A compiled prototype
is not renderer parity or release readiness. Keep all application analysis,
recordings, diagnostic reports and Defender samples on the owner's machines.
Installer and Defender testing are outside the active checklist.

## UnderSea reef and CoralCanyon update

- [x] Prune superseded builds and distribution archives on Windows, Mint,
  and macOS. Keep the current packages, source snapshots, and Git history.
  Restore and verify the current Mint build on persistent storage.
- [x] Add varied individual and connected coral colonies, foreground kelp,
  detailed rock geometry, slow reversing currents, and a refractive surface.
- [x] Add CoralCanyon with two continuous glides, coral-covered terrain,
  current-driven kelp and fish, and a gentle beat response.
- [x] Verify both scenes on D3D12, hardware Vulkan, and Metal and check
  all nine standalone and combined packages. See `build/coral-canyon/INSTALLERS.md`.

## Beach dolphin and seabed update

- [x] Remove the rounded shrubs by the palms and ground coastal and reef rocks
  in the seabed. Use the same shoreline outcrops above and below the waterline.
- [x] Add a detailed bottlenose dolphin and a D-key preview that returns to the
  regular Beach camera after the jump. Preserve the timed beat-triggered jumps.
- [x] Verify the updated scene on D3D12, hardware Vulkan, and Metal. Check
  the standalone and combined packages for Windows x64, Linux x64, and macOS
  arm64. See `build/beach-dolphin/INSTALLERS.md`.

## Beach shader startup and shoreline variation

- [x] Add first-run shader progress and persistent Beach shader caching.
  The staged Windows renderer showed progress and wrote eight cache entries
  on the first launch. The next launch loaded those entries without showing
  progress.
- [x] Add the timed beach-chair visitor with a beat-driven head nod, varied
  shoreline and palm rocks, and six house styles with independent color cycles.
- [x] Verify current packages and record the render results. The Beach scene
  checks passed on Windows D3D12, Metal and Linux hardware Vulkan. All six
  Beach and combined add-on downloads passed checksum and asset verification.
  See `build/beach-startup/INSTALLERS.md`.

## Beach turtle-follow and landscape update

- [x] Add a camera that tracks a turtle from the surface to the reef and back.
- [x] Keep shoreline pebbles, crabs and bushes stationary. Add a more colorful
  sky, smoother sand dunes, detailed palms and turtles, and varied rock formations.
- [x] Verify both sides of the waterline and package the updated Beach and add-on.
  The full dive and return pass on Windows D3D12, macOS Metal and Linux
  hardware Vulkan. The Linux software adapter timed out. Verify package
  checksums and see rendered frames in `build/beach-turtle/INSTALLERS.md`.

## Beach water and shoreline update

- [x] Add faceted, layered coastal rocks, submerged reef stones, and more
  detail to Beach's terrain, plants, animals and buildings.
- [x] Add configurable, staggered turtle dives and refracted underwater
  rendering. Add caustics and water disturbances from rain and rock spray.
- [x] Verify the updated scene and package the Beach build. Scene checks pass
  on Windows D3D12, macOS Metal, and Linux Vulkan llvmpipe. The updated Beach
  and combined add-on packages are checksum-verified for all three platforms.
  See `build/beach-water/INSTALLERS.md`.

## Beach and UnderSea release update

- [x] Add Beach's offshore swells, waveform-riding turtles, beat-triggered fish
  and dolphins, and fourth-beat rock spray with occasional mist rainbows.
- [x] Add UnderSea's variable reef, waveform-driven kelp and caustics, fish
  schools, seahorses, and timed marine encounters. Include both camera views
  and single-event previews on Space.
- [x] Expose the scene defaults through the installed INIs and settings menus.
- [x] Verify render and settings behavior on Windows x64, Linux x64 and macOS arm64.
  Scene probes pass on D3D12, Vulkan llvmpipe and Metal. The staged Windows
  UnderSea runtime renders 90 generated-audio frames and exits cleanly.
- [x] Build and checksum current standalone and combined packages on each platform.
  See `build/undersea-release/INSTALLERS.md`.
- [x] Remove superseded local build and release artifacts. Preserve source,
  current platform packages and all Git history and tags.
  Removed 49 legacy archives and 24 superseded build or staging folders
  totaling 4.47 GiB. The preservation check verifies the original tag IDs.

## 1. Baseline and design

- [x] D01 Record clean repository heads, existing tags and artifact hashes.
- [x] D02 Create migration branches in all five independent repositories.
- [x] D03 Inventory browser dependencies: graphics, workers, image/text helpers,
  settings, input selection, CLI, window lifecycle and shared-audio IPC.
- [x] D04 Document the native host/renderer interface, ownership rules, thread
  model, backend selection and device-loss/resize behavior.
- [x] D05 Pin Dawn source revision and dependency hashes; distinguish prototype
  prebuilt SDKs from reproducible release builds.
- [x] D06 Define a release package layout with one native runtime per suite,
  plugin-only components inside the combined installer and independent
  standalone installers. Measure sizes rather than promising a target.

## 2. Dawn foundation

- [x] D10 Add a reproducible, hash-verified SDK/bootstrap tool and CMake targets.
- [x] D11 Compile a native Dawn adapter/device probe on Windows.
- [x] D12 Render to RGBA16F offscreen, read it back and assert known pixel values.
- [x] D13 Compile and render the existing Terrain WGSL through native Dawn.
- [x] D14 Create native surfaces/windows: Win32, Cocoa/Metal and Linux.
- [x] D15 Verify resize, bounded frames in flight, clean shutdown and device-loss
  handling; report a useful error for unsupported adapters.
- [x] D16 Build and run matching foundation probes on macbook1 and mint.
- [x] D17 Build Dawn from pinned source with only required release features;
  collect licenses, dependency provenance and reproducibility instructions.

## 3. Shared native host and audio

- [x] D20 Define a versioned C plugin ABI without C++ ownership crossing modules;
  share one compatible Dawn implementation and handle lifetimes explicitly.
- [x] D21 Retain float PCM and port the spectrum/RMS/beat analyzer with numeric
  comparison against the current JavaScript reference.
- [x] D22 Replace browser recording with native capture on all three platforms.
- [x] D23 Implement native device lists, stable device identifiers, permission
  errors and ambiguous/missing-device handling.
- [x] D24 Implement standalone picker, Ctrl-L/Command-L reselection and
  --line-in default / device ID / exact label.
- [x] D25 Test mono/stereo/multichannel, common sample rates, silence, clipping,
  nonfinite PCM, device removal, device changes and repeated reselection.
- [x] D26 Replace HTML settings with native controls and preserve existing JSON
  settings and old Windows INI migration.
- [x] D27 Preserve borderless default window, saved fullscreen preference,
  double-click fullscreen/restore, Escape, right-click and VLC menu closure.
- [x] D28 Keep VLC/Winamp audio ownership separate from standalone capture;
  close associated renderer windows on player exit.
- [x] D29 Bound histories/logs/queues; verify capture and GPU resources release
  on initialization failures and normal closure.

## 4. Visualizer parity

- [x] D30 Port Terrain orchestration and waveform history without browser APIs.
- [x] D31 Preserve beat height, waveform-shaped width/depth, color intervals and
  all four flying camera modes with smooth timed transitions.
- [x] D32 Port Prism Flow preset generation/history, CPU warp generation and
  background-worker scheduling.
- [x] D33 Port Prism Flow GPU resources and every render/compute pass, preserving
  WGSL where practical; replace browser image/text helpers.
- [x] D34 Verify four active motions/eight transition slots, RGBA16F feedback,
  palettes, waveform behavior, keyboard controls and supersampling.
- [x] D35 Compare deterministic scenes/audio against the preserved reference;
  do not silently replace missing features with approximate effects.
  Original JavaScript on pinned native Dawn now matches all 24 presentation
  cases and 36 scene frames exactly on D3D12/NVK/lavapipe/SwiftShader/Metal. The
  old cross-backend failures also reproduce between two original outputs.
  Rebuilt Metal, NVK and lavapipe comparisons pass with the same exact limits.
  The Git-archive shader hash is now independent of checkout line endings.
  See dawn/VALIDATION-CANDIDATE-2026-11-19.md.
- [x] D36 Retain original Classic Geiss and Windows MilkDrop 2 behavior and keys;
  retain projectM on macOS/Linux.
  All three original-engine paths build and render with their prior keys.
  Mac adds a hands-on Classic/projectM window pass. The AMD64 VLC matrix is
  tracked under D44.
- [x] D37 Keep HDR Experimental and off by default. Implement explicit native
  presentation capability checks and SDR fallback; no physical HDR claim
  without a display test.
  Windows scRGB, Mac EDR and Linux Wayland capability and fallback checks pass.
  Calibration is corrected and its 56-case Mac rerun passes.
  See dawn/VALIDATION-HDR.md and
  dawn/VALIDATION-MAC-RETURN.md.
  Follow-up confirms the exact Dawn Vulkan pin chooses sRGB and rejects surface
  color management. Four production presentation cases pass on each rebuilt
  Mac and Linux stage. Mint's tested X11 path remains SDR.
  The earlier Terrain payload also falls back to SDR on nested Wayland when
  HDR is requested. The available float16 surface has no extended-linear, PQ
  or HLG presentation color space. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The beat-polling Linux archive passes all eight Wayland SDR selection and
  fallback cases across Terrain, Prism Flow, Beach and Objects.
  An isolated Weston 16.0.0 headless compositor now advertises Wayland color
  management and the float16 extended-linear sRGB Vulkan pair. This supplies
  an HDR-capable test surface for the pinned Dawn experiment.
  The linked Dawn patch and updated presenter pass extended-linear sRGB and
  SDR selection checks with the embedded runtime capability marker. The
  positive case presents float16 frames and sends a Wayland image description.
  Turning compositor color management off yields SDR with HDR requested.
  The exact Linux x64 package passes 12 real VLC playback cases across Terrain,
  Beach, Objects and Prism Flow. Each renderer passes HDR and explicit SDR with
  Weston desktop-shell color management enabled, and SDR fallback with the
  protocol disabled. Physical HDR display output remains unmeasured under the
  accepted Experimental scope. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
- [x] D38 Remove recurring decorative rings from the shared Terrain shader;
  retain beat/waveform response and all camera modes.
- [x] D39 Reproduce and correct the Linux white diagonal in raw GPU output;
  add an animated all-channel pixel regression on hardware and software paths.

## 5. VLC, Winamp and catalog integration

- [x] D40 Dispatch native Dawn plugins from the existing launcher/host.
- [x] D41 Update and validate JSON manifests, compatibility catalog, schemas and
  third-party plugin/installer examples for the native interface.
- [x] D42 Verify Visualizers remains between View and Help, with Open, Settings,
  Close, Close all and persistent scan behavior.
  Linux real QAction checks now cover Terrain Open/Settings/Close, paired
  Prism Flow/Terrain Close all, missing-plugin warning, removal/restoration
  scans and persisted three-entry restart. Both native engines received actual
  VLC-decoded PCM. Windows placement/all three entries/Terrain Open observed;
  Prism Flow/Terrain Open/Close and original MilkDrop Open/CLI closure pass with PCM.
  Fresh Windows installs now populate the menu without a manual scan; all four
  engines pass actual VLC PCM checks. Manual input remains separate from the
  automated handler tests. Mac passes 60 actual AppKit/VLC menu/media cases
  again on the window-recovery build.
  Linux projectM also passes a real-menu
  launch with decoded PCM and bounded window/key checks. See dawn/VALIDATION-LIVE-MENU.md.
  The current AMD64 Mint package passes actual Qt Settings, selective Close,
  paired Close all, removal/restoration Rescan and cache persistence after
  restart. See dawn/VALIDATION-VLC-AMD64.md.
  Both the initial and beat-polling Mint suites pass 16 menu/lifecycle groups,
  including Beach and Objects Settings and paired closure. See
  dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The exact Windows x64 payload passes the actual mouse-driven menu checks
  in VLC 3.0.24. Terrain Settings and Open work, selective Close leaves
  Beach running, and Close all closes Beach and Objects. Rescan removes and
  restores Terrain. The five-entry catalog persists across a VLC restart.
- [x] D43 Retest missing/changed/incompatible plugins and malformed/traversing
  paths at both scan and menu-click time.
  Windows/Linux pass 46 scanner/frontend/runtime cases each, including 160
  Open/Settings rejections per platform and real junction/symlink escapes.
  Corrected confirmed NUL-path acceptance and malformed-Unicode diagnostics
  aborting entire scans. Mac held Open/Settings actions now reject malformed
  JSON/UTF-8, NUL/traversal/wrong API and actual asset/package symlink escapes;
  missing-module removal/restoration passes. A confirmed Cocoa nil-string alert
  exception is fixed. Remaining platform/final-source cases stay open. See
  dawn/VALIDATION-MAC-MENU.md and
  dawn/VALIDATION-CATALOG-SAFETY.md.
  The rebuilt Linux suite passes all 46 scanner/frontend/runtime cases again.
  The Mac window-recovery build also passes the held-action rejection,
  missing-module Rescan and catalog persistence cases again.
  Current AMD64 Mint VLC verifies 14 held Open/Settings rejections across
  seven invalid package conditions, including traversal and an asset escape.
  See dawn/VALIDATION-VLC-AMD64.md.
  The beat-polling Mint package passes those 14 held-action rejections again.
  See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  On the exact Windows payload, actual Qt mouse input passes 12 held Open
  and Settings rejections for malformed JSON, invalid UTF-8, incompatible
  API, embedded NUL, traversal and a missing module. Rescan and restart
  retain the restored five-entry catalog.
- [x] D44 Verify decoded PCM and playback through real AMD64 VLC across the
  supported engines and media formats.
  Ten generated WAV/FLAC/MP3/AAC/Opus/Vorbis fixtures now cover rates, depths,
  mono/stereo/5.1 and a beat envelope. FFmpeg preparation/decode checks pass;
  Mac VLC now decodes all ten through all four engines: 40 bounded PCM/render
  passes. Earlier Windows results cover all four engines with VLC 3.0.24
  AMD64 using stereo 44.1-kHz WAV. The first Windows scope is recorded in
  dawn/VALIDATION-WINDOWS-RETURN.md.
  The current pass prioritizes AMD64 VLC. Winamp follow-up is deferred.
  Earlier real x86 playback and six synthetic x86/x64 ABI results remain
  in the platform reports.
  The isolated KVM guest on Mint reaches the Windows desktop and plays the
  stereo WAV in VLC 3.0.24 AMD64. Five of six engines pass the PID-verified
  playback pilot. The first Terrain case times out waiting for the Qt menu.
  Both the failed initial pilot and PID-verified rerun are in
  dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The full Windows matrix verified decoded PCM and frames in all 60
  engine/format combinations. Fifty-nine passed every gate. Terrain with
  stereo Opus hit the 20-second cutoff at 74 frames before the Escape
  check.
  The Opus rerun again decodes PCM and expires at 60 frames. In the
  Windows rhythm trial, Objects counts 16 of 16 scheduled kicks and Terrain
  counts zero in silence. Beach counts 13 of 16, and the Terrain 120 BPM
  test times out waiting for VLC to exit.
  The corrected Qt VLC run verifies advancing PCM before rendering.
  Terrain at 120 BPM, Beach at 90 BPM and Objects at 150 BPM each count
  all 16 scheduled kicks. Terrain counts zero in silence.
  The ordinary Windows VLC menu rerun of Terrain with stereo Opus passes
  with 222 frames, 72 decoded PCM windows and actual Escape closure. VLC
  uses MMDevice and the virtual HDA output matches the fixture's stereo
  signal. The workstation's default Realtek headphones pass the separate
  physical endpoint check. Stock LibVLC 3.0.24 x64 reaches the end of the
  fixture, with all seven expected tone/silence transitions in its MMDevice
  session peak meter. The muted speaker endpoint keeps its setting.
  The earlier Mint VLC AMD64 run passes 40 real PCM cases. The Windows stock
  VLC entries and all 37 candidate payload binaries pass the PE32+ AMD64
  audit. See dawn/VALIDATION-VLC-AMD64.md.
  The current Mint and Mac packages each pass 60 format/engine combinations
  across six engines. A separate 120 BPM Terrain measurement catches 13 of 16
  scheduled kicks. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  With independent PCM polling, real Mint VLC catches 16/16 scheduled kicks
  each for Terrain at 120 BPM, Beach at 90 BPM, and Objects at 150 BPM. Silence
  registers no beats. The rebuilt Linux package repeats all four results.
  All 60 VLC format/engine cases pass again on that exact Linux archive.
  Windows covers the same matrix with the dedicated Terrain/Opus rerun.
  The beat-polling payload passes the AMD64 PE32+ check for all 39 binaries.
  The first offline guest setup stalled before the desktop. The recovered
  guest runs the staged playback and lifecycle checks above.
- [x] D45 Remove Electron dispatch, N-API audio bridge and browser capture from
  production paths after native replacements pass.
  Current native packages contain the native host/bridge and one Dawn runtime.
  Mac verifies real CoreAudio through both original-engine hosts.

## 6. Installers and package size

- [x] D50 Stage only required native runtime binaries, assets and notices.
- [x] D51 Remove Electron/Chromium/Node from all final payloads and audit
  transitive runtime dependencies.
- [x] D52 Build plugin-only bundled components; eliminate repeated runtime
  copies from the combined installer.
- [x] D55 Record compressed downloads, expanded payload and temporary package
  staging sizes. Compare the combined Windows download against the
  654,552,819-byte Electron candidate.
  The window-recovery combined Windows download is 21,539,231 bytes, about
  96.7% smaller. The staged Windows payload is 74,278,656 bytes. The current
  scene build is 21,643,812 bytes (75,243,754 staged), also 96.7% smaller.
  The beat-polling build is 21,651,100 bytes (75,251,434 staged), still 96.7%
  smaller. The final Windows installer is 21,647,389 bytes with 75,254,740
  uncompressed payload bytes. The final comparison for all three platforms
  is in dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
- [x] D56 Use a new candidate version; preserve all prior tags, hashes and
  security-held artifacts.

## 7. Stability and release validation

- [x] D60 Run targeted native/DSP/ABI tests and sanitizer checks for allocation,
  lifetime, thread, bounds and error-path defects.
  All 23 native Windows CTest cases pass in the desktop session. Six original
  MilkDrop render/resize/unload cycles release both helper DLLs each time;
  six original Geiss cycles also pass. The earlier candidate passes four
  targeted ASan/UBSan tests on each Unix host. The shared window-control
  recovery regression passes on Windows, macOS and Linux. The rebuilt native
  suites pass all 27 Mac cases and all 26 Linux cases.
  The beat polling regression passes under Linux ASan/UBSan and the same host
  sources compile for Windows x64.
  See dawn/VALIDATION-CANDIDATE-2026-11-19.md for sanitizer scope and
  dawn/VALIDATION-WINDOW-CONTROL-FOLLOWUP.md for the current native suites.
- [x] D61 Run extended soaks of Prism Flow, Terrain and the original engines;
  measure frames, CPU/GPU memory and handles after warm-up.
  One actual Windows VLC/Terrain run reaches 3 h 20 min / 718,474 frames and
  closes through VLC. Two samples 393 seconds apart after warm-up have identical
  private/working-set/GPU memory, handle and thread counters. This bounded run
  does not qualify all engines/backends or establish indefinite leak freedom.
  See dawn/VALIDATION-LIVE-MENU.md for the earlier Windows measurements.
  The rebuilt Mac suite completes 15 minutes across all four engines, with
  105,296 presented frames and stable per-process descriptor counts in the
  post-warmup samples. See dawn/VALIDATION-CANDIDATE-2026-11-19.md.
  The four-engine Mint run reaches 30 minutes and 164,576 frames. Terrain
  misses the 15-fps floor under the concurrent build/test load. Retain that
  failure and the remaining Windows coverage. The rebuilt Mint suite passes
  a 120-second follow-up with the same floor and 11,632 total frames.
  An isolated 30-minute Mint rerun now passes with 173,359 total frames.
  Terrain averages 15.95 fps at the original floor and settings. All four
  engines exit normally, and sampled descriptor counts hold steady.
  The Windows follow-up below records the VLC resource samples.
  The exact scene package also completes 30 minutes of real VLC playback on
  Mint. It presents 174,391 frames, keeps all four renderers above 15 fps and
  exits cleanly. The earlier Mac run lost SSH before its result could be
  collected. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The beat-polling Linux build completes its own 30-minute real VLC soak
  with 175,116 frames. Every renderer exceeds 15 fps and exits cleanly.
  Descriptor counts are steady, and two post-warm-up thread samples match.
  The exact Windows payload runs four renderers for the 30-minute sampling
  period and collects 120 process samples. The VLC harness passed an
  unsupported flag, so the renderers recorded zero decoded PCM.
  The corrected Windows VLC playback soak passes 30 minutes with 102,036
  rendered frames, 120 process samples, decoded PCM in all four engines
  and zero remaining ownership records. GPU counters are available at
  warmup only on the virtual adapter. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The fullscreen-fix Windows payload now passes a matching 30-minute VLC
  run with 43,140 rendered frames and 124 direct process samples. The run
  verifies all 384 staged files, reads decoded PCM in all four engines and
  leaves zero ownership records. The virtual adapter exposes no usable
  GPU-memory samples in this run. The first attempt stopped while launching
  the PowerShell sampler. The corrected run reads native process counters.
  The exact final Mac arm64 package passes the 30-minute VLC soak with 210,699
  frames and 240 CPU, memory and descriptor samples. All four renderers hold
  nine descriptors after warm-up and have GPU mapping snapshots at 121 and
  1,778 seconds. Beach and Objects pass a separate real VLC playback check.
  See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md for the evidence and platform limits.
- [x] D62 Exercise resize/fullscreen cycles, multiple visualizers, rapid
  open/close, pause/seek/stop, source reconnection and player shutdown.
  The former stale-record failure is reproduced on Windows and Mac. The
  shared control registry now prunes dead processes and detects recycled PIDs
  in new records. Windows, Mac and Linux pass the new regression. The rebuilt
  Mac and Linux stages pass real forced exit and same-suite reopen, selective
  Close, paired Close all and three rapid cycles. Each stage also passes all
  five native window cases. See dawn/VALIDATION-WINDOW-CONTROL-FOLLOWUP.md.
  The current Mint AMD64 VLC verifies pause, seek, stop, resumed PCM and
  player shutdown cleanup using an actual menu-launched Terrain window.
  The beat-polling package repeats these checks and releases the PCM mapping
  after VLC exits. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The same rebuilt package passes seven more actual VLC window checks.
  Terrain, Beach and Objects restore their resized bounds after fullscreen.
  A forcibly exited Terrain window reopens with stale-record cleanup, and
  three rapid open/close cycles release their registrations.
  Windows live input verifies F11 restoration, selective and paired closure,
  and the VLC controls for pause, seek, stop and resume. Terrain reopens
  after catalog repair and closes after its owning VLC process exits.
  Exact Windows geometry measurements expose a common failure after an
  external resize. Terrain, Beach and Objects restore the position and
  revert to 320 × 240 after resizing to 380 × 280. Forced-exit recovery, three
  rapid menu cycles and PCM reconnection pass. The shared Windows host
  now reads the live Win32 window and client bounds. All 12 groups pass on
  the rebuilt x64 payload in VLC 3.0.24. Each scene restores (180, 160) at
  380 × 280 after fullscreen. Selective Close, Close all, forced-exit recovery,
  three rapid cycles, pause/seek/stop, PCM reconnection and player shutdown
  also pass. The separate native resize regression presents 90 frames and
  records four surface configurations. See dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The Windows x64 payload rebuilt alongside the Wayland HDR release also
  passes all 12 VLC lifecycle groups after verifying 384 staged files.
- [x] D63 Test the available Windows/macOS/Linux machines and record actual
  adapters, backends and unsupported cases; keep Classic fallback available.
  Windows D3D12 runs were observed on vpntest1 and the workstation. Mac M1
  Metal and Mint NVK/lavapipe checks pass within their documented scope.
  Mint window tests use lavapipe because this X11 session lacks NVK surface
  support. See dawn/VALIDATION-CANDIDATE-2026-11-19.md.
  Removed three superseded expanded 0.4 package copies from each remote host,
  preserving all unique reports/settings and compressed releases. Reclaimed
  0.72 GB on Mac and 1.11 GB on Mint; measured free space is 6.04/1.83 GB.
  Recheck capacity before large builds. Sources, SDKs and unique captures remain.
  See dawn/VALIDATION-REMOTE-CLEANUP.md.
  While Mac is offline, one compressed current Windows candidate/evidence set
  is retained on Mint (124,620,382 bytes; no duplicate installation extracted).
  Mint has 509,300,736 bytes free at verification; larger builds need a new
  space preflight.
- [x] D65 Review definitive faults and fixes. Do not claim indefinite stability,
  absence of all leaks, or a cause for historical system crashes merely from
  a passing bounded test.

## 8. Git release preparation

- [x] D70 Update each repository's README, build instructions, changelog,
  dependency pins, licenses and release limitations.
  All five README/release guides and changelogs now describe 0.5.0-rc.1 and
  point older browser instructions to preserved tags. The suite release
  checklist also separates current native evidence from historical 0.4 passes.
  Static package and source inventories verify bundled notices and pinned
  dependency provenance. The four previous automatic hosted workflow recipes are
  preserved as inactive docs/legacy-ci/build.yml.txt files. All five repositories'
  .github/README.md files describe owner-local native builds and release
  execution; no cloud CI run or artifact upload was performed.
- [x] D71 Sync and verify the shared SDK snapshots in all renderer repositories.
- [x] D72 Build Windows installers plus macOS/Linux archives and source archives
  from the final committed source.
  Source archive helpers now use exclusive per-version output and a fixed Git
  commit; 45 isolation/ownership/provenance cases pass on Windows and Linux.
  Mac execution also passes all 45 cases. See dawn/VALIDATION-SOURCE-ARCHIVES.md.
  Project 4's actual catalog/source ZIPs now bind clean commit e1fcc6d and have
  identical bytes/checksums on Windows/Mac/Linux. See
  dawn/VALIDATION-CATALOG-RELEASE.md for that separate reproducibility run.
  Four Windows native installer drafts are built. Current original-compiler
  fixes pass genuine VLC/Winamp playback; exact source hashes, package sizes,
  artifact hashes and remaining gates are in dawn/VALIDATION-WINDOWS-RETURN.md.
  The current rebuilt packages and matching source archives are indexed in
  `build/candidate-20261119-window-recovery/release-index.json`. Earlier
  candidate archives are preserved under `build/candidate-20261119-final/`.
  The six-product scene release, portable catalog and five source archives
  are indexed in `build/scene-variants-final/release-index.json`.
  The independent beat detector has six rebuilt Windows and six rebuilt
  Linux installers, plus five source archives. See
  `build/scene-beat-polling/release-index.json`. The final macOS build
  includes that detector and the later window geometry and HDR changes.
  The fullscreen geometry fix has six rebuilt Windows x64 installers,
  six rebuilt Linux x64 installers and five matching source archives under
  `build/scene-window-geometry/`.
  The final candidate has six Windows x64 installers, six Linux x64 install
  archives, six macOS arm64 install archives and five exact source archives.
  The Mac build used the four native source archives in a fresh checkout-free
  tree. The three ledgers and all downloads are indexed in
  `build/scene-wayland-hdr/INSTALLERS.md`.
- [x] D73 Verify clean repository status, matching candidate tags/source commits,
  complete checksums and binary/source provenance.
  All five release drafts verify artifact checksums against the exact build
  commits. Earlier candidate drafts are recorded in
  dawn/VALIDATION-WINDOW-CONTROL-FOLLOWUP.md.
  The preceding scene release index verifies 19 artifacts, five source
  archives and 74 exact Git blobs against the three platform build ledgers.
  The independent beat detector's index verifies 12 rebuilt installers,
  the portable catalog, five source archives and 100 Git blobs across the
  Windows and Linux build ledgers.
  The fullscreen-fix index verifies the 12 rebuilt Windows and Linux
  installers, five source archives, portable catalog and 110 Git inputs
  against the two platform build ledgers.
  The final index verifies all 18 packages, five source archives and 142
  build inputs across three platforms. Each Linux archive contains the
  checksum-pinned Dawn patch and matching library. The six Mac archives
  match the audited arm64 binaries, pinned dependencies and INI defaults.
  The five local `v0.5.0-rc.1` tags point to their exact archived source commits.
- [x] D74 Complete release checklist, measured size comparison and final
  validation report. Keep HDR clearly Experimental.
  The final size comparison, exact Mac playback, Windows lifecycle and Linux
  Wayland results are recorded in dawn/VALIDATION-SCENE-RELEASE-CHECKS.md.
  The local candidate is signed off within the agreed test scope.
- [x] D75 Prepare independent Git releases without publishing; add repository
  remotes/release URLs when the owner supplies them.
  Five owner-local release drafts record verified tags, exact source commits,
  artifact paths and verified SHA-256 values. Publication awaits owner-supplied
  repository and release URLs.
  The latest drafts are under `build/scene-wayland-hdr/drafts/`.

## 9. Terrain, Beach and Objects

- [x] T01 Add configurable camera selection and separate beat-triggered height and
  wall-width effects to Rhythm Terrain. Default to 100% → 200% → 100% over
  0.5 seconds, with separate INI controls for each effect, duration and angle.
- [x] T02 Add Beach to the VLC catalog and standalone launcher. Render
  waveform-driven sets moving from the ocean at left to the shore at right,
  with randomized 0.3–0.7-second launches, run-up and retreat. Add the beat
  breaker, rock impact and reverse curl around the camera. Animate beach life,
  scenery and neon palettes with the audio and a configurable timed rotation.
- [x] T03 Add Beach's low/high note sensors with rain, depth and separate
  splash sizes. Ship a complete Beach INI with feature, camera, wave,
  environment, lighting, palette and sensor settings.
- [x] T04 Add Objects to the VLC catalog and standalone launcher. Use
  GPUCaster-inspired implicit 3D objects with beat-scale response, waveform
  rotation and low/high articulation. Ship a complete Objects INI with
  individual object switches and motion, camera and rendering settings.
- [x] T05 Verify the packaged INI defaults and overrides, exact pulse timing,
  disabled variations, and actual Dawn rendering for all three visualizers.
  The final stage passes the generated renderer cases on Metal and Vulkan
  and the current real VLC scene launches on Mac and Mint. See
  dawn/VALIDATION-SCENE-VARIANTS.md.
- [x] T06 Build the Windows x64 installers, Linux x64 install archives and
  macOS arm64 install archives. Include all three in the combined suite and
  provide standalone packages. Audit the payloads and record checksums.
  `build/scene-wayland-hdr/INSTALLERS.md` lists all 18 verified downloads.
  The installer index also references all five committed source archives.

## 10. Scene controls and Windows setup follow-up

- [x] Preserve the stock VLC icon on its patched launcher. Accept stock VLC
  3.0.24 AMD64, recover verified previous add-on files and keep independent
  renderer installs. Detect and close VLC from the selected install folder
  after confirmation.
- [x] Add a vendor-independent VSync setting, Windows software adapter fallback
  and Space to advance the focused scene's camera or shape. Rotate Objects'
  starting shape across launches.
- [x] Add nine Beach camera views, overhead rain and depth-tested variations
  of scenery. Keep the Terrain camera clear of the maximum waveform geometry
  and transition between disconnected views without crossing the rock.
- [x] Expose the complete Terrain, Beach and Objects INIs in VLC Settings and
  Winamp Configure. Build the updated Windows VLC installer and all three
  standalone installers with both Winamp bridge architectures.

## 11. Fullscreen VSync and coastal effects

- [x] Gate optional VSync on fullscreen for native and original engines.
  Verify transitions into and out of fullscreen with packaged Windows renders.
- [x] Lock the current camera or shape with L in Terrain, Beach and Objects.
  Resume cycling on unlock and show fading lock and unlock badges.
- [x] Add Beach's inland mountain range, palms, softer shore motion and
  beat-driven building motion. Move successive rock impacts toward the camera
  with airborne spray and foam. Include new controls in the Beach INI.
- [x] Build the updated Windows combined and standalone installers. Record
  the packaged tests under `build/coastal-update/`.

## Mac return checklist

SSH access to macbook1 was restored on 2026-11-19. The owner removed installer
testing from the current scope. Earlier installer results remain in the
validation reports. Native archive creation, static framework and dependency
audits, and renderer/player checks remain in scope. Analysis and captures stay
on owner machines.

The current Mac results, signatures and archive provenance are in
dawn/VALIDATION-WINDOW-CONTROL-FOLLOWUP.md. The macOS 11.0 target remains in
the build. Execution on older macOS and Intel remains unqualified.

The duplicate expanded 0.4 packages were already removed before this deferral.
Their compressed releases and all unique reports/settings were retained.

- [x] M01 Reconnect to macbook1 and check usable disk space. macOS 26.6.2
  arm64 is reachable, with about 3.76 GB free at preflight. The pinned source-
  built Dawn installation is retained. Fresh exact-commit source clones are
  verified, with 3.43 GB free after staging. Preserve sources, SDKs and reports.
- [x] M02 Sync the latest committed native host/helper/module sources, SDK
  snapshots and development manifests; rebuild arm64 against pinned shared
  Dawn, compile the new CommonCrypto installer helper, verify staged library
  paths and sign only our app/modules (D45/D71/M09). Arm64 rebuilds, CommonCrypto
  archive installation, relative dependencies and strict local signatures pass.
  Final-source packaging/independent build follow-ups remain M09.
- [x] M03 Verify microphone permission for the stable production native app
  identity. On 2026-11-19, with explicit owner permission, the signed
  `org.vlcvisualizers.native` app captured 3,961,856 CoreAudio frames from the
  default MacBook Pro Microphone and dropped zero. Terrain presented 5,389
  frames and exited on its duration limit. No audio was saved.
- [x] M04 Test the new original-engine native capture helper with real
  CoreAudio: startup picker, --line-in ID/label/default, Command-L/Ctrl-L
  reselection, cancellation, concurrent-request single-writer behavior,
  continued PCM during dialogs and owner-exit cleanup. Save no audio (D45).
  The approved stable bundle passes four real Classic/projectM reselect/cancel
  cases, exact stable ID/label/default startup, one-writer concurrent requests,
  PCM continuity during the open picker and owner-exit cleanup. The hands-on
  Terrain run exposed a separate modern-picker FIFO overflow and prompted a
  shared runtime fix. Mint's held-picker test passes with zero frame drops.
  The owner-present retest of the latest signed Mac build passes. Four
  pickers, five recording activations, 56,832 frames from the final input
  stream and zero dropped frames are observed. The earlier permission-pending
  run is retained separately. Four real Classic/projectM selection and
  cancellation cases also pass again on the rebuilt bundle with zero drops.
- [x] M05 Verify native presentation capability checks and SDR fallback;
  implement/validate Metal EDR. 29 policy/50 conversion cases, actual layer/color
  space/headroom transitions and four production HDR-off/on windows pass.
  Retain HDR as Experimental/off by default. No physical HDR display claim
  is required because the owner has no HDR display (D37).
- [x] M06 Test real Mac VLC menus/PCM: placement, Open/Settings/Close/Close all,
  persistent scans, changed/missing packages and player shutdown. Retest
  Geiss Classic and projectM behavior/keys separately (D36/D42-D44).
  Verify Escape/right-click closure, double-click fullscreen/restore and
  standalone audio selection/reselection in the staged production apps.
  Sixty actual AppKit/VLC cases pass: placement, all Open/Settings/Close groups,
  Close all, pause/seek/stop/resume, rescans/cache persistence, player exit and
  40 format/engine combinations. Build-gated in-process menu dispatch tests
  exercise real handlers; manual pointer/key delivery is a separate check.
  Catalog matrix: 45 passes; invalid UTF-8 filename unsupported because APFS
  returns EILSEQ before scanner input. Generated-only helper cases also pass.
  Held Open/Settings actions reject malformed JSON/UTF-8, NUL/traversal/wrong
  API and real assets/package symlink escapes; missing-module refusal/rescan
  passes. Production packaging rejects the actual instrumented test module.
  See dawn/VALIDATION-MAC-MENU.md. The owner exercised all four staged windows
  on 2026-11-19. The modern engines logged two fullscreen toggles each and
  the expected Escape/right-click closures. Current automated window tests
  check exact bounds. The remaining rebuilt-app recording check is M04.
  AppKit's self-test cutoff now matches Qt's 20-second/90-frame contract;
  the current Mac playback fixtures pass. Keep manual interactions separate.
- [x] M08 Run the final scene/regression, allocation/lifetime/sanitizer and
  extended-soak checks; record actual Metal adapter, resource measurements,
  unsupported cases and remaining definitive faults (D35/D60-D63/D65).
  Include the new 56-case calibration probe and explicitly corrected
  presentation-reference fixtures; preserve unmodified captures separately.
  Build the pinned development-only Dawn JavaScript reference binding for
  arm64 and compare original JavaScript against C++ on the same Metal adapter:
  all 24 presentation cases and 36 full-scene frames, with unchanged limits.
  Use Node 24+ with native Float16Array for scene captures; retain source,
  binding/runtime provenance, raw outputs and failed cross-adapter evidence.
  Keep this reference runner and Node out of production payloads.
  Metal presentation (24 cases/3,710,976 components) and scenes
  (36 frames/5,381,100 components) are exact against original JavaScript;
  the 56-case calibration probe also passes. The rebuilt Metal suite passes
  26 native tests and four targeted ASan/UBSan cases. Four concurrent engines
  complete 900 seconds and 105,296 frames. Reports include sampled CPU, RSS,
  descriptor counts and a snapshot of GPU-related process mappings.
- [x] M09 Build Mac archives/source from final commits. Verify dylib/framework
  relocation and signing with the native macOS archive builder. Audit runtime
  dependencies/notices, sizes, provenance, checksums and release limitations.
  Exercise the new immutable source helper with Python 3.11+ and Git.
  Prepare local Git release artifacts without publishing (D50-D56/D70-D75).
  Fresh pinned Dawn/SDL and production binaries now target macOS 11.0, verified
  across all 12 packaged binaries and signed app metadata. The former floating
  from_chars/macOS-26 compiler blocker is fixed in settings and palettes.
  Both SDL record-existing verification and fresh normal builder pass with
  identical raw dylib hashes. Tests run only on macOS 26.6.2/SDK 27.2; actual
  older-OS/Intel qualification and final-source release reconciliation remain.
  Preserved earlier drafts still require macOS 26+. See
  dawn/VALIDATION-MAC-COMPATIBILITY.md.
  Native combined/standalone arm64 archive builders now exist and pass bounded
  relocation/signature/ownership checks. Latest combined draft is 5,523,411
  bytes; earlier draft sizes/hashes remain in their validation reports.
  Source helpers pass 45 Mac cases; final commits/artifacts remain pending.
  Three independent 11.0-target SDK builds/packages now pass, with clean
  before/after source commits and 18 more ownership/install cases. Four actual
  independently installed generated renderer cases pass and uninstall cleanly.
  The 2026-11-19 build produces the combined archive and all three independent
  SDK archives again, plus matching source ZIPs and SHA-256 metadata. Static
  relocation and strict local signatures pass. The draft index records each
  archive and its build-source commit.

## Work log

- 2026-11-19 return: installer testing leaves the current scope at the owner's
  request. Mac SSH, the stable-bundle microphone check, original-engine real
  CoreAudio cases and the owner's four-engine window pass succeed. Fixed the
  re-opened input picker dropping queued PCM and the Prism Flow manifest's
  CRLF-sensitive source hash. Mac and Linux pass current native/sanitizer
  checks. Four Windows installers, native Mac/Linux archives, all five source
  snapshots and five release drafts are prepared locally. See
  dawn/VALIDATION-CANDIDATE-2026-11-19.md for versions, measurements and open gates.

- Preserved all five v0.4.0-rc.1 baselines and created feature/dawn-native
  branches. Recorded the browser inventory, native ownership/thread model and
  package layout in dawn/README.md.
- Added the pinned/hash-verified SDK bootstrap, pinned-source build recipe,
  dependency provenance/notices collection and separate native CMake targets.
- Windows D3D12, macOS Metal and Linux Vulkan each passed both offscreen
  foundation checks: exact RGBA16F readback and existing Terrain WGSL with
  four deterministic camera views and a waveform/silence comparison.
- The initial Windows milestone used our locally compiled Dawn library;
  Mac/Linux initially used development SDKs. See dawn/VALIDATION.md for those
  measurements and test limits; subsequent source-build results follow below.
- Added native Win32, Metal and Linux surfaces. Five lifecycle cases pass on
  Windows and Mac hardware and Linux lavapipe: resize/fullscreen restoration,
  fullscreen Escape/right-click/menu closure, and controlled device loss.
  NVK hardware presentation is rejected cleanly on Mint's non-DRI3 X session;
  its hardware offscreen probe still passes. These are app-owned event tests,
  not a replacement for later physical input/player checks.
- Ported the float analyzer: 612 exact-PCM reference windows pass on all three
  platforms, including 40 detected beats. Linux ASan/UBSan/leak checks pass.
- Ported Terrain waveform history, color intervals and timed camera blending;
  24 reference fixtures match exact half-float history and camera/palette
  uniforms. Removed the recurring ring decorations at the owner's request;
  ring-free shader checks pass on all three hardware offscreen backends.
- Source-built Dawn now passes on Windows, Mac and Linux. Windows native binaries
  use static CRT and have no VC runtime redistributable DLL imports. The
  exact SDK shader-compiler binary is matched to its SDK license/notices.
  D17 source provenance/notices are collected; final payload staging and the
  runtime redistribution audit remain release gates.
- API 4 modules now share the pinned Dawn runtime with explicit C ownership
  and tracked asynchronous callbacks. Layout, negative-loader, renderer parity
  and failed-initialization callback cleanup checks pass on all three systems.
- Native float capture/FIFO tests pass on all systems; real Windows and Linux
  recording checks pass. After owner-approved microphone access, Mac CoreAudio
  also passes real mono-48-kHz to stereo-44.1-kHz recording without FIFO drops.
- Native Win32/Cocoa/Qt input dialogs pass selection, duplicate/Unicode-name,
  cancel and owner-close tests. Startup plus Ctrl-L/Command-L capture switching
  passes windowed/fullscreen checks on all three systems. After the owner's
  separate approval for the Mac visualizer app, both Mac cases pass four
  native input activations, zero FIFO drops and Escape shutdown. See
  dawn/VALIDATION-INPUT.md.
- Added 200 streaming PCM format/channel/rate conversion cases, passing on all
  systems. Linux ASan/UBSan/leak checks also pass the capture FIFO, selector
  and converter. A real Linux virtual-input removal exposed a PulseAudio
  shutdown hang; the recorded wakeup patch fixes it. Removal, missing-ID,
  reconnection, real-window device changes and Escape shutdown pass, including
  backend ASan/UBSan/leak checks. Temporary modules/defaults are cleaned/restored.
  This is available-device coverage; longer lifecycle/soak checks remain open.
- Reproduced the Linux white diagonal in software-rendered RGBA16F output,
  independent of remoting. Snapshotting output controls before ray tracing
  removes it; 128-frame SDR/linear audits pass on all four tested paths.
  Hardware output remains bit-exact. See dawn/VALIDATION-PIXELS.md.
- Native settings/storage and next-launch window preferences pass on Windows,
  Mac and Linux, including old INI/UTF-16 migration, unknown JSON preservation,
  Unicode paths, concurrent atomic saves, native controls, saved fullscreen and
  double-click restoration. Linux settings ASan/UBSan/leak checks pass. Corrected
  a reproduced Windows rename failure during concurrent reads without changing
  Defender settings. Five closure/device-loss cases pass on all three systems;
  actual VLC menu clicks remain D42. See dawn/VALIDATION-SETTINGS.md. The native
  workbench still uses SDR presentation pending D37. Current CTest sets pass
  Windows 9/9, Mac 9/9, Linux 8/8; final installers are not rebuilt yet.
- Native player PCM and OS process watches pass on Windows/Mac/Linux, with
  input-selection isolation, two windows following one owner, missing-bridge
  silence, fullscreen owner-exit closure, menu closure that leaves the owner
  running and IPC/control cleanup. Core player checks also pass Linux
  ASan/UBSan/leak detection. See dawn/VALIDATION-PLAYER.md. These generated-PCM
  checks are separate from real VLC/Winamp playback (D44). Native CTest sets
  now pass Windows 10/10, Mac 10/10, Linux 9/9.
- Native foundation lifecycle checks pass 40 renderer creation/resize/destruction
  cycles on all four backends, with unchanged warmed live GPU bytes/handle
  counts and a positive retained-texture control. Callback-quota cleanup,
  partial initialization, 40 capture-context cycles on all systems and 40
  recording cycles on Windows/Linux pass; Linux recording also passes
  ASan/UBSan/leak detection. Three failed-window initialization cases release
  control records on all systems. Bounded UTF-8 diagnostics and deterministic
  accelerated test timing are in place. See dawn/VALIDATION-LIFETIME.md.
  CTest now passes 12/12 on each platform. Repeat resource checks with Prism
  Flow after its port; extended soaks remain.
- Began D32 with the complete native C++ wormhole geometry/flow map generator.
  Eight full-map comparisons cover both displacement components at 295,041
  pixels on Windows/Mac/Linux, with exact surface classifications and maximum
  error 2.91e-11. Linux ASan/UBSan/leak checks pass. Numeric bounds and
  cancellation are tested; notices are retained. See
  Geiss64/native/prismflow/VALIDATION.md. D32 stays open for the remaining
  motions, presets, palettes/history and bounded worker scheduling.
- D32 CPU motion generation now covers all 40 selected modes, additional
  reference branches, dynamic descriptors, all 43 preferences, compatibility
  matrices, seeded random banks and weighted selection. Across 94 full-map
  cases, 45,088,896 float components pass on Windows/Mac/Linux and Linux
  ASan/UBSan/leak detection; maximum error is 1.86e-9. All 1,196 weighted
  selection cases pass. See Geiss64/native/prismflow/VALIDATION.md. D32 remains
  open for preset/palette/history logic and bounded background scheduling.
- D32 palette/background helpers now pass all 355 saved palettes, 79 seeded
  selections/generated palettes, full candidate rankings and nearly one million
  ordinary tone-map components on Windows/Mac/Linux plus Linux ASan/UBSan/leak
  detection. Explicit regression checks correct confirmed reference zero-
  luminance NaNs and extreme custom-palette white-fade extrapolation; bundled
  palette output matches. See Geiss64/native/prismflow/VALIDATION.md. D32 stays
  open for preset/history logic and bounded worker scheduling. Windows CPU
  CTest passes 4/4; these helpers are not yet dispatched or packaged.
- D32 now passes native preset generation/re-randomization, history and bounded
  worker/controller checks on Windows/Mac/Linux plus Linux ASan/UBSan/leak
  detection. The 78 preset cases compare 150 states and 1,094,688 sampled motion
  components; full native map release/rebuild round trips pass. All 1,027
  timing/navigation cases match the reference. History retains 128 entries,
  releases static maps outside radius three and rejects stale worker results;
  cancellation, edited settings, dynamic descriptors, 40 normal/8 interrupted
  worker lifetimes and lookahead pass. Windows CPU CTest passes 6/6. See
  Geiss64/native/prismflow/VALIDATION.md. GPU/control integration, image parity,
  production dispatch and release packaging remain open.
- D33 began with 17 tracked, unchanged WGSL modules and native pipeline checks.
  All 16 render/7 compute pipeline variants pass on Windows D3D12, Mac Metal,
  Linux NVK and Linux lavapipe; Windows targeted CTest passes 2/2 including
  exact reference extraction. The shader/manifest assets total 87,010 bytes.
  See Geiss64/native/prismflow/VALIDATION-GPU.md. D33 remains open for resource/
  cache ownership, actual frame execution, image/text and render-controller
  integration; pipeline creation is not full renderer or HDR parity.
- D33 now executes native procedural-noise compute and all four dynamic-motion
  render passes. Exact hash/storage, 288 uniform and 512 spectrum checks pass
  on D3D12/Metal/NVK/lavapipe, with 8,388,608 finite motion components per path.
  Linux ASan/UBSan/leak detection also passes. Confirmed and fixed the shared
  half converter's lost subnormals and incorrect midpoint ties; exhaustive
  half round trips/boundaries and affected Terrain/native-module regressions
  pass on all systems. All 23 shader pipelines now use the same renderer bank.
  See Geiss64/native/prismflow/VALIDATION-GPU.md and dawn/VALIDATION-PRECISION.md.
  D33 remains open for feedback/cache, remaining draw passes, image/text and
  render-controller integration. No release artifacts are replaced.
- D33 now executes RGBA16F feedback/exposure with a twenty-map/eight-slot cache.
  Sixteen-frame unchanged-renderer captures match all 365,568 half components
  exactly on same-adapter Windows, Metal and Linux SwiftShader. NVK passes
  the fixed cross-adapter limits. Gaussian bytes, uniforms and protected LRU
  evictions match. Forty normal/partial-failure cleanup cycles pass on those
  paths; Linux SwiftShader ASan/UBSan/leak detection also passes. Lavapipe's
  cross-adapter pixel difference remains recorded rather than weakening limits.
  See Geiss64/native/prismflow/VALIDATION-FEEDBACK.md. D33 remains open for
  particles/dots, text/image helpers, presentation and controller integration.
- D33 now executes native palette presentation/supersampling and palette UI.
  Twenty-four offscreen scenes across RGBA16F/RGBA8/BGRA8 match 3,710,976
  components exactly on same-adapter Windows, Metal and SwiftShader, including
  Linux sanitizers. Packed palettes and uniforms match; NVK passes fixed
  cross-adapter limits. Lavapipe's synthetic title scene exceeds those limits
  and remains recorded. See Geiss64/native/prismflow/VALIDATION-PRESENTATION.md.
  Confirmed reference HDR calibration index fault (last square 2.80 instead
  of 3.00) is retained explicitly for correction/regression under D37.
  Particles, text/image rasterization and controller integration keep D33 open.
- D33 now executes native explicit points and 1,024 persistent GPU warp dots.
  All 98,304 particle state components and 512 control bytes match same-adapter
  Windows/Metal/SwiftShader. Windows has six feedback half-component differences
  within existing limits; Metal/SwiftShader pixels match. Linux sanitizers and
  expanded forty-cycle cleanup checks pass; independent NVK/lavapipe lifetimes
  also pass. NVK's common-Windows particle-state difference remains a recorded
  failed comparison. See Geiss64/native/prismflow/VALIDATION-DOTS.md. D33 stays
  open for bokeh, image/text rasterization and complete controller integration.
- D33 now executes native bokeh simulation/depth/lifetime, death history,
  asynchronous statistics and RGBA16F/RGBA8/BGRA8 drawing. Fixed-limit comparisons
  pass on D3D12/Metal/NVK/SwiftShader, including Linux sanitizers: 131,856 particle,
  16,384 history and 3,502,080 draw components plus exact controls/counts. Actual
  GPU ticket rollover, 30 ms throttling, stale snapshots and quota recovery pass.
  Forty-cycle partial-constructor/draw/readback-cancellation checks pass,
  including independent lavapipe lifetimes and all 128 future slots recovered.
  Reproduced original finite-input spawn/history overflows are rejected by
  native guards. See Geiss64/native/prismflow/VALIDATION-BOKEH.md. D33 remains
  open for text/image helpers and full renderer/controller integration.
- D33 now executes native text-mask upload/state/timing and the final GPU stamp
  pass. Fifteen cases match all 921,600 output components exactly on Windows,
  Metal, NVK and sanitizer SwiftShader; lavapipe passes fixed limits. Forty-cycle
  cleanup checks pass on all paths. Original zero-duration/omitted-timing NaNs
  are reproduced; native timed/immediate paths stay finite. See
  Geiss64/native/prismflow/VALIDATION-TEXT-GPU.md. All seven compute/sixteen
  render-format variants now execute natively. Supplied glyph masks are only
  development fixtures: native fonts, image helpers and controller keep D33 open.
- D33 now has native DirectWrite/CoreText/Qt 6 font rasterization. Unicode,
  strict bounds/UTF-8, whitespace and repeatability checks pass on all three
  systems, including 3,000-call resource samples with stable handles/descriptors.
  Confirmed original left-bearing/leading-space clipping is corrected and
  guarded by independently padded renders. Actual native glyph upload and
  1x/2x/4x stamping pass on D3D12/Metal/NVK/lavapipe with explicit CPU sampling
  quantization limits; original-GPU comparison limits remain unchanged. See
  dawn/VALIDATION-FONTS.md. Linux CPU/lavapipe sanitizers pass; NVK leak detection
  reports 64 driver-allocated bytes, also reproduced by the font-free Dawn
  foundation probe. That unsuppressed failure remains recorded for D60/D63.
  Full image services and renderer/controller/host
  integration keep D33 open; no release artifacts are replaced.
- D33 now has the complete native image-palette algorithm and WIC/ImageIO/Qt
  file services. All 457 identical-RGBA cases match the preserved algorithm's
  random consumption and numeric limits on Windows/Mac/Linux plus Linux
  sanitizers. The 23-file corpus checks real decode/profile/orientation/alpha,
  Unicode names, malformed input and repeated release. Corrected native Mac
  row inversion, WIC GIF canvas/EXIF 5/7 mappings and Qt GIF background handling.
  Native resampling/color rounding can change image-derived palettes; the
  original-file comparisons record that difference without claiming full image
  parity. See dawn/VALIDATION-IMAGES.md and
  Geiss64/native/prismflow/VALIDATION-IMAGE-PALETTE.md. Drop/controller/host and
  complete scene integration still keep D33 open. No final installer is rebuilt.
- D34 now passes native frame orchestration, complete scene and API 4 module
  checks. All 144 CPU frames match point/wave samples on Windows/Mac/Linux;
  Linux CPU sanitizers pass. Thirty-six complete scenes compare 5,381,100
  feedback/color components exactly on each same-adapter D3D12/Metal/SwiftShader
  path, including four/eight slots, transitions, resize, waveform/palettes,
  M/P/W/D/S and 1x/2x supersampling. Complete-scene SwiftShader ASan/UBSan/leak
  detection also passes with exact output. The reference harness fixes its own
  import-time RNG capture and mutable diagnostic snapshots. Native window
  key/fullscreen/closure checks pass on all three systems. See
  Geiss64/native/prismflow/VALIDATION-FRAME.md and dawn/VALIDATION-PRISMFLOW.md.
  The NVK-versus-SwiftShader full-scene test fails unchanged numerical limits;
  that result remains recorded. Windows capture/reselection works with 2,560
  FIFO drops recorded. The rebuilt Mac capture check awaits OS permission;
  earlier approved capture results remain separate. D33 stays open for image
  drop/worker and remaining host controls; D35/D60/D63 retain open issues.
- D33 now includes native image drop and render-thread palette publication.
  A single cancellable worker keeps one running/pending/result slot; 200 rapid
  requests decode only the held and newest file. Cancellation, failure recovery,
  40 normal/8 interrupted lifetimes, 120 real imports and eight module closes
  with work pending pass. An optional C control extension preserves base API 4
  compatibility and rejects malformed tables. Worker/control checks pass on
  Windows/Mac/Linux and Linux ASan/UBSan/leak detection. All three native window
  drop/recovery/newest-result sequences pass; complete 36-scene output remains
  exact on same-adapter D3D12/Metal/SwiftShader. See
  Geiss64/native/prismflow/VALIDATION-IMAGE-IMPORT.md. Native-codec color/resampling
  variation, NVK failures, final physical UI/capture checks, production dispatch
  and release/security gates remain separate open work.
- D40 now dispatches API 4 packages through the existing production frontend
  into one native runtime. Twelve local integration cases pass on Windows,
  Mac and Linux: both modern modules, native settings for all three IDs,
  original Geiss/MilkDrop/projectM initialization and existing key checks,
  fresh manifest over stale cache, post-scan missing/incompatible rejection,
  and two windows receiving generated player PCM with capture isolation and
  owner-exit/IPC cleanup. See dawn/VALIDATION-DISPATCH.md. Original-host
  standalone capture still needs its native helper; public catalog, complete
  engine behavior, real player playback, installers and security gates remain.
- D41 now supplies native development manifests, a pinned API 4 schema and
  compatibility declarations separate from preserved API 3 artifacts. The
  independent Audio Color third-party module builds and passes real scanner/
  frontend load, render, resize/fullscreen, menu-close, safe removal and rescan
  checks on Windows/Mac/Linux. Its plugin-only NSIS example compiles; installer
  execution/security QA remains open. All four example schemas and 25 native
  rejection cases pass. See VLC-Visualizer-Catalog/DAWN.md and
  VALIDATION-DAWN.md. Old release variants/tags are retained pending native
  packaging; no unreleased native installer or repository URL is advertised.
- D45 implementation now has an original-engine native capture helper on
  Windows/Linux. Generated and real WASAPI/PulseAudio checks pass reselection,
  cancelled dialogs and owner/IPC cleanup through Geiss Classic and original
  MilkDrop/projectM. Ten concurrent requests retain one writer; wrong nonce,
  forty failed initializations, startup cancel and foreign-owner rejection
  pass. Exact streaming/history tests and Linux C/C++ ASan/UBSan/leak checks
  pass, including four real recording cases. All eight regular real-backend
  cases report zero FIFO-overflow/transport-drop counters, with queued source/
  shutdown tails explicitly recorded. Canonical hosts no longer dispatch
  Electron. See dawn/VALIDATION-CAPTURE-SERVICE.md. D45 remains open for Mac,
  packaging/N-API removal and final SDK integration; no release is rebuilt.
- D71 now verifies 114 matching shared SDK source hashes across all renderer
  repositories. The common runtime is module-only and has no Terrain/Geiss
  source dependency; independent runtime/module builds pass on Windows/Linux.
  Independent Prism Flow matches the direct renderer at all 1,002,000 compared
  components. Independent-module dispatch and five workbench lifecycle cases
  pass on Windows/Linux. See dawn/VALIDATION-SDK.md. Mac rebuilds stay deferred
  under M02. Keep SDK snapshots synchronized as packaging changes continue.
- Next: remove old production packaging dependencies, then remaining menu/player,
  installer, soak and full release QA. Mac verification stays in M01-M09.
- Owner is taking macbook1 offline temporarily. Mac return tasks M01-M09
  preserve the outstanding platform work; continue independent Windows/Linux
  tasks and wait for the owner's availability update before accessing the Mac.
  Existing candidate versions/tags/artifacts remain preserved.

- D56 selects 0.5.0-rc.1 for the native candidate. All prior release artifacts,
  checksum files, metadata and tags are recorded in dawn/PRESERVED-RELEASES.json
  and retained unchanged. The 0.4 candidate security/QA results move intact to
  previous_candidates; new native installer/security results start pending.

- Windows combined native packaging now builds a 20,149,163-byte 0.5.0-rc.1
  draft with one Dawn runtime and plugin-only selectable components in the
  reversible patch transaction. All 36 PE import audits pass. Fourteen local
  checks on vpntest1 pass all eight selections, scan, exact NSIS restoration,
  modified-module refusal, locked-file rollback and invalid selectors. Active
  local-only Defender reports no detection; test copies are removed and saved
  preferences restored. See dawn/VALIDATION-PACKAGING.md for exact hash/limits.
  All 62 preserved artifact/metadata/checksum files and old tags remain intact.
  Independent/Winamp and Linux/Mac packaging, full size accounting and final
  installer/security QA were still pending at that stage. SDK hashes match 121
  source files across all four copies. Mac remains deferred under M01-M09.

- Independent Windows renderer installers now package native runtime/plugin
  files and both Winamp bridges, with one runtime for combined standalone/
  Winamp selections and plugin-only VLC installs. Native transactions cover
  all roots and generated shortcuts; occupied/modified/locked files and a
  competing target are preserved. All four SDK snapshots match 123 files;
  independent control-tool builds pass in all three renderer repositories.
- 34 native filesystem tests and 36 actual NSIS installer checks pass on
  vpntest1, plus a Unicode/spaced NSIS destination. All 14 combined-v2 checks
  pass. A reproduced NSIS macro-jump/default-selection defect is fixed in V3.
  Exact individual drafts measure 5.36/7.01/5.10 MB; combined-v2 is 20.17 MB.
  See dawn/VALIDATION-RENDERER-INSTALLERS.md for hashes, scope and retained
  failed-draft evidence. Nine local Defender custom scans complete with no
  new detections; remote copies/registrations are removed and original
  preferences restored with all protections enabled. This does not clear the
  historical 0.2 detection or substitute for real Winamp playback/HDR/soaks.
- Next: migrate Linux production staging/install archives, then remaining
  real-player, HDR, numerical/stability and final release gates. Mac work
  stays deferred under M01-M09. Do not mark global packaging/security items
  complete from the Windows-only checks.

- Linux production archive staging now uses native runtime/module/asset plans,
  pinned Dawn provenance and ELF/transitive system-dependency audits. Combined
  and independent archives contain one shared live Dawn runtime; standalone
  installs no longer require the addon and --mode vlc adds plugin files only.
  The system VLC install remains unchanged. Independent packaging/builds work
  from the renderer repositories' own shared SDK, now 127 matching source files.
- 18 actual Linux archive checks, 14 native fault cases and the same 14 under
  ASan/UBSan/leak detection pass. All 12 staged and 12 installed dispatch cases
  pass with lavapipe and generated player PCM. Reproduced archive permission
  normalization and post-preflight original-file overwrite defects are fixed.
  See dawn/VALIDATION-LINUX-PACKAGING.md for exact scope and draft sizes
  (8.57-9.79 MB), retained evidence and remaining GPU findings. Installed size
  still includes verification snapshots/backups; full size accounting stays open.
- Low Mint disk space required removing generated object caches and verified
  expanded copies; source, installed SDK, reports and old releases are retained.
  Current archives/reports were copied back with matching hashes. macOS now
  fails explicitly at its deferred builder boundary instead of packaging
  Electron; M02/M09 record the CommonCrypto/builder/relocation/signing follow-up.
  Remaining real-player/HDR/numerical/soak and final-source release gates remain.

- Windows native HDR now checks the actual monitor/OS HDR state, the float16
  surface and the module's declared linear capability before selecting scRGB.
  Eleven policy cases and 30 GPU conversion cases pass on D3D12, NVK and
  lavapipe. A real Windows surface switches float16/SDR/float16 with injected
  display states; no physical HDR result is claimed. SDR-only plugins retain
  SDR output. Both modern modules pass HDR-off/requested production checks on
  Windows and Linux SDR displays (four cases each). The eight-case window
  regression passes. All three independent Windows runtime builds compile;
  shared SDK snapshots match 128 source hashes. See dawn/VALIDATION-HDR.md.
  At this stage, D37 still required Mac EDR/M05, Linux HDR transport and the
  calibration-square clamp correction. Mac was offline at the time.

- Corrected the native calibration final-square index without changing preserved
  upstream assets. A before-fix GPU failure is retained; 56 cases / 6,496
  channels now pass on D3D12, NVK and lavapipe. Windows/SwiftShader captures
  prove only 2,900 intended pixels change; the other 23 outputs are exact.
  Corrected native presentation matches 3,710,976 channels exactly on both
  same-adapter paths; 36 ordinary Windows scene frames remain exact. The
  lavapipe title mismatch still fails its original limits, so D35 remains
  open at this stage. Mac calibration reruns are recorded in M08.

- Source archive preparation now uses fresh version-specific directories in all
  five repositories, preserving earlier metadata/checksums. Fixed a reproduced
  HEAD/source-metadata race, Git-setting line-ending conversion and Unicode
  console failure. All 45 source-helper cases pass on Windows and Linux with
  actual Git archives. Final source/binary artifacts and Mac execution remain
  D72/D73/M09; see dawn/VALIDATION-SOURCE-ARCHIVES.md.

- D35 now has a development-only original-JavaScript oracle on the same pinned
  native Dawn backend as C++. All 24 presentation cases / 3,710,976 components
  and 36 scene frames / 5,381,100 components match exactly on Windows D3D12 and
  each Linux NVK/lavapipe/SwiftShader path, including a fresh build of the
  tracked Linux binding recipe. Windows presentation also matches all 49 raw
  corrected browser-reference input/output files. Original-to-original title
  and first-feedback comparisons reproduce the retained cross-backend errors;
  production renderer/shaders and comparison limits are unchanged. The
  independent NVK driver allocation finding remains open. See
  Geiss64/native/prismflow/VALIDATION-NATIVE-REFERENCE.md. Final/Mac checks
  keep D35 open; M08 records the new Metal reference run. Node/reference
  bindings stay outside production payloads. No Mac access was attempted.

- D43 now has 46 cases passing on Windows and Linux through actual scan/check
  commands and public/native Open/Settings launch paths: 160 expected rejection
  exits, four >=90-frame valid/stale-cache launches and four concurrent scans
  per platform. Real Windows junction/Linux symlink escapes and healthy-plugin
  preservation pass. Fixed two reproduced defects: accepted NUL entrypoints
  and invalid Unicode diagnostics aborting whole scans. Windows 34-case
  filesystem tests and Linux 14-case regular plus ASan/UBSan/leak-detection
  installer regressions pass. All four SDK snapshots match 128 hashes. See
  dawn/VALIDATION-CATALOG-SAFETY.md. Real VLC QAction/Mac/final-package checks
  remain open; no installer/release/tag was replaced or Mac contacted.

- Real Linux VLC 3.0.20 menu actions now pass Terrain Open/Settings/Close,
  paired Prism Flow/Terrain Close all, a stale-click missing-module warning,
  removal/restoration scans and three-entry persistence without rescanning on
  a monitored restart. Actual decoded PCM is measured in both native engines.
  Windows VLC 3.0.24 shows correct placement/three entries and opens Terrain;
  its direct-input checks await app-control approval. A mis-staged Windows
  backup exposed recursive wrapper launches; the new original/core guard
  passes 12 read-only cases, independent scan, normal headless VLC launch and
  import audit. See dawn/VALIDATION-LIVE-MENU.md and VALIDATION-LAUNCHER-GUARD.md.
  Mac, Winamp UI, full media matrix, soaks and final source/installer/security
  gates remain open. Source, SDKs, releases and diagnostic evidence are retained.
  A subsequent real-menu projectM run passes 956 frames with decoded PCM,
  fullscreen/restore and key round-trip checks, plus normal host/VLC exit.
  Windows Prism Flow also passes actual menu Open/Close with 10,091 frames;
  original MilkDrop passes menu Open and CLI closure with 2,388 frames. Both
  consume nonzero player audio. Direct window/key and remaining menu checks
  stay open; CLI closure is not counted as a MilkDrop GUI-close pass.
  Terrain later closes through VLC after 3 h 20 min / 718,474 frames. Two
  resource samples 393 seconds apart have unchanged memory/GPU/handle/thread
  counters. All private player/renderer processes and control records are gone.
  A later desktop Defender read finds cloud/sample reporting enabled; earlier
  automatic transmissions are unverified, and further desktop binary tests
  wait for the owner's privacy-mode choice. The separate guarded-installer
  vpntest1 run passes with verified local-only mode and complete restoration.

- Mac return 2026-11-17: M02 rebuild/SDK/helper/staged dependency/signature and
  M05 Metal EDR/fallback checks pass. Four actual AppKit activation/PCM cases
  pass. Exact original-JavaScript Metal parity: 24 presentation/36 scene frames;
  calibration: 56 cases. Native archives: 18 ownership/selection/refusal cases
  after confirmed Mac app-path and symlink-publication fixes. Catalog: 45
  passes/one APFS EILSEQ unsupported fixture. Four generated original-host helper
  and 45 source-helper cases pass. Real microphone launch refused before
  execution by automatic review; fresh permission pending. Earlier failed
  harnesses/archives retained, no external analysis upload, no final signoff.
  See dawn/VALIDATION-MAC-RETURN.md.

- Mac version/independent packaging follow-up: all four production bundles now
  derive version 0.5.0 from the candidate metadata. All three independent SDK
  builds/archives pass and the 18-case install matrix passes against them; two
  default-path/space cases also pass. Native UI, PCM/FIFO and generated helper
  single-writer/10-concurrent/40-failure cases pass. Fourteen ASan/UBSan
  transaction fixtures pass with leak detection disabled: this Apple runtime
  explicitly rejects detect_leaks=1 before execution. Real capture remains
  pending approval. Five exact-commit source drafts are prepared, without tags
  or publishing. That earlier dependency/binary OS floor is macOS 26.0;
  the later compatible dependency rebuild is recorded below.

- Mac concurrent generated-audio soak: all four engines complete ten minutes
  concurrently, 70,140 total frames, with orderly cleanup and zero remaining
  test processes/control records. Sampled RSS after warm-up is retained; it is
  not a leak detector or final extended-soak signoff. M08/D61 remain open.

- Mac menu/deployment follow-up: 60 actual AppKit/VLC checks pass, including
  all menu action groups, transport lifecycle, stale-click safety and 40
  generated format/engine combinations. Fixed a reproduced malformed-UTF-8
  Cocoa alert exception; two native profile-error regressions also pass.
  Fresh pinned Dawn/SDL, all production modules/apps and four archive drafts
  target macOS 11.0. Settings/palette floating conversion no longer requires
  macOS 26. New SDL normal-build and provenance verification pass; all 133 SDK
  source hashes match. New-dependency Metal parity/calibration/presentation,
  18 archive cases, four HDR fallback, five window and ten dispatch cases pass.
  Actual older macOS is untested. Independent final-source builds, microphone,
  manual input and global release gates stay open. See
  dawn/VALIDATION-MAC-MENU.md and VALIDATION-MAC-COMPATIBILITY.md. No external
  analysis upload, replacement release/tag or microphone test was performed.

- Independent Mac deployment follow-up: all three repositories build/package
  from their own 133-file SDKs with the new Dawn/SDL dependencies, and all
  packaged binaries declare minOS 11.0. The 18-case ownership matrix and four
  actually installed generated renderer runs pass; original Classic/projectM
  fullscreen/key fixtures also pass. Exact commits/hashes are recorded in
  dawn/VALIDATION-MAC-COMPATIBILITY.md. Verified duplicate fixture cleanup
  preserves original archives/builds/SDKs/profiles/reports and restores about
  136 MB. Mint still has only 140 MB free. Reconciled five release guides and
  changelogs plus the suite release checklist with the native candidate;
  physical HDR remains waived in favor of Experimental status. Global final
  source/license, capture, player and stability/security gates stay open.

- Catalog release reconciliation reproduces two concrete omissions: the old
  data ZIP lacked the documented validator and any source-commit record.
  The packager now snapshots a clean fixed Git commit, includes its tools and
  release guides, validates that delivered snapshot and records commit/hash
  metadata. Eight isolated actual-Git cases pass on Windows, including dirty
  sources, ignored files, output preservation and a real HEAD/VERSION change.
  Before-fix artifacts remain in Catalog/build/catalog-release-before-v1;
  see the Catalog VALIDATION-RELEASE.md. No application binaries or external
  analysis service are involved. Final suite release gates remain open.

- Catalog packaging follow-up: all eight actual-Git cases pass on Mac and
  Linux as well as Windows. Real catalog/source ZIPs from clean e1fcc6d are
  byte-identical across the three platforms (44,633 and 48,908 bytes), with
  verified commit metadata/checksums and the delivered validator passing.
  Private Git PATH and a transient Linux disk preflight were setup issues;
  no system Git install or GPU test was performed. About 513 MB remains on
  Mint, so larger renderer builds still need a separate space preflight.
  The new source-only artifacts remain unpublished and all other suite
  release gates remain open. See dawn/VALIDATION-CATALOG-RELEASE.md.
