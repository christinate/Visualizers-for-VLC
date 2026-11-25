# Stability and validation

The current native candidate is **0.5.0-rc.1**, still under release validation.
[README.md](README.md), [CHANGELOG.md](CHANGELOG.md) and
[RELEASES.md](RELEASES.md) describe the native runtime and current limitations;
the Addon repository's `todo.md` tracks the remaining suite gates. The older
measurements below retain their original version and exercised-path scope.
They do not qualify all current renderers or establish indefinite leak freedom.

## Historical 0.2.0-rc.1 validation

Tests were performed on this Windows AMD64 machine with MSVC 14.50.35717 and
VLC 3.0.24. These are measured results for the exercised paths, not a claim of
universal compatibility or proof that every leak has been eliminated.

## Confirmed source defects corrected

* Geiss allocated frame/map buffers using C allocation and released them with
  C++ scalar delete. They now use `free` on the original allocation addresses.
  Its old pointer alignment narrowed addresses to 32-bit integers; it now uses
  `uintptr_t`. Partial allocation failure now frees every acquired buffer and
  returns failure before any null pointer is aligned or used.
* MilkDrop allocated five mesh/index buffers with `new[]` and used scalar
  `delete` during resize/cleanup. All five releases now use `delete[]`.
  GString's character and wide-character arrays had the same mismatch and are
  corrected.
* MilkDrop passed byte counts to Unicode profile APIs that require capacities
  in wide characters. Font faces, preset paths, custom messages, and image
  names now pass `_countof` capacities. This corrects actual overstatement of
  the destination capacity; no claim is made that the tested short values
  previously overflowed.
* MilkDrop's `_create_locale` result had no matching release. It is freed at
  DLL unload. Windows dialog procedure/extra-byte storage now preserves
  pointer-sized values in x64 builds.
* The port no longer executes Geiss's x86 self-modifying/MMX warp or MilkDrop's
  x86 expression-code generator. Geiss uses a bounded portable warp; MilkDrop
  uses the pinned WDL portable EEL backend.
* The host's early exits now clean up the renderer, loaded DLL, DLL-directory
  cookie, presentation resources, and protocol/input windows. A distinct
  hidden protocol parent prevents recursive forwarded-key feedback.

These are identifiable ownership, capacity, and architecture defects in the
source. They do not establish which defect, driver, or other component caused
the historical whole-computer crashes. The x64 renderer runs outside VLC and
Winamp so a renderer-process failure does not share their address space.

## Executed checks

| Check | Observed result |
| --- | --- |
| Both renderers, simultaneous 30-minute demo-audio stress run | Exit 0; 52,788 Geiss frames and 52,829 MilkDrop frames; effect/preset keys exercised |
| Startup fullscreen, requested-HDR fallback, borderless/fullscreen/native key round trips | Passed through the native window procedures; original rectangle restored |
| Final Geiss DLL: 12 init/render/quit/unload cycles × 300 frames | Exit 0; map/palette/waveform keys exercised; Geiss retains its fixed engine buffer |
| Final MilkDrop DLL: 8 cycles × 300 frames | Exit 0; repeated render-target/mesh reallocation at three sizes |
| Geiss ASAN DLL: 8 cycles × 600 frames | Exit 0, no sanitizer diagnostic; mismatch detection enabled |
| MilkDrop ASAN DLL: 8 cycles × 600 frames | Exit 0, no bounds/use-after-free diagnostic; mismatch-check limitation below |
| VLC decoded-audio matrix | Eight cases passed: 8 kHz mono PCM16; 44.1 kHz stereo PCM16; 48 kHz stereo PCM24; 96 kHz stereo PCM32; 192 kHz stereo PCM24; 48 kHz 5.1/7.1 PCM16; 48 kHz stereo float32 |
| Winamp ABI bridges | Both x86 and x64 callbacks reached each x64 renderer with 48 kHz audio and exited cleanly |
| Real VLC menu integration | Two entries attached; Open launched Geiss with decoded VLC PCM; VLC exit 0 |
| Audio-core CTest | Passed sample folding, FFT, interpolation, ring wrap, stale silence, and nonfinite input handling |
| Backup/restore integration | Verified hashes; original-executable copy; persistent scan; stale/missing-package refusal; traversal rejection; wrapper scan command; modified-file preflight refusal; byte-identical restoration; locked-file rollback and staging cleanup |

The matrix alternates renderers, rather than running every format through both
engines. VLC decodes the source to the common float-PCM bridge. The native menu
test enables a test-local module; system VLC files were not patched. Patch tests
use workspace copies of the actual installed executable and core.

After the first minute of the 30-minute run, Geiss private memory ranged from
62,308,352 to 62,681,088 bytes (GDI objects fixed at 7), and MilkDrop ranged from
174,534,656 to 174,624,768 bytes (GDI objects fixed at 45). Handles ranged 236–242
and 293–299 respectively. Final normal lifecycle runs keep Geiss handles/GDI
flat and MilkDrop handles/GDI within the recorded small range; private bytes
vary with allocation and graphics-driver caching. Those counters alone cannot
prove the absence of leaks. See the raw JSON/JSONL reports.

The 30-minute run preceded the final packaging rebuild and MilkDrop's
locale-unload correction. `validation/tested-builds.json` in Project 3 records
both the tested and final binary hashes. Final packages were rechecked with the
audio matrix, bridges, menu, and normal lifecycle tests. The locale change
runs on unload and is exercised by the final lifecycle check.

## Sanitizer scope and limitation

Windows AddressSanitizer does not provide a complete leak check. Its quarantine
retains freed allocations, so ASAN private-memory growth is not interpreted as
a production leak. MilkDrop's run uses
`ASAN_OPTIONS=alloc_dealloc_mismatch=0:halt_on_error=1`; other ASAN checks remain
active. The compiler/runtime reports an allocation mismatch for a correctly
paired `new wchar_t[1]` / `delete[]` in a dynamically loaded minimal DLL.
Project 3's `tests/asan_dll_sanity.cpp` reproduces that result without MilkDrop
code, and `validation/asan-dll-sanity.log` preserves it. The actual source-level
mismatches above were corrected independently by inspecting allocation/free
pairs. Do not call the suppressed sanitizer check a pass.

## Remaining release sign-off

No actual Winamp UI session, NSIS install/uninstall registry round trip in a
disposable VM, physical HDR brightness/color check, long overnight soak, broad
third-party MilkDrop preset collection, multi-GPU/monitor migration, Windows
sleep/resume, or driver-reset campaign was completed. Only the bundled starter
preset was tested. Hardware HDR output falls back to SDR here; the effects
remain SDR and are expanded by the common scRGB presenter.

A stable release should complete those checks on the intended hardware before
being advertised as stable. No evidence here establishes a fix for a kernel
or graphics-driver crash. Installers are unsigned release candidates.

## Reproduce

Build renderer installers first, then build Project 3 and run:

```powershell
ctest --test-dir build -C Release --output-on-failure
python tools/test_menu.py
python tools/test_audio_matrix.py
python tools/test_winamp_bridges.py
python tools/test_patch.py
python tools/soak.py --seconds 1800
```

Set `VLC_INSTALL` for a non-default VLC 3.0.24 path. Menu/graphics tests need an
interactive Windows desktop. The renderer probe accepts DLL path, assets path,
cycle count, and frames per cycle. Enable `ENABLE_ASAN` in a renderer and
`ENABLE_ASAN_TESTS` in Project 3 to build instrumented probes, and put MSVC's
`clang_rt.asan_dynamic-x86_64.dll` directory on PATH before running them.
Use the documented MilkDrop mismatch-check setting only with this independently
reproduced toolchain limitation. Keep normal release DLLs uninstrumented.

## Native 0.3.0-rc.1 validation (2026-11-14)

The inline menu was exercised with VLC 3.0.24 on Windows AMD64, VLC 3.0.20
and Qt 5.15.13 on Linux Mint 22.3 x86_64/X11, and VLC 3.0.24 on Apple M1
macOS 26.6.2. The menu follows View; on Windows/Linux it immediately precedes
Help. macOS retains its platform menu ordering. Both native renderers received
real decoded VLC PCM. Eight cases passed on each native platform: 8 kHz mono,
44.1 kHz stereo, 48 kHz 24-bit, 96 kHz 32-bit, 192 kHz 24-bit, six/eight-channel
48 kHz and 48 kHz float. Standalone demo input was not used for those audio tests.

Both renderers completed simultaneous 1,800-second stress runs on each native
computer, with automatic preset/map changes and key/fullscreen self-tests:

| System | Engine | Frames | Observed resident-memory range after warm-up |
| --- | --- | ---: | --- |
| M1 macOS | Geiss | 51,295 | 42,254,336–76,513,280 bytes |
| M1 macOS | projectM MilkDrop | 51,185 | 49,692,672–84,918,272 bytes |
| Mint x86_64 | Geiss | 51,031 | 131,112,960–131,702,784 bytes |
| Mint x86_64 | projectM MilkDrop | 38,726 | 174,747,648–175,902,720 bytes |

All four exited normally. No shader/preset errors were written during these
runs. Mac memory returned below its transient high-water marks. RSS includes
shared libraries and graphics-driver allocations; these bounds are observations,
not proof that no leak exists. CPU/GPU contention from concurrent tests affected
frame throughput on Mint. Native metrics and process logs are retained.

A Linux Geiss CPU-only probe completed 16 init/render/shutdown cycles and 1,440
frames under AddressSanitizer **with LeakSanitizer enabled**, without a report.
A separate 60-second instrumented Geiss graphics run completed 1,673 frames;
leak detection was disabled only for that graphics run because it also loads
uninstrumented SDL/Mesa/Qt driver runtimes. No native projectM sanitizer claim
is made. Both native engines also passed direct borderless/fullscreen and L-key
round-trip tests. Packaged launchers were tested with real VLC audio for each
engine, in addition to direct module activation.

Confirmed native problems addressed:

- Legacy Windows `long` fields would widen on LP64 macOS/Linux. The extracted
  Geiss core uses explicit `int32_t`/`uint32_t` for those legacy integer widths.
- SDL2 Cocoa presentation waited indefinitely in `Cocoa_GL_SwapWindow` on an
  occluded/background SSH-launched window. A process sample showed the wait;
  presentation now uses swap interval zero and the host's bounded frame pacing.
- Explicitly supplying the same `--extraintf` as the native launcher activated
  the adapter twice. Launchers now deduplicate the colon-separated interface
  list. The packaged-launcher playback test passes after this correction.
- Cocoa's menu delegate previously trusted the JSON cache's field types.
  Malformed entries are filtered before menu construction, keeping JSON
  exceptions from escaping through the delegate into VLC.

Native installer tests cover default components, opt-outs, persistent scans,
missing-engine refusal, rescan removal, modified-file refusal, exact original
restoration and removal of owned files. Linux projectM dependencies resolve via
`$ORIGIN`; macOS dependencies use `@loader_path` and only addon-owned binaries
are ad-hoc signed. The original signed VLC app is unchanged.

Windows tests exercise the rebuilt inline Qt interface through the launcher's
real extra-interface path, including decoded audio and the original engine's
keys/fullscreen handling. Filesystem patch tests now include the original Qt
module and all private Qt dependencies, with byte-identical restoration and
in-use rollback. Full NSIS registry integration, the original Defender behavior
alert, Winamp UI, physical HDR displays and overnight endurance remain separate
checks. Native SDR and the documented key subsets are the supported scope.

Startup fullscreen and 512 × 384 window restoration were verified separately
for both native engines on both systems. Linux fullscreen rendering needed a
longer test allowance to reach the self-test's 45-frame checkpoint; this does
not assert a guaranteed frame rate. Malformed JSON, a non-array catalog and
malformed entries were exercised through real VLC menus without a process crash.

The patch utility now hashes large Qt modules with a fixed 1 MiB buffer instead
of allocating a whole-file vector. Its SHA-256 install/restore tests still pass.
This bounds temporary hashing memory; the earlier allocation was not a leak.

A final full-payload Windows test reproduced MAX_PATH failure while staging a
long Qt license filename. The control utility now preserves extended absolute
paths through canonical containment checks, normalizes separators and uses
those paths for copy/restore. The complete install/verify/restore/rollback test
passes with paths beyond 260 characters, without changing Windows policy.

The native fullscreen self-test also sends SDL click counts 2 and 4, covering
two rapid double-click pairs in one multi-click sequence. Both even counts
toggle fullscreen; the old equality-to-2 check would ignore the second pair.
The renderer engines were unchanged by this input correction; final host
windowed/fullscreen cases were rerun separately from the 30-minute soak.


## 0.4.0-rc.1 validation and remaining work

HDR is Experimental and disabled by default, as requested by the owner. No
physical HDR display is available; calibration/appearance is not certified.

The original classic engines retain their prior 30-minute and sanitizer
validation. The new WebGPU renderers have bounded smoke/flight tests, not an
indefinite-stability guarantee. Terrain completed all four 60-second camera
modes in a 270-second Windows test (7,820 reported frames). Prism Flow reported
two RGBA16F feedback textures and four active motions on Intel Iris Xe.
MilkDrop standalone received nonzero PCM through the synthetic capture broker.

Specific fixes: capture initialization failure now stops acquired tracks and
closes its AudioContext; input lists remain usable after permission denial;
synthetic input does not open a microphone; native source reselection drops a
stale IPC mapping; normal native playback no longer appends self-test metrics
forever; browser logs rotate at 8 MiB; C++ exceptions are caught at N-API
callbacks. The terrain half-float rounding carry is covered by a regression.
Settings migration preserves older Windows INI display preferences. Installers
refuse an older registered package, including the legacy Geiss64 product,
before it can conflict with the new package's uninstaller.

The local-only Windows installer matrix passed standalone plus both synthetic
Winamp architecture layouts for all three plugins, including upgrade refusal,
owned-file uninstall and preservation of a user file. This does not replace
real Winamp playback validation. Computer Use was stopped by physical Escape;
real Winamp UI checks remain pending. Extended soaks of the new WebGPU engines
and physical recording-device changes also remain for stable-release signoff.

Mac and Linux archive install/scan/opt-out/uninstall checks passed, including
modified-file refusal and exact restoration. A separate regression verified
framework directory links, file links, changed dangling-link refusal, and
original symlink restoration on both platforms. Mac Terrain rendered using a
floating-point canvas. Mint's current graphics stack returned no compatible
WebGPU adapter; Classic Geiss/projectM remain the tested options there.

## Mac return — 2026-11-17

Arm64 projectM rebuild and actual VLC PCM/fullscreen/key fixtures pass. The
vendored CMake code incorrectly took CMAKE_PROJECT_VERSION from the parent
(0.5.0); using its own PROJECT_VERSION restores the 4.1.8 dylib metadata.
Isolated before and after modules both load. This is a confirmed metadata
fix, not a demonstrated crash/load fix. The exact patch hash is recorded in
dependencies.lock.json. Final capture/soak/release checks remain open.
