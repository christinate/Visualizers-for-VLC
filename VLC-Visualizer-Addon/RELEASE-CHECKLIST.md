# 0.5.0-rc.1 native candidate release checklist

The native candidate is local and unpublished on `feature/dawn-native`.
[todo.md](todo.md) is the canonical detailed checklist; this page groups its
release gates. Old 0.4 and earlier artifacts, tags and recorded hashes remain
unchanged. An older checklist or green development test does not qualify the
current source/binary combination.

## Observed development results

- Native C++/Dawn production packaging replaces Electron/Chromium/Node.
  The combined Windows draft contains one shared runtime and five optional,
  selected-by-default plugin components. Renderer installers are independent.
- Original-JavaScript presentation and scene comparisons pass exactly on the
  documented same-adapter D3D12, Vulkan and Metal paths. Corrected calibration
  cases pass. The renderer's `native/prismflow/VALIDATION-NATIVE-REFERENCE.md`
  records exact scope; the [Mac report](dawn/VALIDATION-MAC-COMPATIBILITY.md)
  records the Metal reruns.
- Bounded Windows/Linux real VLC menu/PCM checks pass. Windows Terrain runs
  3 h 20 min with orderly closure; sampled resource counts retain their scoped
  limits. See [live menu validation](dawn/VALIDATION-LIVE-MENU.md).
- Mac passes 60 actual menu/media checks, including 40 generated format/engine
  combinations, stale actions, rescans and owner exit. This is in-process
  AppKit handler delivery, separate from manual pointer/key input. See
  [Mac menu validation](dawn/VALIDATION-MAC-MENU.md).
- Native draft NSIS and Linux/Mac ownership, selection, modified-file refusal,
  rollback and restoration checks pass. A guarded Windows draft passes local
  Defender checks on vpntest1 with verified local-only settings and restoration.
  See [the exact artifact report](dawn/VALIDATION-PACKAGING-GUARD.md).
- Fresh Mac dependency/app/module drafts target 11.0 and pass bounded tests on
  26.6.2. All three independent renderer builds/packages and an 18-case archive
  matrix pass again with those dependencies. The app's minimum matches all
  packaged Mach-O dependencies; actual older macOS and Intel Macs remain
  unqualified. See [Mac deployment validation](dawn/VALIDATION-MAC-COMPATIBILITY.md).
- Shared SDK source hashes, immutable source-helper tests and preserved-release
  checks pass. These do not replace building/verifying final artifacts from
  their exact source commits.
- Project 4's catalog and source ZIPs are prepared from one clean commit and
  are byte-identical on Windows/Mac/Linux. Eight actual-Git packaging cases
  pass per platform; its delivered validator/tools and source metadata are
  included. See [catalog release validation](dawn/VALIDATION-CATALOG-RELEASE.md).

## Gates for the intended release

- [x] Verify deterministic reference rendering and original-engine
  compatibility paths (D35-D36).
- [x] Verify Experimental HDR transport and SDR fallback (D37).
  The earlier Terrain package uses SDR on nested Muffin Wayland with HDR off
  or requested. The compositor reports no HDR presentation color space.
  The beat-polling package passes eight nested Wayland SDR output checks
  across Terrain, Prism Flow, Beach and Objects.
  A private Weston headless compositor now exposes the exact float16
  extended-linear sRGB pair for the Dawn transport experiment.
  The exact patched Linux x64 archive passes 12 real VLC cases across all
  four scenes. Weston desktop-shell covers HDR opt-in, SDR with HDR off,
  and SDR fallback when the compositor color-management protocol is disabled.
- [x] Verify real AMD64 VLC playback and a physical Windows audio endpoint (D44).
  The real AMD64 VLC menu and playback checks pass across all formats.
  The current Mint and Mac packages each pass 60 format/engine combinations.
  The beat-polling Mint rebuild repeats all 60 cases and 16 menu/lifecycle
  groups. It also passes the scheduled Terrain, Beach and Objects beat
  fixtures in real Mint VLC.
  All 60 Windows x64 engine/format combinations decoded PCM and rendered
  in VLC 3.0.24. Fifty-nine met every gate. Terrain with stereo Opus hit
  the duration limit before the Escape check on the software D3D12 adapter.
  The ordinary VLC menu rerun passes the Terrain/Opus case with 222 frames
  and actual Escape closure. The virtual Windows sound card verifies the
  replayed stereo signal through MMDevice.
  Stock LibVLC 3.0.24 AMD64 also plays a generated fixture through the
  workstation's default Realtek headphones. Its MMDevice session meter
  verifies all seven expected transitions between tone and silence.
  Actual Windows VLC menu controls, catalog persistence, 12 held-action
  rejections and four scheduled rhythm checks pass.
  [Scene release follow-up](dawn/VALIDATION-SCENE-RELEASE-CHECKS.md)
- [x] Finish native production-path, dependency, component and size audits for
  the final packages (D45/D50-D52/D55/M09).
- [x] Collect the matching macOS soak and finish extended resource coverage
  (D61). The earlier Mac soak and isolated Mint soak pass.
  The exact beat-polling package passes a 30-minute, four-renderer VLC
  soak on Mint. The earlier Mac run lost SSH before its report was collected.
  The corrected Windows VLC run passes 30 minutes with 102,036 frames and
  zero remaining ownership records. Initial GPU samples are recorded.
  The fullscreen-fix Windows x64 payload repeats the 30-minute VLC soak.
  It records 43,140 rendered frames and 124 process samples. The test
  verifies all 384 staged files and releases all ownership records.
  GPU-memory counters are unavailable on the virtual adapter in this run.
  The final macOS arm64 package passes a 30-minute VLC soak across four
  renderers with 210,699 frames and 240 process samples. GPU process mappings
  were sampled after warm-up and near the end. Beach and Objects pass separate
  playback checks from the same archive.
- [x] Verify native window recovery and real VLC lifecycle controls (D62).
  The rebuilt Windows x64 payload passes all 12 check groups. Terrain,
  Beach and Objects restore their resized dimensions after fullscreen.
  The rebuilt Linux stage passes five native window cases. Seven earlier
  live VLC controls also pass on the beat-polling Linux release.
  The Wayland HDR source update repeats all 12 Windows VLC checks on its
  rebuilt x64 payload with the same exact restoration bounds.
- [x] Reconcile release documentation and package/source provenance and
  build the candidate artifacts (D70/D72).
  Windows x64 and Linux x64 installers and five source archives include the
  independent beat detector. The matching macOS arm64 build includes it too.
  The fullscreen-fix Windows and Linux drafts each have six verified
  installers. The new release index verifies 110 Git inputs and five
  source archives.
  The final candidate has six packages per platform. Its index verifies
  142 inputs, the Linux Dawn patch and library, and the Mac arm64 binaries
  and dependency provenance.
- [x] Prepare five independent unpublished release drafts with exact source
  archives and artifact checksums (D75). The new index covers 12 rebuilt
  installers, the catalog and 142 Git inputs checked against Windows and Linux
  build ledgers. The preceding three-platform index records 19 artifacts.
- [x] Verify Terrain, Beach and Objects controls, actual renders and the new
  combined and standalone packages (T01–T06).
  [Scene validation](dawn/VALIDATION-SCENE-VARIANTS.md)
- [x] Close the remaining validation items and verify candidate tags and the
  complete release record (D73-D74).
  The five local `v0.5.0-rc.1` tags match the archived build commits.
  The final packages, size comparison and results are listed in
  [the release validation report](dawn/VALIDATION-SCENE-RELEASE-CHECKS.md).

Repository URLs are intentionally unset until supplied by the owner. Local
Mac archives use ad-hoc signatures, not Developer ID signing/notarization;
Windows drafts are unsigned. Keep those limits explicit when preparing a
candidate, and recalculate hashes/retest any artifact changed by signing.

## Accepted limits and unresolved findings

HDR remains **Experimental**, opt-in and off by default. The owner accepted
that scope for physical HDR signoff. The Mac app has observed both fallback
at headroom 1.0 and active Metal EDR at higher headroom. Linux uses opt-in
extended-linear sRGB on a supported Wayland compositor and SDR elsewhere.
Physical HDR output remains unmeasured.

Mint uses lavapipe for window tests. NVK window-surface support in its Xorg
session and an unsuppressed 64-byte driver/Dawn allocation finding remain open.
The current source-built packages and exact NVK/lavapipe offscreen comparisons
are recorded in the latest validation report. [Candidate validation](dawn/VALIDATION-CANDIDATE-2026-11-19.md)

The owner approved a microphone test for the production Mac app identity
`org.vlcvisualizers.native`. Its default CoreAudio input delivered 3,961,856
frames with zero drops. Real original-engine checks pass. The owner-present
held-picker regression also passes on the rebuilt app (M04).

Installer and Defender testing are outside the current release checklist.
Their earlier results remain in the historical reports.

The window-control follow-up fixes stale registrations after forced exits.
The rebuilt Mac and Linux stages pass forced-exit recovery and repeat window
control tests. The Mac stage passes the 60-case real VLC matrix again. The
follow-up records the current package provenance and remaining platform gates.
[Window-control follow-up](dawn/VALIDATION-WINDOW-CONTROL-FOLLOWUP.md)

The 0.2.0-rc.1 Defender behavioral quarantine remains unresolved. No Microsoft
cloud determination is claimed. No application sample/report has been
explicitly submitted by these tools to an external analysis service; earlier
desktop automatic transmissions are unverified. Follow
[SECURITY.md](SECURITY.md) and the owner-local analysis requirement.
