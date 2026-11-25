# Native catalog and third-party example validation

Observed locally on the owner's machines, 2026-11-16. No samples, audio or
reports were uploaded to a third party. This is development integration,
not a rebuilt release or installer security verdict.

The standard-library validator checks all four manifest examples, development
and preserved catalog declarations, three API 4 platform/CPU variants, and
25 missing-field/incompatible/path/type rejection cases. It implements the
explicit subset used by these checked-in schemas and rejects unsupported
schema keywords; it is not a general JSON Schema library. The JSON schema
itself uses draft 2020-12. Native API 4 supports currently tested Windows
AMD64, macOS arm64 and Linux x86_64 targets, with the exact shared Dawn pin.

The MIT-licensed Audio Color module compiles separately against that pinned
SDK on Windows MSVC, Mac AppleClang and Linux GCC. It does not link Addon C++
implementation libraries, contain a private Dawn runtime, or depend on SDL,
Electron or Node. It renders a bounded RGBA16F image from analyzed RMS and
beat count, handles Space, retains/releases borrowed GPU references and has
no asynchronous callbacks.

`tools/check_dawn_example.py` passes on Windows D3D12, Mac Metal and Linux
lavapipe through the real production frontend. Each case copies the new ID
into an unused integration package directory, scans/checks it, loads its
exported API, presents at least 90 frames, resizes, restores fullscreen,
closes through the nonce menu protocol, verifies owned bytes, removes only
those files and rescans. The entry disappears from the persistent registry.
Mac staging resolves/signs only the copied example module; installed VLC and
the central runtime are not changed. Generated audio is used; no microphone
permission or real player playback is involved.

Windows `examples/dawn-visualizer/installer.nsi` compiles with the built AMD64
payload. Its NSIS bootstrap is x86 Unicode, while the renderer is AMD64.
The installer requires the native Addon, refuses an occupied package directory
and validates before scanning; its uninstaller compares all owned files to
embedded original bytes before deletion. **The example installer was not
executed in this check.** Runtime/rollback/modified-file and Defender checks
remain part of final installer QA, separate from compilation/module validation.

Source development manifests for Prism Flow/Terrain and all three staged
Windows package manifests pass their matching schemas. Original MilkDrop
remains API 1 on Windows and API 2/projectM on Mac/Linux. The native variants
coexist with preserved browser source manifests until D45/D50 packaging;
the old 0.4.0-rc.1 binaries are not relabeled API 4. Catalog development fields
explicitly say native installers are unavailable and real-player/release QA
remains pending. API 3 entries still describe preserved artifacts.

Evidence: `build/dawn-example-check-windows/summary.json` here and
`~/vlc-visualizers-work/native-catalog-example-check-{mac,linux}/summary.json`
on the owner's respective machines. The NSIS binary is an ignored local build
artifact. No release script was run, prior checksum overwritten, new tag
created or release published. HDR stays Experimental/off by default; current
native presentation is SDR. Retained NVK/permission findings remain separate.
