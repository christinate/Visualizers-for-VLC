# Prism Flow and Geiss Classic releases

Current native development candidate: **0.5.0-rc.1**, on
`feature/dawn-native`. Validation is incomplete; no final release is published.
This repository owns its source history, source archive and binary artifacts.

## Build and package

Use the commands and dependency pins in [README.md](README.md) and
[sdk/NATIVE-RUNTIME.md](sdk/NATIVE-RUNTIME.md). Windows packaging uses
`sdk/renderer-release.py`, NSIS and the source-built shared Dawn/verified SDL
SDKs. Independent installers provide standalone, VLC and Winamp routes. A
VLC-only selection adds the plugin to an existing native Addon suite rather
than installing another shared runtime there. Real Winamp UI qualification
remains separate from synthetic installer/bridge-layout tests.

Linux x86_64 and macOS arm64 archives use
`sdk/renderer-release-native.py` with explicit native host/module/dependency
inputs and fresh staging/output paths. They provide `install.sh` and
`uninstall.sh` with schema-2 ownership and verified restoration. Mac staging
relocates bundled dependencies and uses local ad-hoc signatures; the archives
are not Developer ID signed or notarized. Fresh Mac drafts target 11.0, but
execution is tested only on 26.6.2; Intel and older-OS qualification remain open.

Production payloads use native C++/Dawn without Electron, Chromium or Node.
Preserved JavaScript reference files and Node-based comparison tooling are
for development. Use the native packagers for this candidate.

## Source archives and provenance

Commit the intended source before running `python tools/source_release.py`.
The helper requires a clean checkout and creates a fresh
`releases/<VERSION>/source/` directory. `--output NEW-PATH` selects another
fresh destination. The ZIP, RELEASE.json and SHA256SUMS.json bind to one exact
commit; existing output is refused. Keep the source archive, dependency pins,
local patches, notices and binary build provenance with the corresponding
binary release. A source ZIP alone does not qualify untested binaries.

Each build or packaging revision must use a fresh output location. Keep the
current packages, Git history, tags and recorded historical hashes. Check every entry in
that output directory's SHA256SUMS.json; do not use the old top-level release
checksum list as a claim about a different candidate. Signing or changing a
binary requires new checksums and validation of the resulting exact artifact.

## Release gates

The Addon repository's `todo.md` is the suite's canonical release checklist.
Final-source builds and remaining player/installer/stability tests remain
required. All four engines pass bounded real Winamp 5.9.2 x86 playback with
decoder PCM and a private silent output. Physical sound output, real x64
Winamp UI and production Mac recording approval/capture remain unqualified.
Further Defender investigation is deferred at the owner's request; preserve
the historical results. Bounded passes do not establish indefinite leak
freedom or the cause of historical system crashes.

HDR is Experimental, opt-in and off by default. The owner has no HDR display
and accepted this status instead of physical HDR signoff. Tested SDR fallback
and float16 rendering do not establish physical HDR output.

No repository remote, release URL or publication is invented. The owner will
supply the Git repositories. Candidate artifacts remain local and unpublished.
Previous tags/artifacts remain unchanged; their old browser build instructions
remain in those tagged revisions. Application diagnostics and analysis stay
on the owner's machines.
