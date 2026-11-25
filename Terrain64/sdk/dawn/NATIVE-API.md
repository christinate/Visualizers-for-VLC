# Native renderer API 4

This migration interface is defined by `sdk/native/visualizer_dawn_api.h`.
It is exercised by the native workbench and production dispatcher using
schema 4 manifests. Installers and the public compatibility catalog are still
migration work. Existing APIs 1–3 remain separate interfaces. See
[dispatch validation](VALIDATION-DISPATCH.md).

Export the C symbol `VisualizerGetNativeApi(uint32_t)` and return a pointer
to a static `VisNativeApi` for version 4, or NULL for an unsupported version.
Populate structure sizes, the API version, package ID, display name, pinned
Dawn revision and every required callback. Strings and error messages are
UTF-8. Return zero for successful operations, nonzero for errors; `create`
returns NULL on failure. Error text belongs in the caller's bounded buffer,
including its terminating NUL. Catch exceptions inside the module.

## Runtime and ownership

Use the pinned WebGPU C header and the suite's one shared Dawn implementation.
Do not statically link a private Dawn copy into a plugin. The loader checks
the revision and compares the canonical `wgpuCreateInstance` procedure
returned by each module's `wgpuGetProcAddress`; Windows import-thunk addresses
themselves are not an implementation identity check.

GPU handles passed to `create` are borrowed. Retain them with WebGPU
AddRef/Release if needed, and never destroy the host device or replace its
callbacks. Allocate and free renderer state in the plugin. No STL objects,
C++ ownership, exceptions or allocator responsibilities cross the interface.
The host calls all entry points on one render thread. Plugin workers must
finish and join before `destroy` returns.

The host keeps module references until both renderer destruction and GPU
teardown. Register every plugin-created asynchronous WebGPU future with
`track_future`; it returns zero on success. Use `WaitAnyOnly` callbacks,
forget futures after completion, and finish a newly created future before
returning if the bounded registry is full. These services remain necessary
when initialization fails before it can return renderer state. Other callback
modes are reserved to the host. The host destroys its device and delivers
tracked completion/cancellation callbacks before releasing module code.
If an unresponsive future cannot be cancelled, the module remains pinned
until its isolated renderer process exits; unloading a live callback is unsafe.

## Frames and images

Each frame borrows 2,048 mono float waveform samples and 1,024 float spectrum
bins from analysis of 44.1 kHz stereo PCM. Copy input if a worker needs it
after `render` returns. Beat counts are 64-bit. Timings, RMS, energy, bass
and beat envelope are explicit fields.

Return an RGBA16F texture from the shared device, with matching dimensions
and an explicit SDR-encoded or linear-output label. The texture is borrowed
until resize/destroy. The host AddRefs any texture it retains for presentation
and Releases it later. Linear output does not establish display HDR support;
HDR presentation remains a separate, Experimental host feature.

Packages that implement `VIS_NATIVE_LINEAR_OUTPUT` declare
`settings.hdr_output: "native-float"` in their API 4 manifest. Linear RGB uses
203-nit reference white; it may exceed 1.0. The production host requests that
encoding only after checking the actual display and surface. Missing/other
values keep a package in SDR, including the SDR-only third-party example.
This declaration is a renderer capability, not proof of HDR display support.
The host verifies each returned image's encoding against the creation flags.

`Terrain64/native/plugin_dawn.cpp` is the working example. Build it through
the native CMake workbench. `dawn_window --plugin <module> --terrain-shader
<terrain.wgsl>` loads it; adding `--line-in default` uses native recording
input without the startup selector. Add `--synthetic` for generated audio.
Native picker/keyboard and real-capture checks on all three systems are
recorded in VALIDATION-INPUT.md; production dispatch is still checklist work.

## Validation

The C11/C++ layout test checks the 64-bit structure sizes: GPU 72 bytes,
create info 112, frame 96, image 32 and API table 88. Runtime tests reject
truncated tables, wrong revisions, a foreign runtime and missing exports;
exercise malformed settings, resize and bounded error buffers; compare
module output against the direct scene; and retain a usable host device
after plugin destruction. A C fixture queues a tracked buffer-map callback
then fails initialization, verifying callback delivery before module release.
These tests pass on Windows D3D12, Mac Metal and Linux Vulkan. They are not
a sandbox for malicious native code or proof of indefinite stability.

## Optional image controls, version 1

`sdk/native/visualizer_dawn_control.h` is a separate optional extension.
Export `VisualizerGetNativeControlApi(uint32_t)`, returning a static
`VisNativeControlApi` for version 1 and NULL otherwise. A module without this
export remains compatible through the unchanged 88-byte API 4 table. The
loader validates an extension's size/version/callbacks before use.

The table supplies capability bits, `submit` and nonblocking `poll`. Hosts
ignore unknown capability bits and do not poll them as image import.
Image-capable tables handle these commands:

| Command | Payload/token | Behavior |
| --- | --- | --- |
| IMPORT_IMAGE | Absolute filename, 1..32768 UTF-8 bytes, nonzero opaque token | Copy before returning; validate UTF-8/NUL and flags. Supersede earlier requests. |
| CANCEL_IMAGE | No payload; token 0 or a matching current token | Cancel current work; leave a newer nonmatching request unchanged. |

Callbacks run on the normal render thread. Submit returns 0 when accepted,
nonzero with bounded error text for invalid input. Offload codecs/extraction
to a bounded worker; do not retain the caller's UTF-8 pointer or touch live
GPU/preset objects from it. Invalidate publication on cancellation even when
a codec cannot stop immediately. Join before destroy returns.

Poll writes the latest type/token and state: IDLE, PENDING, APPLIED, FAILED
or CANCELLED. APPLIED means render-thread commit. A normal poll returns 0
even for FAILED, with the recoverable failure in the caller's error buffer;
the host keeps rendering. A nonzero poll return is a protocol/callback error.
All status text uses caller-owned bounded storage. No STL ownership crosses
the interface. C11/C++ tests check command 32 bytes, status 24 bytes and the
optional table 32 bytes, plus rejection of incompatible tables.

Prism Flow implements image controls; the SDL file-drop handler uses them
when available. Terrain remains base-only. The image worker/control probes
check input copies, failure recovery, cancellation, newest-result publication
and pending-work destruction. See the renderer's VALIDATION-IMAGE-IMPORT.md
for actual platform coverage. Native code remains trusted code, not sandboxed
by this interface. Production manifests and SDK snapshot sync remain later
checklist items.
