# Mac return validation — 2026-11-17

Development evidence for 0.5.0-rc.1. Final-source releases, real microphone
permission/capture, manual input, sanitizers and extended soaks remain
gates in [todo.md](../todo.md). No artifacts were published or submitted for
external analysis. Audio is not saved by the capture implementation.

Follow-up results: [60 real VLC menu/media checks](VALIDATION-MAC-MENU.md) and
[fresh macOS-11-target dependencies/packages](VALIDATION-MAC-COMPATIBILITY.md)
now pass. The measurements below preserve the earlier macOS-26-target drafts;
their hashes and OS requirements have not been retroactively changed.

## Environment and source

The owner made macbook1 available again. The machine reports macOS 26.6.2
(25G83), arm64 Apple M1, AppleClang 21 and CMake 4.4.5. Initial free disk was
3.76 GB. Fresh exact-commit clones live under
`~/vlc-visualizers-work/mac-return-20261117-v1/src`. Old source, builds and
reports remain intact. Source-only Git bundles and their SHA-256 manifests
record each update; no worktree was silently merged with an earlier build.
For the initial drafts below, the actual Mach-O minimum OS is 26.0 (build SDK 27.2), including shared
Dawn/SDL. These drafts require macOS 26+; older macOS and Intel Macs remain
unqualified. M09 retains deployment-compatible dependency builds before an
earlier baseline can be claimed.

The production shared Dawn installation remains at pinned revision
`7daf3b0bee2c73621001ab15b256b5a84fb0b45f`, Node bindings OFF. Its dylib SHA-256
is `37c8000717a2fc26af9b9baecc4a43e1cfe6bba660bd91969586980bfe4228bc`.
SDL2 is the retained source-built 2.32.10. New arm64 host, controller,
launcher, CommonCrypto installer helper, VLC module, native app, Prism Flow,
Terrain, Classic and projectM modules compile successfully.

## Metal presentation and original-engine parity

| Check | Observed result |
| --- | --- |
| Capability/conversion probe | 29 policy cases, 50 GPU cases, 12,800 components; pass within unchanged limits |
| Real Metal surface | 457 frames; float16/8-bit configuration and color-space checks pass |
| EDR headroom transitions | Injected inactive/active/inactive observations select modes 3/1/3 on one real float16 surface configuration |
| Production Prism Flow/Terrain | Four HDR-off/on cases, 90 frames each, Escape closure, no recording; pass |
| Calibration | 56 cases/6,496 channels; exact final-square endpoints 3.0/2.75; existing half-step limits pass |
| Original JavaScript presentation | 24 cases, 3,710,976 pixel components and 24,576 packed palette bytes exact |
| Original JavaScript full scenes | 36 frames, 5,381,100 components exact |

The new Metal path sets the real CAMetalLayer EDR flag, extended linear sRGB
and a float16 surface when requested and supported. EDR uses relative SDR
white 1.0 rather than Windows' 203/80 scRGB scale. Actual current headroom was
1.0 and potential headroom 2.0. Production therefore used the explicitly
reported linear-surface SDR fallback when HDR was requested. Injected active
observations exercise policy/lifecycle; they do not establish physical HDR
appearance. HDR stays Experimental, opt-in and off by default.

This follows Apple's [EDR tone-mapping guidance](https://developer.apple.com/documentation/metal/performing-your-own-tone-mapping)
and [headroom guidance](https://developer.apple.com/documentation/metal/determining-support-for-edr-values).
Linux native HDR transport remains a separate open item.

The original-JavaScript oracle uses stock Node 24.22.3 arm64 with native
Float16Array and the official pinned Dawn Node adapter linked to the existing
shared SDK. Its platform-independent generated IDL files come from the earlier
official pinned Windows Node build; every supplied original Dawn source file
was checked against the clean Mac source pin. Input/file hashes, binding
provenance, raw reference frames and comparison reports are retained. Node,
the binding and reference scripts are excluded from production payloads.

## VLC, catalog and generated capture

Four actual VLC/AppKit delegate activation fixtures pass: Terrain, Prism Flow,
projectM and Geiss Classic. Each menu has exactly three entries. The observed
Mac order is `VLC, File, Edit, View, Visualizers, Playback, Audio, Video,
Subtitles, Window, Help`. VLC decodes generated 48-kHz stereo media through the
actual audio bridge, with float PCM peak 0.3661828935 for modern renderers and
legacy-host peak 0.367188. Modern runs present 90 frames and close by Escape;
original-engine fullscreen restoration and key round-trip fixtures also pass.
This does not claim manual pointer input, Settings/Close/Close-all actions or
the complete media-format matrix. The Mac self-test cutoff is now 20 seconds,
matching the 90-frame contract already used by the Qt test fixture.

The catalog matrix completes with 45 passed cases and one unsupported fixture:
APFS rejects the invalid UTF-8 filename with EILSEQ before a scanner can read
it. Malformed JSON/UTF-8 content, NUL/traversing paths, wrong ABI/architecture,
missing/changed packages, resolved directory symlink escapes, stale caches,
Open/Settings refusal and four concurrent persistent scans are exercised.
Valid frontend/direct-runtime launches render actual frames. The initial
invalid-filename harness failure remains preserved separately.

Four generated-only original-host helper cases pass: Classic/projectM ×
reselect/cancel. Each handles three input requests, keeps the expected capture
activation count, publishes PCM and releases IPC/control state after owner
exit. Generated mode never opens a microphone or requests recording permission.
Real capture and the distinct production app's permission remain unverified.
An attempted microphone launch was rejected before execution by automatic
approval review because older approvals covered different app identities;
fresh owner permission was requested. TCC was not reset or edited.

Further generated-only checks pass on the version-corrected bundle: ten
concurrent helper requests share one writer; a bad nonce is ignored; 40
invalid-input attempts release IPC/lock state; startup cancellation publishes
no PCM; and bare `--line-in` selects the generated default. The real-mode
owner check is explicitly untested in this invocation. Native AppKit settings
and picker fixtures pass Unicode/duplicate-label identity, unchanged numerical
precision, display/HDR/engine/supersampling/Terrain control round-trips,
cancellation and owner closure without opening recording devices. FIFO checks
cover 1,280,000 threaded and 640,000 streamed frames. The player PCM/process
watch probe passes too.

Fourteen native transaction/fault fixtures pass with Apple ASan/UBSan halting
on supported errors. Leak detection is unsupported by this Apple runtime: its
initial `detect_leaks=1` run aborts before fixtures and is retained. The passing
run uses `detect_leaks=0`; it is not a leak check and does not instrument GPU
or system libraries.

## Bounded concurrent soak

Four separate generated-audio windows run concurrently for ten minutes on
the version-corrected staged suite. All exit 0: Terrain 17,998 frames, Prism
Flow 17,982, Geiss Classic 17,061 and projectM 17,099 (70,140 total). Terrain
visits all four camera modes. Legacy hosts receive repeated preset-key
requests. Native renderers report orderly renderer/presenter/context/window
release. Post-run process and suite-control-record checks both find zero.

| Process | RSS at ~120 s (KiB) | Last sampled RSS (KiB) | Largest sampled RSS after 120 s (KiB) |
| --- | ---: | ---: | ---: |
| Terrain | 76,000 | 49,648 | 76,320 |
| Prism Flow | 103,328 | 75,584 | 107,184 |
| Classic | 72,848 | 48,144 | 73,136 |
| projectM | 79,136 | 58,176 | 82,912 |

Samples are about 30 seconds apart; most final observations are at ~572 s,
Prism's at ~602 s. RSS may fall through paging/reclamation and is not an
allocation-leak detector. Virtual reservations also change in some processes;
raw samples are retained without diagnosing a leak from reservation totals.
These observations do not track continuous peaks, private/GPU allocations or
indefinite stability. Longer final-source/real-media checks remain open.

## Native Mac archives and confirmed installer fixes

The new arm64 builder creates combined and standalone native archives with
one shared Dawn library per payload, SDL2, native modules, assets and notices.
It strips development LC_RPATH entries, rewrites bundled dependencies to
contained `@loader_path` references, retains projectM's relative SONAME link,
and signs only staging copies. Static dependency, arm64 and strict signature
audits pass; the original signed VLC application remains unchanged. Local
ad-hoc signing is not Developer ID signing or notarization.

Two actual faults were reproduced and fixed:

1. Plugin-only install rejected a valid Mac suite because it checked the Linux
   `native/VisualizerNative` path. It now uses the platform's app-bundle path.
2. `link()` followed projectM's source symlink on macOS and failed with ENOENT
   while its target was unpublished. A direct libc reproduction confirms that
   `linkat(..., 0)` preserves the entry. Publication and rollback now use it.

All 18 archive install/ownership cases pass after both fixes: eight selection
combinations, exact original restoration, whole-package modified-file refusal,
changed SONAME-link refusal, standalone installs, plugin-only sharing,
independent ownership retention and invalid opt-out rejection. The first
failed matrix remains preserved. These are user-directory tests, not a final
release or system-wide installer qualification.

The app's scaffold 0.1.0 version is corrected: Addon and all three independent
renderer bundles now derive numeric 0.5.0 from the product/SDK candidate version.
Each renderer builds and packages independently from its own SDK. All 18
installation cases pass again using those independent archives and the corrected
combined draft. Two further tests pass with default HOME paths containing
spaces. Plugin-only removal uses `--prefix EXISTING-SUITE`, including when
installation selected that suite by default.

| Development archive | Bytes | SHA-256 |
| --- | ---: | --- |
| Combined Addon | 5,158,966 | `22689119586f6c9584ccf894c61951afc6d520ef5f4d1bcb85c305745051e9ef` |
| Prism Flow | 4,484,252 | `9f77168fed8f566ffeadd54e1584ac1287b5db8e1fafdf8416b5747258958dab` |
| MilkDrop/projectM | 4,563,955 | `360aa31858d286f8d24f6a80c943616afd34747e20bfa50f7ac46e117455a6b9` |
| Terrain | 4,179,625 | `1f978ce57f823b210ca07245dcc4cc2d987535a3aa3e2e836ed0a4eb922362e0` |

The vendored projectM subproject also stamped the parent's 0.5.0 into its dylib
version. Using its own PROJECT_VERSION restores 4.1.8 metadata. Before and
after modules both load successfully; this is a confirmed metadata correction,
not evidence of a previous crash or load failure. The local patch and hash
are recorded in MilkDrop's dependency lock.

The immutable source helpers pass all 45 isolated actual Git/archive checks
on Mac, including concurrent HEAD/VERSION changes and preservation of prior
artifacts. Actual final project source/binary releases remain pending.
Five actual current-commit source-archive drafts are prepared locally, with
metadata binding their exact commits. Later documentation/tool changes do not
silently replace these snapshots. They have no final tags or published release.

## Local evidence

Paths below are relative to the fresh Mac workspace above:

- `build-addon`, `build-geiss`, `build-milkdrop-version-fixed`: builds/logs.
- `edr-numeric-metal-v1.json`, `edr-surface-metal-v1.json`,
  `production-hdr-metal-v1`, `calibration-metal-v1`: presentation evidence.
- `reference-inputs-v1.json`, `reference-binding-provenance-v1.json`,
  `reference-metal-{present,scene}-v1`, `metal-{present,scene}-exact-v1`:
  original/native comparisons and raw reference outputs.
- `native-package-suite-v2`, `native-archives-v2`, `mac-install-matrix-v2`:
  relocated/signature-audited drafts and installation checks.
- `native-package-suite-v3`, `native-archives-v3`, `app-version-v3.json`,
  `independent-builds-v1.json`, `independent-packages-v1.json`,
  `mac-independent-install-matrix-v1`, `mac-default-prefix-v1`: corrected and
  independently built packages. The table above preserves v2 artifact hashes.
- `production-window-metal-v3`: five production resize/fullscreen/closure and
  deliberate device-loss fixtures; expected outcomes and cleanup pass.
- `native-ui-core-v1.json`, `generated-capture-control-mac-v1`,
  `installer-sanitizer-mac-v1` (unsupported leak detection),
  `installer-sanitizer-mac-v2` (14 supported sanitizer cases),
  `source-archives-drafts-v1.json`: further local evidence.
- `concurrent-soak-metal-v1/{result,summary}.json`: complete samples, per-renderer
  outcomes and post-run process/control cleanup verification.
- `plugin-only-mac-before-v1`, `mac-link-semantics-v1`,
  `projectm-version-{before,after}`: before/after reproductions.
- `mac-vlc-menus-v2`, `catalog-safety-metal-v2`,
  `mac-generated-capture-v2`, `source-release-macos-v1`: runtime/helper checks.

A verified copy of 28 JSON reports is in the Addon's ignored local
`build/mac-return-evidence-20261117-v1/evidence-summary-v1`. The transfer archive
SHA-256 is `12696e3accbcc536f9716fa092d9b31d53744a999c3e9e2b645921539d9264ac`.
Raw frames stay on the Mac. Finite checks do not establish indefinite stability,
universal leak freedom or an explanation for historical whole-system crashes.

An additional 23 JSON/log evidence files are verified locally under
`build/mac-return-evidence-20261117-v1/evidence-summary-v2`; archive SHA-256
`462de1b48bd027a073714ec14928d33a72b02326064edc7027ee608bcc20c9c8`.
The version-corrected v3 combined draft is 5,159,087 bytes, SHA-256
`074705503c0c59eaea1b0de31b218f202a561b54b60662b1a559d5f5eda2eece`.
The latest static audit verifies 12 arm64 binaries, strict local signatures,
the production app identity/version and macOS 26.0 minimum. The v2 artifacts
and failed runs remain unchanged. All remaining release gates retain their
explicit unchecked status.
