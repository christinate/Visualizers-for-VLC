# Catalog release artifacts — 2026-11-17

Project 4 now has an actual local catalog package and corresponding full
source archive from clean commit
`e1fcc6d2729b7352d9054326d7113f7eafbf8527`. Both are byte-identical when
prepared independently on Windows, Mac and Linux. They remain unpublished
development candidates; they do not certify the renderer/installer releases.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Catalog ZIP | 44,633 | `1ead3a4b07aaddc4e2ea9dbcd47df54ceba9ac6f24d598fa5a23592945edb56a` |
| Source ZIP | 48,908 | `b7172f56f7d191fa10cc9e313dda7888964125ff806ccdf7025d9241ff637ff3` |

The Windows artifacts are in the Catalog repository's
`releases/0.5.0-rc.1/{catalog,source}/`. Each has exact-commit RELEASE.json and
verified candidate-only checksums. The catalog also has validation.log from
running the validator shipped inside that exact ZIP. These do not replace or
refresh any earlier release metadata/checksum list.
An additional direct Git-object audit verifies all 33 catalog files and all
36 full-source files byte-for-byte against that commit, plus both ZIP commit
comments. Identical bytes on Mac/Linux give the same archived content there.

## Reproduced release problem and correction

The previous catalog packager copied live worktree files, succeeded without
Git, omitted the README's documented tools/validate.py, and wrote no source
commit metadata. Its 31-entry before-fix archive is preserved in
`VLC-Visualizer-Catalog/build/catalog-release-before-v1/`.

The corrected tool snapshots one clean Git commit, includes the documented
tools and release guides, runs the delivered snapshot's validator, and records
its source commit, version, archive hash and validation status. Dirty tracked
or untracked source is rejected before output creation. Ignored local files
never enter the Git archive. Failed validation leaves diagnostics without a
success record; occupied staging/output is preserved.

All eight actual isolated Git/archive cases pass on each platform: clean
package/checksums/delivered command, dirty tracked source, untracked source,
ignored example output, occupied output, occupied staging, invalid committed
catalog and a real HEAD/VERSION change during validation. The last case proves
the captured snapshot cannot be relabeled by a concurrent commit. The current
25-case schema/example validator also passes against every real packaged
snapshot. Only Python/source/JSON tools run; no recording, application renderer
or installer is executed.

The Mac uses Python 3.14 on macOS 26.6.2; Windows uses Python 3.12. Mint uses
Python 3.12.3 and the previously verified private Git 2.43.0 installation.
Its bare SSH PATH initially lacked Git; adding the private tool directory and
GIT_EXEC_PATH for this process resolves that setup failure without changing
the system installation. An initial disk preflight also refused before work
started. Free Linux space later recovered to about 520 MB; the source-only
checks fit, leaving about 513 MB. GPU/build runs remain subject to their own
larger space preflight. Neither setup failure is a product test failure.

## Owner-local evidence

- Catalog: `build/catalog-release-check-windows-v1/` and
  `build/catalog-candidate-0.5-v1.json`,
  `build/catalog-candidate-blob-audit-v1.json`.
- Mac, under `~/vlc-visualizers-work/mac-return-20261117-v1/`:
  `catalog-release-check-mac-v1/`, `catalog-release-artifact-mac-v1/`,
  `catalog-source-artifact-mac-v1/`, `catalog-release-summary-mac-v1.json`.
- Mint: `~/vlc-visualizers-work/catalog-release-return-20261117-v1/` contains
  a fresh exact-commit source clone, tests, logs and both verified archives.
- Verified copies in the Addon:
  `build/catalog-release-cross-platform-v1/`. The Mac summary SHA-256 is
  `615e27ae8fc147c2dbd4a42a0aedcaf092bd1f36f1f97374c2d1ca1297909cea`;
  Linux summary SHA-256 is
  `901fd0f8b07726f477dfd8cf279237e03d42a5d374c9b73514e12af588d1874a`.

No external analysis upload, remote, tag or publication was created. The
catalog continues to distinguish preserved 0.4 artifacts from native 0.5
development capabilities; it does not advertise final native installers.
The remaining full-suite gates are in [todo.md](../todo.md).
