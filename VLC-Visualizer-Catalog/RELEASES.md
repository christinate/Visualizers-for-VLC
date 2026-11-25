# VLC Visualizer Catalog releases

Current native development candidate: **0.5.0-rc.1**, on
`feature/dawn-native`. The catalog owns its JSON compatibility list, schemas,
installer examples, documentation and independent Git/source artifacts.
Final native releases and public repository URLs are pending.

## Catalog artifact

Commit a clean source checkout, then run `python tools/validate.py` and
`python tools/release.py`. The release helper writes a fresh
`releases/<VERSION>/catalog/` ZIP, RELEASE.json, validation.log and checksums,
all bound to one captured Git commit. It runs the validator delivered in that
ZIP. Existing staging/output is refused; `--output NEW-PATH --stage NEW-PATH`
selects separate fresh locations for additional drafts. It packages tools,
documentation, catalog, schemas and examples; it is not an installer or a
renderer runtime. See [packaging validation](VALIDATION-RELEASE.md). API 4
native Dawn package guidance is in [DAWN.md](DAWN.md). Preserved APIs and
browser-reference guidance are explicitly versioned in the catalog.

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
Final-source builds, full player/installer/stability tests and the exact final
Windows installer's local-only Defender checks remain required. Real Winamp UI
and production Mac recording approval/capture are incomplete. Bounded passes
are not evidence of indefinite leak freedom or the cause of historical system
crashes. The 0.2.0-rc.1 Addon Defender detection remains unresolved; no
false-positive determination or cloud clearance is claimed.

HDR is Experimental, opt-in and off by default. The owner has no HDR display
and accepted this status instead of physical HDR signoff. Tested SDR fallback
and float16 rendering do not establish physical HDR output.

No repository remote, release URL or publication is invented. The owner will
supply the Git repositories. Candidate artifacts remain local and unpublished.
Previous tags/artifacts remain unchanged; their old browser build instructions
remain in those tagged revisions. Application diagnostics and analysis stay
on the owner's machines.
