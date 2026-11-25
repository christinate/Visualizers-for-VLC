# Native host bounds and resource lifecycle

Development evidence observed 2026-11-15. This is the Dawn foundation/Terrain
host milestone D29. Repeat the resource checks with Prism Flow after its port;
extended soaks, full driver memory measurements and final installer validation
remain D60–D65. All analysis remains on the owner's machines.

## Enforced bounds

- Capture FIFO: 16,384 stereo float frames; analysis window: 2,048 frames.
  Overflow drops incoming frames, and stale input becomes silence.
- Terrain history: a fixed 256x128 RGBA16F texture, one 256-pixel upload row
  and fixed controller/analysis arrays. No growing frame history.
- Presentation: at most two pending submissions; host asynchronous registry:
  128 futures; retained native modules: 32. Registrations reject overflow.
- Internal render targets: at most 16,777,216 pixels and device dimension
  limits. Native output metadata is checked against its actual texture.
- Recording enumeration: at most 1,024 inputs. Settings: at most 1 MiB,
  32 nesting levels; plugin settings and shader files have separate limits.
- GPU error history: 32 messages, at most 4,096 bytes each. Driver string
  copying is bounded before allocation, keeping a complete UTF-8 prefix.
  Shader diagnostics retain at most 32 such errors. The native workbench
  writes a final report by replacement, and logs the blocked player-input
  shortcut warning once per window. Production legacy logging is still part
  of later dispatch/removal work.

These limits bound the host's histories and queues. They are not a promise of
an identical total process/driver footprint on every backend.

## GPU lifecycle evidence

`resource_probe` runs 40 API 4 renderer cycles, each with partial-initialization
failure, creation, three render/readback steps, resize and destruction. The
first eight cycles warm backend caches; the next 32 must not increase live
GPU texture/buffer bytes. Texture usage must return to the original baseline
after every cycle. It also fills the real 128-future registry, rejects the
129th registration, and delivers all 129 callbacks before freeing state.

The check uses the pinned Dawn build's estimated live texture/buffer accounting.
A deliberately retained 64x64 RGBA16F texture increases it by 32,768 bytes and
releasing that texture removes the increase. This positive control verifies
that the measurement detects retained application resources.

Only the test executable uses Dawn Native's POD-returning accounting and
maintenance functions. No STL ownership crosses that DLL boundary. Production
plugins continue using the C WebGPU/API 4 interface. Estimates are not total
physical GPU memory; driver heaps and caches need the later soak measurements.

| Backend | Live bytes after warmup / cycle 40 | Handles or FDs at cycles 8 / 24 / 40 |
| --- | ---: | --- |
| Windows D3D12, RTX 3070 Ti Laptop | 4,194,304 / 4,194,304 | 427 / 427 / 427 |
| macOS Metal, Apple M1 | 0 / 0 | 7 / 7 / 7 |
| Linux NVK Vulkan, GTX 1660 Ti | 22 / 22 | 8 / 8 / 8 |
| Linux lavapipe Vulkan | 22 / 22 | 5 / 5 / 5 |

Backend baseline bytes are retained Dawn resources, not additional renderer
bytes. The initial Windows buffer baseline shrank from 8 MiB to 4 MiB after
submission. The diagnostic therefore compares warmed cycles and independently
checks texture usage. Vulkan trimming can submit deferred deletion work;
the checker advances completed queue serials while polling that trim. Merely
polling the trim flag falsely timed out, which was a checker defect.

The existing failed-initialization callback fixture separately verifies that
a pending map callback completes/cancels before its module unloads. Device-loss
and resize/fullscreen/window-closure checks also remain in the regression set.

## Capture and failed-window cleanup

`capture_lifetime_probe` creates/enumerates/destroys 40 native audio contexts,
rejecting a missing input in each. All three systems pass. Optional explicit
`--record-input default` also starts, receives PCM, stops and destroys the
recording stream on each cycle. This recording variant passes on Windows and
Linux, including Linux ASan/UBSan with leak detection.

Windows recording handle counts stay 145 / 145 / 145; Linux recording FD counts
stay 4 / 4 / 4 at cycles 8 / 24 / 40. Mac context-only FD counts stay 3 / 3 / 3.
Mac physical capture and repeated windowed/fullscreen input replacement passed
at the approved settings milestone (VALIDATION-INPUT.md); the 40-cycle Mac
probe does not open a microphone. No audio is saved. Sanitizer quarantine can
increase its process resident memory even when leak checks pass; its resident
numbers are not used as a production leak verdict.

`tools/check_dawn_window.py --plugin <terrain module>` now includes three
failed-initialization cases: missing plugin, invalid shader and missing input.
All three return errors and release their nonce control records on Windows,
Mac and Linux. Together with the five normal/device-loss cases, all eight pass
on Windows D3D12, Mac Metal and Linux lavapipe. Accelerated test camera time
uses a fixed frame step, preventing a compositor pause from skipping an entire
camera phase; interactive rendering continues using monotonic time.

The current native CTest sets pass 12/12 on Windows, Mac and Linux. This is bounded
evidence of cleanup and limits, not proof of indefinite stability or absence
of every possible leak.
