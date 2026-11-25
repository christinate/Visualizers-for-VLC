# Native scan and launch revalidation

Measured locally on 2026-11-16 on Windows AMD64 and Mint Linux x86_64.
These are development-build checks, not final installer or real VLC menu
signoff. Mac remains deferred while macbook1 is offline. No external analysis
service was used; no recording input was opened or audio saved.

## Confirmed defects

1. Both real scanners accepted an entrypoint such as
   `terrain_dawn.dll\u0000invalid` / `terrain_dawn.so\u0000invalid`. Filesystem
   calls can use the prefix before NUL, so this string did not identify the
   file the manifest actually described. Both containment helpers now reject
   NUL before canonicalization or file inspection, covering entrypoint and
   cached package paths. Paths are rejected, never repaired into accepted paths.
2. Invalid UTF-8 in one manifest produced a parser exception containing the
   invalid byte, which aborted catalog JSON serialization on both platforms.
   The preserved Windows scanner also failed with `Invalid Unicode` when
   reporting a real directory containing an unpaired UTF-16 surrogate.
   Rejection messages/labels now use bounded valid UTF-8, replacing invalid
   bytes only in diagnostics. An unconvertible Windows filename gets an
   explicit diagnostic label. Malformed packages remain ineligible to load.

Before-fix failures are retained separately. Neither finding establishes a
cause for the historical whole-computer crashes or Defender detection.

## Coverage and limits

`tools/check_catalog_safety.py` copies an expendable suite to fresh output and
records binary/runner hashes. It invokes actual `VisualizersCtl scan`/`check`,
the public `Visualizers` frontend and `VisualizerNative` directly. Open and
Settings must reject invalid current files despite a previously valid cache.
Expected error exits exclude crashes/signals, and rejection must occur before
the window/renderer report path. Inputs are never modified.

The Qt VLC menu invokes that same CLI `check` before launching. This matrix
does not click a QAction in real VLC; that remains D42/D43.

| Check | Windows | Linux |
| --- | ---: | ---: |
| Package/cache matrix | 46 cases pass | 46 cases pass |
| Open/Settings frontend/runtime rejection exits | 160 expected | 160 expected |
| Valid and stale-cache rendering launches | 4 pass, each >=90 frames | 4 pass, each >=90 frames |
| Concurrent scans, identical persisted catalogs | 4 pass | 4 pass |
| Installer ownership/fault regressions | 34 pass | 14 pass |
| Installer ASan/UBSan with leak detection | not claimed | 14 pass, no sanitizer report |

Cases cover incompatible schema/API/Dawn pins, platform/architecture,
adapter/ID/name/version types, empty/invalid/NUL/absolute/traversing paths,
malformed/oversized/non-UTF-8 JSON, removed manifests/modules/assets/packages,
empty/wrong-architecture binaries, real package/assets directory escapes,
stale cache metadata and corrupt cache entries/paths. Windows uses actual
junctions and Linux actual symlinks. Three mixed scans preserve healthy Terrain
alongside malformed JSON, invalid UTF-8 or a non-Unicode filename. Scanner stdout
is decoded strictly as UTF-8; persisted JSON must equal the returned catalog.

Positive launches use generated audio and automatic Escape after 90 frames.
Windows uses RTX 3070 Ti Laptop/D3D12; Linux uses lavapipe in the existing Xorg
session. Real VLC PCM, Winamp UI, microphone, physical input/HDR and NVK window
support are separate checks. The independent NVK allocation finding remains.
The sanitizer instruments installer/path code, not system OpenSSL, Dawn or GPU
drivers. These bounded results do not prove indefinite stability or no leaks.
All four local SDK snapshots retain 128 indexed files and the same changed
validator/header hashes. No Mac access, release/tag replacement or final
installer rebuild was performed.

## Reproduction and retained evidence

Rebuild the production frontend/scanner and native runtime from updated source,
then stage them with native Terrain/shaders/shared libraries in a new suite:

```text
python tools/check_catalog_safety.py --source-suite <expendable-native-suite> --output-dir <fresh-output>
```

On Linux add `--software` and use the owner's active X session. Source and
output must be separate; existing output is refused. The current checker
accepts Windows/Linux only. Adapt its Mac bundle path under M06/M08 when the
owner restores availability. Final installer/security/source gates stay open.

The subsequent [Mac menu follow-up](VALIDATION-MAC-MENU.md) passes actual held
Open/Settings actions for malformed JSON/UTF-8, NUL/traversal/wrong API and
real assets/package symlink escapes, plus missing-module rescan/restoration.
It also fixes a reproduced malformed-UTF-8 Cocoa alert exception. This is a
separate Mac handler matrix, not a claim that this Windows/Linux checker was
ported unchanged.

Owner-local Addon build evidence:

- `catalog-nul-before-windows-v1/result.json`: pre-fix acceptance.
- `catalog-invalidname-before-windows-v1/result.json`: pre-fix filename abort.
- `catalog-safety-windows-v2/`: pre-fix UTF-8 diagnostic abort.
- `catalog-safety-windows-v4/summary.json`: final 46-case pass and hashes.
- `catalog-filesystem-regression-windows-v1/result.json`: 34 filesystem cases.
- `catalog-safety-linux-evidence-v1/`: copied Linux reports/logs.

On Mint under `~/vlc-visualizers-work/`, see `catalog-nul-before-linux-v1/`,
`catalog-safety-linux-v1/` (original failures), `catalog-safety-linux-v3/`
(final 46 cases) and `catalog-installer-regression-linux-v1/` (regular/sanitizer).

The first Windows positive fixture allowed too little time for the existing
90-frame closure self-test. A later harness read UTF-8 JSON using the Windows
default code page and failed its replacement-character comparison. Those
setup failures remain separate; the final harness allows normal closure and
reads catalogs/reports explicitly as UTF-8.
