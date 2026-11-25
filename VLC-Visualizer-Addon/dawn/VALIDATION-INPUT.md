# Native recording and input selection

Development evidence observed 2026-11-15, on the native migration branch.
Production dispatch/settings/installers are still separate checklist work.
All analysis and test reports remain on the owner's machines; no recorded
audio is saved or submitted elsewhere.

## Native recording

Pinned miniaudio 0.11.26 supplies float32 stereo at 44.1 kHz to the native
analyzer. Audio callbacks only write a bounded single-producer/single-consumer
FIFO. Device teardown joins capture before releasing that FIFO or context.

Two-second recording checks passed on Windows WASAPI, Linux PulseAudio and
Mac CoreAudio. After the owner approved the helper's normal OS microphone
request, the Mac test delivered 87,552 frames without FIFO drops from a
48 kHz mono input, converted to float32 stereo at 44.1 kHz. This is actual
native recording evidence, separate from GPU or generated PCM fixtures.

macOS bundles include a microphone usage description. Authorization requests
are asynchronous; the visualizer renders silence and remains closable while
waiting. Denied/restricted/missing-description states produce explicit errors.
Enumeration does not request microphone access. The helper and visualizer
window are separate development app identities, requiring separate approvals.
Ad hoc signatures are tied to build contents; rebuilding can require approval
again. No privacy setting is reset or bypassed by the tests.

## Device identity and native dialogs

Input IDs use backend-specific stable endpoint UIDs rather than enumeration
positions. CLI selection accepts `--line-in default`, an exact ID or an
exact label (ASCII case-insensitive, plus exact UTF-8 labels). IDs take
precedence; duplicate labels and missing/invalid selections are rejected.
CLI selections are resolved before requesting microphone permission.

The standalone workbench opens a picker unless an explicit `--line-in`,
`--synthetic` or automated test mode is selected. Ctrl-L and Command-L reopen
the picker. Controls are Win32 on Windows, Cocoa on Mac and the existing Qt 6
system dependency on Linux. Rows show names and IDs. Cancelling retains the
current source. A replacement opens successfully before the old input stops.
The host polls its nonce-protected menu close request while the picker is open.

Five app-owned dialog fixtures pass on all three systems: default choice,
retained ID, duplicate/Unicode names, cancellation and owner-close cancellation.
The new Linux wrapper initially crashed at process exit inside
`QApplication::~QApplication`; a local GDB backtrace identified destruction
during global exit cleanup. `NativeUiSession` now owns the Qt app inside main
and releases it before global cleanup. The same fixtures then exit normally,
including repeated dialog creation.

`tools/check_dawn_input.py` runs those fixtures followed by real recording
with the startup picker and four input activations through Ctrl-L/Command-L.
Windowed and startup-fullscreen cases pass on Windows D3D12, Mac Metal and
Linux software Vulkan. They retain at most two submitted frames, record after the
final replacement, restore fullscreen state, close via Escape and remove
their scoped control records. After the owner separately approved the Mac
visualizer app, both integrated Mac cases pass: four input activations, four
picker operations, 59,392/57,856 frames delivered by the final stream and no
FIFO drops. An earlier pending-permission run stayed responsive for its
180-second limit without capturing. Development rebuilds can require renewed
OS permission because the ad hoc signing identity is tied to build contents.

## Format and memory checks

`dawn_capture_formats` passes 200 streaming tone/silence cases on all three
systems: unsigned 8-bit, signed 16/24/32-bit and float32 PCM; 1, 2, 6 and 8
channels; and 22.05, 44.1, 48, 96 and 192 kHz. Tests check duration, finite
output, left/right tone identity, mono duplication and silence bounds while
converting 127-frame chunks to 44.1 kHz stereo. Unsigned 8-bit silence allows
its quantization step; signed PCM and float silence stay exactly zero.
Physical hardware was not available for every converter-fixture format.

FIFO/device-selector tests preserve nonquantized float samples, sanitize
nonfinite/extreme input, retain finite above-full-scale values, enforce the
16,384-frame capacity, silence stale input and check stereo integrity across
1,280,000 concurrently produced frames. Device fixtures cover stable IDs,
duplicate names, exact Unicode names and invalid selections.

Linux address/undefined-behavior sanitizers with leak detection pass the FIFO,
selector and all 200 conversion cases. This does not instrument the complete
GPU driver or establish absence of all leaks.

## Reproduced device-removal hang and correction

`tools/check_capture_removal.py` creates a uniquely named silent monitor on
the local PipeWire/PulseAudio server. It checks explicit-ID/exact-label
capture, removes that input during capture, rejects its now-missing ID, then
recreates it and captures with the same ID. It removes only the identified
test module and restores test-owned default changes. Cleanup is verified.

The original miniaudio 0.11.26 uninitializer hung in this test. A local GDB
trace showed the main thread joining the capture worker while that worker
remained in `pa_mainloop_iterate()`/`ppoll()`. Changing device state and
signalling the generic worker event could not wake PulseAudio's separate
poll. The recorded vendor patch calls its existing data-loop wakeup after
changing state and before joining. Original/patched source hashes and the
patch are recorded in vendor/miniaudio/SOURCE.json.

Normal and ASan/UBSan/leak-detection removal/reconnection checks now pass.
Optional window cases also pass: five actual input activations change from
the explicit virtual monitor to the original system default through four
picker operations, and a separate window closes via Escape after input
removal. Control registrations are released. No temporary module remains;
original source/sink defaults are unchanged. These removal checks cover the
available Linux virtual device, not every physical OS/device pair.

Longer reselection/lifecycle soaks, player ownership and
final release payload checks remain open.
