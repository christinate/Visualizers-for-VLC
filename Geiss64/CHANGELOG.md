# Changelog

## 0.5.0-rc.1 — native candidate, validation pending

- Replace the modern Electron host with native C++/Dawn, including dynamic
  motion generation, RGBA16F feedback, palettes, text/images and supersampling.
  Use the approved Prism Flow name and preserve Geiss attribution/Classic mode.
- Retain the original Windows Classic engine and portable Mac/Linux Classic
  backend; expose native standalone audio selection and Ctrl-L/Command-L.
- Add independent native NSIS/archive packaging and a shared-runtime SDK.
- Keep the recording FIFO drained during device selection. Verify preserved
  shader hashes consistently in LF and CRLF checkouts.
- Correct calibration, malformed diagnostics and Mac floating conversion;
  exact original-JavaScript scene/presentation checks pass on the documented
  D3D12, Vulkan and Metal paths. Fresh Mac drafts target 11.0, tested on 26.6.2.
- Keep HDR Experimental/off by default. Actual Winamp UI, final-source,
  remaining stability/security/platform checks and real Mac capture are open.

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

Native AMD64 renderer with portable bounded warp and pointer-width alignment; matched allocation cleanup; isolated host, HDR-output option, x86/x64 Winamp capture bridges, and NSIS VLC/Winamp installation.

Independent repository, README, pinned dependencies/notices, build CI,
local release artifacts and SHA-256 checksums. Remotes and public download
URLs remain unset until supplied. See STABILITY.md where applicable.
