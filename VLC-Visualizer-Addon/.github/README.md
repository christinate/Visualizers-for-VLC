# Build and release execution

The native candidate is built, tested and packaged on the owner's machines.
There is no automatic hosted build, diagnostic upload or release publication
on this branch. The previous workflow is preserved, unchanged, in
[`docs/legacy-ci/build.yml.txt`](../docs/legacy-ci/build.yml.txt); it is a
historical recipe, not an executable GitHub Actions workflow.

Use [`sdk/NATIVE-RUNTIME.md`](../sdk/NATIVE-RUNTIME.md) for the pinned Dawn/SDL
source builds and platform inputs. Configure with `VIS_BUILD_DAWN_RUNTIME=ON`
and explicit `Dawn_DIR`/`SDL2_DIR`, then build and run the platform checks on
the intended desktop. The old default CMake build alone does not build the
native runtime or qualify an installer.

Package with `tools/release.py` on Windows, `tools/release_native.py` on Linux
or `tools/release_native_macos.py` on an arm64 Mac. Supply the documented native
build/dependency inputs and fresh output directories. Package builders audit
their inputs; a built archive is still a candidate until the matching checks
in [`todo.md`](../todo.md) pass.

Commit the intended source and use `python tools/source_release.py` for its
immutable source archive and checksums. Keep corresponding dependency source,
notices and exact binary provenance with each independent release. Local
commands do not create a remote, tag, public release or cloud CI run. A future
CI configuration requires an explicit decision about execution and uploads.
