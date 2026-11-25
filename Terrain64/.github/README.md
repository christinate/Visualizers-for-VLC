# Build and release execution

Build, test and package the native candidate on the owner's machines. This
repository has no automatic hosted workflow or artifact upload.

Follow [`sdk/NATIVE-RUNTIME.md`](../sdk/NATIVE-RUNTIME.md) for pinned Dawn/SDL
preparation and native builds with `VIS_BUILD_DAWN_RUNTIME=ON`. Package with
`sdk/renderer-release.py` on Windows or `sdk/renderer-release-native.py` on
Linux/macOS, using the documented native inputs and fresh output directories.

Commit the intended source before `python tools/source_release.py`.
[`RELEASES.md`](../RELEASES.md) describes source/binary checksums, notices and
remaining acceptance gates. Keep artifacts local and unpublished until the
owner supplies repositories and authorizes publication. A future CI recipe
must explicitly decide where builds run and which outputs may be uploaded.
