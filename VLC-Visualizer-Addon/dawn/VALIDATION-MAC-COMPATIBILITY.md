# Mac deployment and dependency follow-up — 2026-11-17

Fresh arm64 dependencies, host, modules, production app and development
archives now target macOS 11.0. Every packaged Mach-O declares that minimum;
the signed app's LSMinimumSystemVersion matches it. Tests pass on the available
Apple M1 running macOS 26.6.2, SDK 27.2. Actual older macOS and Intel hardware
remain unqualified. A declared deployment floor is not an old-OS test result.
Final-source artifacts and release gates remain in [todo.md](../todo.md).

## Reproduced compatibility blocker and fix

Compiling the previous numeric parsers for macOS 11 fails: this installed
libc++ makes floating-point std::from_chars available only from macOS 26.
Both native settings and Prism Flow palettes used it. The shared
`include/decimal_parse.hpp` now uses explicitly C-locale strtod_l on Mac with
strict finite full-token decimal/scientific grammar. Windows/Linux retain
their existing from_chars implementation.

Settings tests pass for decimals, exponents, representable subnormals, signed
zero, rejected hexadecimal/nonfinite/partial/overflow/total-underflow inputs,
and independence from a French process numeric locale. Existing settings,
unknown-field preservation, INI migration and 80 concurrent atomic saves also
pass. Both former failing targets now compile for 11.0. The first failed
compiles remain retained; no older deployment failure is silently relabeled.

## Fresh dependency builds

Dawn is rebuilt from pinned revision
`7daf3b0bee2c73621001ab15b256b5a84fb0b45f`, shared/Metal-only, Node bindings
OFF, with `tools/build_dawn.py --macos-deployment-target 11.0`. Its raw dylib
SHA-256 is `4dedbd7363386e8f790dfa43bfd7d8f22368dd705a3217f619541120dad1e533`.
Old SDKs remain unchanged.

The new `tools/build_macos_sdl.py` verifies the locked SDL 2.32.10 source
archive, the complete extracted source tree and contained relative aliases,
then records actual CMake options, architecture, Mach-O deployment version
and artifact hash. Both verification of an existing canonical build and a
fresh normal tool-driven build pass. The two raw SDL libraries are identical:
1,689,456 bytes, SHA-256
`b174c0a9a83b522f6d8891a721ada47e2b169d4f28580cf450eb5ef5c6226b8b`.
The packager requires and includes this verified provenance.

The retained old SDL archive did not match the locked archive SHA. It was
preserved and not accepted as a canonical input. Comparison of the archives
finds 1,747 common regular files with zero content differences and different
extra-file inventories. This does not establish corrupt or malicious code.
The canonical archive includes two legitimate relative Android aliases;
the extraction tool validates and preserves them.

## Requalification with the new dependencies

| Check | Result |
| --- | --- |
| Production and original-engine builds | Host/controller/launcher/app/VLC module, both Dawn modules, Classic and projectM pass |
| Settings/editor | Numeric/locale/concurrent persistence and native UI probe pass |
| Presentation | 29 policy/50 GPU cases and actual Metal surface/EDR transitions pass |
| Calibration | 56 cases, 6,496 channels; corrected endpoints exact |
| Original-JavaScript presentation comparison | 24 cases; all 3,710,976 components and 24,576 packed palette bytes exact |
| Original-JavaScript scene comparison | 36 frames; all 5,381,100 components exact |
| Real VLC menu/media | 60 cases, including 40 format/engine combinations; pass |
| Archive installation | 18 ownership/selection/refusal/restoration cases; pass |
| Production presentation | Four HDR-off/on checks; real SDR/float16 fallback surfaces; pass |
| Production windows | Five resize/fullscreen/closure/injected-device-loss cases; pass |
| Production dispatch | Ten modern/original/settings/stale-cache/failure cases; pass |
| Invalid-UTF-8 profiles | Two bounded failure-report/source-preservation cases; pass |
| Production package audit | 12 arm64 binaries; relative dependencies, one Dawn library, strict local signatures, no browser runtime; pass |

The JS comparisons use retained original-JavaScript captures on the same M1
Metal adapter and pinned Dawn source, made with the earlier dependency build.
The new native binaries match them exactly. This is explicit regression scope,
not a claim that both runs load the same dylib bytes. HDR stays Experimental,
off by default; real display headroom is 1.0. No physical HDR claim is made.

## Preserved development archives

These are new immutable drafts under `macos11-runtime-v1/native-archives-v1`
in `~/vlc-visualizers-work/mac-return-20261117-v1/` on macbook1:

| Archive | Bytes | SHA-256 |
| --- | ---: | --- |
| Combined Addon | 5,523,411 | `8453fcac5081abb513e5987c1de401eae04d764998beb99ea4130f347bdc4ef5` |
| Prism Flow | 4,821,527 | `000adc964a67dff72f0c5cb555168007afc0dc38dac7539ee2c3af0640e3e7db` |
| MilkDrop/projectM | 4,901,566 | `f2b0b9c8c500a52caaea991d4135ca83edd6c2a098cd30e7fcb31617d3536bdb` |
| Terrain | 4,515,484 | `48e1c0d69f41cb36ed20a9790ee01494b69715263e22d3bab8a759b68cffa70d` |

Earlier macOS-26 drafts, hashes and reports remain unchanged. Current archives
have only local ad-hoc signatures, without Developer ID signing/notarization.
Independent per-repository builds now also pass with the new dependencies,
as recorded below. Final-source release reconciliation remains a gate.
No tags/remotes/releases were published.

The 26-file evidence copy and source commits are listed in
[Mac menu validation](VALIDATION-MAC-MENU.md). Dependencies are under
`macos11-dependencies-v1/`; runtime reports and full build logs are under
`macos11-runtime-v1/`. The fresh SDL normal-build log, both provenance files,
archive comparison and prior numeric compilation failures remain preserved.
About 1.09 GB is free after these checks; further builds need a space preflight.

## Independent builds and installed renderer follow-up

All three repositories build from their own pinned 133-file SDK snapshots
with the new 11.0-target Dawn/SDL dependencies. Each source checkout remains
clean at its recorded commit before and after compilation/packaging. Each
archive passes arm64, minimum-OS, relative-library and strict local signature
audits; no sibling renderer checkout supplies its runtime/module sources.

| Project | Source commit | Archive bytes | SHA-256 |
| --- | --- | ---: | --- |
| Prism Flow/Geiss | `bf5afa39eda8deac0bac022156b2244e0bf77090` | 4,821,518 | `99541d7671d8d2f5d040136f5fbe0e41551369aef83e100e6b7f2029c971cd15` |
| MilkDrop/projectM | `91be57965824500e46b6e5612fc7f136d03bf7d0` | 4,901,499 | `1bb0df06e6a48ab7aeff7d721f6c95ee509ff96feb76d24611f14438d3977a1e` |
| Terrain | `27536df04df62eab32d1d2b68af4ed752b43504f` | 4,515,510 | `275d6b61da5b554b0ba16f144753d9509efd5a8e88ce4a890ab30589e59f338f` |

All 18 install/ownership/restoration cases pass again using these independent
archives and the retained combined draft. Actual standalone installation,
generated rendering and uninstall also pass: Prism Flow/Terrain present
90 frames each; Classic/projectM present 194/188 frames and pass the original
host's fullscreen/borderless and key round-trip fixtures. These are generated
input checks, not physical recording or additional real-player UI passes.

Mac originals are under `macos11-runtime-v1/independent-v1/`, including full
build logs, dependency audits, source commits, archive checksums and installed
renderer reports. A 16-file diagnostic copy is verified locally under
`build/mac-return-evidence-20261117-v1/evidence-summary-v4/`; archive SHA-256:
`941887eef31abb77e5b6651c70e8c6df75d52069ad39cfc2e2f0d60499fa81a5`.

After checking no test processes remained, cleanup removed only this run's
four byte-identical archive extractions, verified duplicate archives and three
uninstalled disposable prefixes. Original archives/builds/SDKs, neighboring
profiles and all reports remain. Free space rises from 774,078,464 to
909,873,152 bytes. The separate cleanup report SHA-256 is
`dbf284e100e200c609856e521c5a99c3ec34474fa4d9c2bc0418758d153c0156`.
Further large builds still need a space preflight; Linux remains at 140 MB free.
