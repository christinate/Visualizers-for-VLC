# Build and release execution

Validate and package this catalog on the owner's machines. This branch has
no automatic hosted workflow or artifact upload. The former workflow is
preserved unchanged in
[`docs/legacy-ci/build.yml.txt`](../docs/legacy-ci/build.yml.txt).

From a clean, committed checkout run `python tools/validate.py`, then
`python tools/release.py` for a new candidate catalog archive. Use
`python tools/source_release.py` for its corresponding immutable source ZIP.
Both release helpers refuse existing output; retain earlier releases and
their checksums. See [`RELEASES.md`](../RELEASES.md) for provenance and remaining
release requirements.

Keep artifacts and analysis local and unpublished until the owner provides
repositories and authorizes publication. A future CI configuration must
explicitly decide execution and upload behavior.
