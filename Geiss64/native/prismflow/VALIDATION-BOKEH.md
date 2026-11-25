# Native bokeh simulation, rendering and statistics

Observed 2026-11-16 during D33. `gpu_bokeh.cpp` ports the original bokeh pool,
spawn packing, aspect-correct coordinates, depth/lifetime envelopes, motion
following, culling, rolling death history, cached asynchronous statistics and
present-only drawing. It executes the preserved `bokeh_advection`,
`bokeh_deaths`, `bokeh_stats` and `bokeh_draw` WGSL. The simulation does not burn
bokeh into the iterative feedback image. Drawing retains RGB-only additive
blending and preserves the output alpha channel.

This is a private native helper, not production dispatch or full scene parity.
HDR remains Experimental and off by default. Linear offscreen RGBA16F output
does not establish HDR surface support or physical display behavior.

## Ownership and bounds

The helper retains at most 4,096 pending requests, 4,096 GPU particles, 8,192
death events, 1,024 history samples, three format-specific draw pipelines and
one outstanding 16-byte statistics readback. Pending doubles are preserved
through expiry filtering, then packed to float32 as in the source. The pending
and upload arrays are allocated once on the heap (576 KiB of payload storage),
and do not grow with playback duration. GPU buffers have 467,112 declared
payload bytes, with backend alignment, pipeline and bind-group overhead
additional. The host's existing frame and dimension budgets still apply.

Statistics poll ready work with a zero-timeout wait, retaining cached values
while a snapshot is pending. Requests are throttled to 30 ms using a caller-
supplied monotonic clock. A history-generation check prevents an older snapshot
from replacing a newly seeded mean. Callbacks capture heap-owned state, never
the renderer object; their futures use the host's bounded callback registry.
Closure destroys the staging buffer and delivers ready cancellation without
waiting for rendering. The host retains tracking/module ownership for deferred
callbacks through GPU teardown. Exceptional quota rejection cancels and delivers
the rejected callback before recovery; normal refreshes do not block. A
cancellation timeout retains its original handle and fails rather than silently
restarting a request.

## Local original-renderer oracle

The existing local harness supports `--case bokeh`, executing unchanged
`GPUWarp`/`GpuBokehDots` methods. It adds diagnostic `COPY_SRC` usage to bokeh
buffers except the mapped readback buffer. It uses synthetic points/palettes,
offscreen outputs and loopback-only serving, blocks external requests and
retains browser sandboxing. Original source hashes, actual adapter and diagnostic
changes are recorded. The preserved local Electron runtime is a development
oracle; no Electron/Node dependency is added to native rendering or payloads.

Sixteen frames exercise capacity pressure, expiry before submission, fade/depth
changes, motion-follow weights, clipping/culling, two simulation resizes, output
resize, reseeding and all three render formats. There are forty-eight output
draws across RGBA16F/RGBA8/BGRA8. For ticket rollover, a diagnostic input seeds
the GPU history counter at `0xfffffff0`, then forces enough deaths to wrap it.
The new ticket, processed ticket, sample count and ring offset must match both
the original and an independent unsigned arithmetic check using actual death
events, including deaths from previously living particles.

Atomic spawn allocation and death-ticket assignment do not promise fixed slot
order. The probe compares canonical living particle contents, sorted history
ages and death ages, plus exact counts/headers/controls. Unused inactive payload
bytes and arbitrary ticket order are not substituted for simulation behavior.
Every complete comparison checks:

- 131,856 living-particle components and 1,792 exact update/draw control bytes.
- Exact packed spawn bytes for every submitted request and exact acceptance,
  capacity, living/death counts and history headers.
- 16,384 history ages; maximum observed history error is zero on the listed
  final comparisons. Lifetime-mean error must be at most 0.000001 seconds.
- 365,568 feedback components and 3,502,080 bokeh output components, including
  unchanged output alpha.
- Single-pending-readback behavior, the 30 ms boundary, reseed protection,
  quota cancellation/recovery, invalid inputs and the overflow fixes below.

The particle limit was selected before results: `0.0002 + abs(reference)*1e-6`.
The existing feedback limit remains 0.002. Draw limits remain maximum 0.004 and
mean 0.0002, with byte differences divided by 255. `--exact` additionally
requires zero particle/feedback/draw bit differences; the passing comparisons
below use the fixed numerical limits and do not claim bit-exact pixels.

| Native / original-renderer path | Particle differing bits / max error | Draw differing components / max error | Result |
| --- | --- | --- | --- |
| Windows RTX 3070 Ti Laptop D3D12 / same adapter | 1 / 5.96e-8 | 1,548 / 0.00048828125 | pass |
| Mac M1 Metal / same adapter | 0 / 0 | 9 / 0.00048828125 | pass |
| Linux GTX 1660 Ti NVK / common Windows NVIDIA fixture | 1,783 / 1.544e-5 | 2,729 / 1/255 | pass |
| Linux SwiftShader ASan/UBSan/leak detection / same adapter | 0 / 0 | 15 / 0.00048828125 | pass, no sanitizer report |

These are recorded final-run counts, not a promise of a fixed bit-difference
count on every repeat. Draw counts vary between runs with the same fixtures;
atomic slot allocation can change the order of half-float additions. All
recorded runs keep their actual differences and apply the same fixed limits.
Earlier same-adapter ordinary SwiftShader comparisons also pass. The new bokeh
result does not convert the separate failed NVK warp-dot or lavapipe
feedback/title comparisons into passes; see VALIDATION-DOTS.md,
VALIDATION-FEEDBACK.md and VALIDATION-PRESENTATION.md.

## Confirmed source overflow cases and native guards

The oracle reproduces two source faults before resetting normal scene state:

1. A finite `x=1e300` spawn is accepted by the original, but packing its screen
   position to float32 produces infinity. The native helper rejects requests
   whose packed GPU fields or expiry are not representable as finite float32.
2. The original accepts the maximum finite float32 lifetime as a history seed.
   The actual GPU reduction of 1,024 such values produces an infinite mean.
   Native seeding rejects a value whose full window sum cannot stay finite.

Both expected differences are explicit regressions, not pixel-limit changes.
Ordinary scene packing/controls/history remain compared to the original.
These demonstrated invalid-data paths are not evidence of the cause of the
historical computer crash, and no kernel/driver stability claim follows.

## Bounded lifecycle checks

Forty create/simulate/draw/destroy cycles pass on Windows, Metal, NVK and
SwiftShader with Linux sanitizers. Each run also checks forty failed partial
constructors (missing compute shader after buffer allocation), twenty closures
with a readback pending and twenty completed snapshots. A deliberately retained
32,768-byte texture must increase live texture accounting before release.
After the cycles, all 128 host callback slots must accept real new futures and
deliver every completion, proving cancellation did not retain registry slots.

Live texture bytes return to the cold baseline and total live GPU bytes do not
grow after warmup. Handles/descriptors stay constant at samples 8, 24 and 40:
Windows 433, Metal 9, NVK 11, sanitizer SwiftShader 7. Windows/Mac/NVK sampled
RSS is unchanged in those final runs. Sanitizer RSS varies (about 392 MiB to
352 MiB); constant process RSS is not claimed. The shared Dawn and driver
libraries are not themselves instrumented.

An explicitly separate lavapipe lifetime-only run also passes all forty cycles,
partial failures, pending cancellations, texture control and future-slot
recovery. Its live GPU bytes stay 22, live texture bytes stay at the 6-byte
driver baseline and descriptors stay 6; sampled RSS changes by 32 KiB.
No same-adapter original-renderer lavapipe image comparison is claimed.
Longer full-renderer soaks and absence of every possible leak remain unproven.

The final optional bokeh CTest passes on Windows, Metal and NVK. All seventeen
tracked WGSL modules still verify byte-for-byte against their preserved source.
All seven compute passes and fifteen of the sixteen render-format variants now
have native execution coverage; native text-overlay execution/rasterization and
complete renderer/controller integration remain D33 work. D34/D35/D37 and final
installer, player and release gates remain open.

The final Mac rebuild initially failed on a full data volume. Two task transfer
archives, totaling 191,243,089 bytes, were removed only after their SHA256 hashes
matched retained Windows copies. Sources, reports and release artifacts were
preserved. The rebuild and CTest then passed; the Mac still has limited disk
headroom. This was an observed storage failure, not a renderer failure.

## Reproduction

From Geiss64, using an existing owner-local preserved runtime:

```text
<local-electron> tests/prismflow_feedback_reference.cjs --case bokeh --assets assets/prismflow --output build/bokeh-reference
```

Build `prismflow_bokeh_probe` in the addon workbench, then run:

```text
prismflow_bokeh_probe --assets ../Geiss64/native/prismflow/shaders --fixtures <local-reference>/fixtures.json --output build/bokeh-native --cycles 40
```

Optional `--backend d3d12|metal|vulkan` selects the native backend. Inspect the
actual reported adapters; Linux software reference capture selected SwiftShader.
Per-child `VK_DRIVER_FILES` can select that preserved local runtime's ICD.
`--lifetimes-only --cycles 40` requires no fixtures and labels its scope as
resource/state coverage without image parity. `--exact` retains its stricter
zero-difference requirement and is not used by the reported fixed-limit runs.

Set `PRISMFLOW_BOKEH_FIXTURES=<absolute-local-path>/fixtures.json` in an existing
addon CMake build to register `dawn_prismflow_bokeh_reference`. Tests never
retrieve fixtures or launch Electron automatically. Synthetic raw fixtures,
profiles, reports and the intentionally incomplete shader manifest remain
ignored local build data. No samples or reports were uploaded to a third-party
analysis service and no driver/security settings were changed.
