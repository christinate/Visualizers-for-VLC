# Catalog packaging validation — 2026-11-17

The old catalog packager successfully created a ZIP outside any Git repository
from copied worktree files. That archive omitted `tools/validate.py`, although
its included README directs recipients to run it. No source commit record was
written. The owner-local before-fix reproduction is retained under
`build/catalog-release-before-v1/`; its 31-entry ZIP confirms both omissions.
This is a catalog release defect, separate from renderer/installer security.

The packager now requires a clean source checkout, captures one commit and
reads its VERSION and selected content through actual Git archive operations.
It includes tools, release guides and changelog alongside the catalog, schemas
and examples. It extracts the delivered ZIP into a fresh validation directory
and runs that snapshot's validator. Successful output includes RELEASE.json
with the exact commit/archive hash, validation.log and SHA256SUMS.json.
Validation failure retains diagnostics without creating success metadata.
Existing outputs/staging are never replaced.

`python tools/test_catalog_release.py --output NEW-PATH` passes eight actual
isolated Git/archive cases on Windows Python 3.12:

- Clean commit, exact metadata/hashes and the delivered validator executed
  outside the source checkout.
- Dirty tracked source rejected before artifact/staging creation.
- Untracked source rejected.
- Ignored generated files under examples excluded from the delivered ZIP.
- Occupied output retained byte-for-byte, without creating staging.
- Occupied staging retained, without creating output.
- Invalid committed catalog rejected without success metadata.
- A real HEAD/VERSION change during validation cannot relabel the captured
  snapshot or its archive.

Local evidence: `build/catalog-release-check-windows-v1/result.json` and its
isolated fixture repositories/logs. Tests create source/JSON artifacts only;
no renderer, installer, recording or external analysis service is involved.
This bounded result does not complete the suite's remaining release gates.
