# Native image import and palette publication

Measured 2026-11-16 on the owner's Windows, macbook1 and mint machines.
All inputs are locally generated fixtures; no image/audio/report is uploaded.

`ImagePaletteWorker` connects the verified native codecs and complete image
palette algorithm to Prism Flow. It copies an absolute filename and owns an
isolated seeded RNG. Decode, extraction and companion-color selection run
off the render thread. Only an owned palette and bounded error cross back;
the render thread alone edits live presets and GPU state. One scene RNG seed
is consumed at submission, so completion never races that stream. Image
filenames are not added to saved settings.

One worker retains at most one running job, one replaceable pending request
and one result. A newer request cancels/discards older work. Generation checks
reject an old completion even when its codec ignores stop. Cancellation is
cooperative around native codec calls and inside extraction; it cannot
forcibly interrupt every OS decoder call. Destruction stops and joins before
module unload. Bounds remain 64 MiB compressed, 32,768 source pixels per axis /
64 million source pixels, and a 128-pixel longest-side extraction image.

The scene publishes on a frame boundary. P cancels pending import before
randomizing its palette. Invalid/missing/corrupt/transparent images report a
recoverable status and keep rendering. The window frees SDL drop filenames,
retains at most 32 diagnostic status entries and one pending notice, and
shows normal-run error notices outside other dialogs' event pumps. Base-only
native plugins report import as unsupported and continue rendering.

## Verified results

Worker checks pass on Windows, macOS and Linux:

- 200 requests while the first decoder deliberately ignores cancellation:
  exactly two decodes occur, the held file and the newest pending file.
  Only token 200 is published.
- Replacing or cancelling an already ready result discards it. A bounded
  1,024-byte worker error is followed by successful recovery. Relative paths,
  zero tokens and submissions after close are rejected.
- Forty normal and eight interrupted worker lifetimes join without publishing
  after close. Actual native decode/extraction matches the independently
  called, already reference-validated helper chain.
- 120 real imports through one worker sample handles/RSS after warm-up:

| Native probe | Handles/FDs at 24 / 72 / 120 | Resident bytes at 24 / 72 / 120 |
| --- | --- | --- |
| Windows | 139 / 139 / 139 | 9,363,456 / 9,453,568 / 9,519,104 |
| Mac | 3 / 3 / 3 | 11,993,088 / 12,058,624 / 12,058,624 |
| Linux | 6 / 6 / 6 | 19,247,104 / 19,247,104 / 19,247,104 |

The control/module probe passes on D3D12, Metal and Linux NVK. It checks
borrowed UTF-8 copying, Unicode filenames, malformed UTF-8/NUL, unsupported
commands, missing/corrupt/transparent input, stale and matching cancellation,
P-key cancellation, 200 replacements and eight module destructions with work
pending. APPLIED requires a render-thread commit. Truncated/wrong-version/
missing-callback control tables are rejected. Unknown future capabilities are
not polled as image import. Existing Terrain API 4 modules
without the extension still load/render and reject import as unsupported.
C11/C++ layout checks keep the original base table at 88 bytes.

A reproduced error-path defect was fixed before commit: taking a raw
1,024-byte substring could split a Unicode scalar. A 1,023-byte ASCII prefix
followed by a four-byte emoji failed the regression before the fix. Worker
errors and module callback buffers now copy complete UTF-8 scalars within
their bounds and replace invalid codec bytes with `?`. Unicode-boundary,
invalid-byte and short caller-buffer guards are part of the worker probe.

Linux ASan/UBSan with leak detection passes worker and control/module checks
using SwiftShader. The default-quarantine run is retained and reports no
leak. Its RSS grows from 147,836,928 to 358,260,736 bytes over the sampled
imports. An additional diagnostic with a 16 MiB quarantine
also passes leak detection, with 72,245,248 / 73,752,576 / 74,588,160 resident
bytes. This demonstrates instrumentation-sensitive RSS; ordinary native
samples above remain separate. Neither sample proves indefinite stability.
The separately built Dawn/driver and all system codecs are not fully
instrumented. Unsuppressed NVK driver-allocation and cross-adapter failures
remain open; software-path results do not erase them.

## Native windows and scene regression

Each platform passes 90 frames of injected SDL drops: apply an image,
recover from a corrupt file, queue two replacements, apply only the newest.
Windows uses D3D12/Escape, Mac Metal/right click, and Linux lavapipe/menu
protocol because its NVK X session lacks DRI3. Mac/Linux use Unicode filenames
and start fullscreen; all resize and restore saved bounds, with at most two
frames in flight. Automated tests record errors without opening modal
notices. Physical OS drag/drop and the normal-run notice remain final UI QA.

Complete scenes remain exact after this change: 36 frames / 5,381,100
feedback/color components on each same-adapter Windows, Metal and Linux
SwiftShader path. Existing native file-resampling/color differences in
[VALIDATION-IMAGE-PALETTE.md](VALIDATION-IMAGE-PALETTE.md) and the Addon's
VALIDATION-IMAGES.md remain. Do not claim identical palettes from all OS file
decoders or that NVK matches SwiftShader.

Build through the Addon's Dawn CMake project. CTest sets up generated local
files for `dawn_prismflow_image_worker` and `dawn_prismflow_image_control`;
`dawn_native_plugin` checks unchanged base compatibility. The optional C
contract is in the Addon's `sdk/native/visualizer_dawn_control.h` and
`dawn/NATIVE-API.md`. The workbench's normal Prism Flow window accepts drops.
Its local check adds `--synthetic --self-test --exercise --drop-image-test
<fixture>/opaque.png`, with a `corrupt.png` sibling for recovery.

D33 implementation is now verified. Production dispatch/manifests, final SDK
snapshot synchronization, original-engine retention, soaks, installer/security
checks and release packaging remain separate work. No final installer is
rebuilt and no prior tag/artifact is replaced.
