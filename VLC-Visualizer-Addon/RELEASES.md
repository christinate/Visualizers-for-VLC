# VLC Visualizer Addon releases

The native candidate is **0.5.0-rc.1**. Download the full installer or select
an individual visualizer in the
[GitHub release list](https://github.com/christinate/Visualizers-for-VLC/releases).
The exact current state is in [todo.md](todo.md),
[RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) and
[release-status.json](release-status.json).

## Build and package

[README.md](README.md) and [sdk/NATIVE-RUNTIME.md](sdk/NATIVE-RUNTIME.md)
describe the pinned native toolchains and explicit input paths. The Windows
builder is `tools/release.py`; it creates a reversible VLC launcher patch with
one shared native Dawn runtime and plugin-only components for all seven
renderers. Components are selected by default and can be deselected. Build the
required renderer binaries first; the combined builder does not embed their
standalone installer executables. It needs NSIS and the matched VLC/Qt ABI.

Use `tools/release_native.py` for Linux x86_64 and
`tools/release_native_macos.py` for macOS arm64, with verified native build and
dependency inputs. Both use fresh staging/output paths. Mac archives preserve
the original signed VLC app and have local ad-hoc signatures, without Developer
ID signing/notarization. Fresh Mac drafts target 11.0; execution is tested only
on 26.6.2. Earlier retained Mac drafts still require 26.0.

Production payloads contain no Electron/Chromium/Node runtime. Independent
renderer installers are prepared by their respective repositories; each can
install its own standalone runtime or only its plugin into an existing Addon.

## Source archives and provenance

Commit the intended source before running `python tools/source_release.py`.
The helper requires a clean checkout and creates a fresh
`releases/<VERSION>/source/` directory. `--output NEW-PATH` selects another
fresh destination. The ZIP, RELEASE.json and SHA256SUMS.json bind to one exact
commit; existing output is refused. Keep the source archive, dependency pins,
local patches, notices and binary build provenance with the corresponding
binary release. A source ZIP alone does not qualify untested binaries.

Each build or packaging revision must use a fresh output location. Keep the
current source and binary package set, recorded historical hashes and Git
tags. Superseded local archives can be pruned. Check every entry in
that output directory's SHA256SUMS.json; do not use the old top-level release
checksum list as a claim about a different candidate. Signing or changing a
binary requires new checksums and validation of the resulting exact artifact.

## Release gates

The Addon repository's `todo.md` is the suite's canonical release checklist.
The current standalone and combined add-on packages for Windows x64,
Linux x64, and macOS arm64 are indexed in `build/coral-canyon/INSTALLERS.md`.
The Beach dolphin and UnderSea release reports remain in their original
build directories.
The [validation report](dawn/VALIDATION-SCENE-RELEASE-CHECKS.md) records the VLC
playback results, measured resource samples and package sizes. Earlier Winamp
and installer checks remain in their historical reports. Current qualification
follows the owner's VLC and packaging scope.

HDR is Experimental, opt-in and off by default. The owner has no HDR display
and accepted this status instead of physical HDR signoff. Tested SDR fallback
and float16 rendering do not establish physical HDR output.

The full Windows installer offers the optional DualSubs for VLC checkbox.
See the [DualSubs build and testing instructions](integration/qt/DUALSUBS.md)
for the pinned overlay version, merged Qt backend, and uninstall behavior.
Verify tags and retained legacy artifacts
with `python tools/check_preserved_releases.py --allow-pruned-archives`.
Application diagnostics and analysis stay on the owner's machines.
