# Changelog

## 0.5.0-rc.1 — native candidate, validation pending

- Route original 64-bit Windows MilkDrop 2 and Mac/Linux projectM through the
  native suite, without an Electron/Chromium/Node production runtime.
- Provide native standalone recording selection/reselection and separate
  player-owned decoded-PCM input; retain original keyboard behavior.
- Synchronize the native SDK's input-picker polling and capture regression.
- Add independent native NSIS/archive packaging with plugin-only VLC installs
  and shared runtime ownership. Correct projectM's own 4.1.8 dylib metadata.
- Pass bounded actual VLC PCM and Mac ten-format/projectM checks. Fresh Mac
  drafts target 11.0, with execution tested only on 26.6.2.
- Fix startup on clean Windows installations by loading the packaged DirectX
  helper and original shader compiler. Balance their runtime/configuration
  references, including partial initialization cleanup.
- Pass actual Winamp 5.9.2 x86/VLC decoded-PCM cases and six bounded original
  renderer unload cycles. Winamp uses a private silent output test fixture.
- Keep HDR Experimental and its limits explicit. Final-source builds/installers,
  remaining player/stability checks and deferred Mac capture remain release gates.

## 0.3.0-rc.1 — local cross-platform candidate

- Move Visualizers into VLC's menu bar after View; keep Help on the same row
  on Windows/Linux, with native Cocoa menu integration on macOS.
- Add native 64-bit OpenGL/SDL2 hosts, the original Geiss CPU core and projectM
  4.1.8 MilkDrop presets for macOS/Linux; retain original Windows engines.
- Add native package manifests/API 2, installers, scan launchers and ownership
  journals; preserve original VLC app files on macOS/Linux.
- Verify decoded PCM formats, missing packages, fullscreen/keys, installation
  restoration and 30-minute native stress runs; record finite test limits.
- Preserve the unresolved 0.2.0-rc.1 Defender report and local-only policy.


## 0.2.0-rc.1

Native AMD64 renderer with portable WDL EEL, native resource services, array/Unicode-capacity/locale cleanup fixes, isolated presentation, x86/x64 Winamp bridges, and NSIS VLC/Winamp installation.

Independent repository, README, pinned dependencies/notices, build CI,
local release artifacts and SHA-256 checksums. Remotes and public download
URLs remain unset until supplied. See STABILITY.md where applicable.
