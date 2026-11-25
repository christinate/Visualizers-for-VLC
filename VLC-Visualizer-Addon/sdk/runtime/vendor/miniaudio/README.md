# miniaudio source input

Version 0.11.26, upstream revision
`877ea4a7c2601c7b0f2a44e83dfac42e6470fea6`. The original dual license is
preserved in LICENSE and the header. SOURCE.json records exact file hashes,
the original upstream header hash and the local patch.

`patches/0001-pulse-wake-before-uninit.patch` fixes a reproduced Linux
PulseAudio shutdown hang after capture input removal. The generic device
uninitializer changed state and signalled its worker event, but that event
could not wake a worker blocked inside PulseAudio's own polling loop. The
patch calls the existing PulseAudio data-loop wakeup before joining that
worker, after setting the uninitialized state. It does not force-free a live
worker or use a timeout in place of resource cleanup.

To reproduce the patched header, obtain miniaudio.h at the exact upstream
revision, verify its upstream SHA256 in SOURCE.json, and apply the patch with
`git apply patches/0001-pulse-wake-before-uninit.patch` from a directory
containing that header. Verify the resulting header SHA256 against its files
entry. Native CMake configure checks the committed header, shim, license and
patch hashes before building.

The patch was verified against real virtual-input removal/reconnection,
normal capture, the native visualizer's input switching/Escape closure and
address/undefined-behavior/leak checks. Raw diagnostics remain in ignored
local build folders. This local change has not been submitted upstream.
