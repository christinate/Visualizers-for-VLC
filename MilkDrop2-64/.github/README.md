# Build and release execution

Build, test and package this native candidate on the owner's machines.
No hosted workflow or artifact upload runs automatically on this branch.
The previous workflow is preserved unchanged in
[`docs/legacy-ci/build.yml.txt`](../docs/legacy-ci/build.yml.txt). It lacks the
native candidate's pinned Dawn/SDL preparation and is historical only.

Follow [`sdk/NATIVE-RUNTIME.md`](../sdk/NATIVE-RUNTIME.md) to prepare the pinned
dependencies and configure with `VIS_BUILD_DAWN_RUNTIME=ON`. Use
`sdk/renderer-release.py` for Windows or `sdk/renderer-release-native.py` for
Linux/macOS, with the documented explicit native inputs and fresh outputs.
Verify the original Windows MilkDrop compiler dependencies and both bridge
architectures separately from projectM on macOS/Linux.

Commit the intended source before `python tools/source_release.py`.
[`RELEASES.md`](../RELEASES.md) describes source/binary checksums, notices and
remaining acceptance gates. Keep release artifacts local and unpublished
until the owner supplies repositories and authorizes publication. Future CI
must explicitly decide where builds run and which outputs may be uploaded.
