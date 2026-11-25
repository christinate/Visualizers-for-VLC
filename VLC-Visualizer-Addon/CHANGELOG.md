# Changelog

## 0.5.0-rc.1 — native candidate, validation pending

- Replace the production Electron runtime with one shared native Dawn host,
  native settings/capture/input selection and API 4 module dispatch.
- Retain inline Visualizers menus, persistent scanning and click-time checks;
  add Open/Settings/Close/Close all with separate player-owned windows.
- Add native NSIS/Linux/Mac package builders, ownership/restoration checks and
  pinned dependency provenance; bundle plugin components without repeated
  standalone runtimes. Preserve earlier artifacts and the security-held 0.2
  installer unchanged.
- Fix reproduced catalog NUL/Unicode/path failures, malformed Cocoa alerts,
  recursive backup-wrapper launches and Mac installer/app-path/link faults.
- Populate bundled Windows visualizers on first installation and restore VLC
  if that initial scan fails. Bundle MilkDrop's original shader compiler and
  balance its helper-runtime references through rendering/configuration cleanup.
- Keep reading live PCM during long native input-selection dialogs to prevent
  recording FIFO overflow.
- Pass bounded actual player tests including 60 Mac menu/media cases. Fresh
  Mac dependencies target 11.0, tested on 26.6.2. Windows passes 23 native tests
  and four engines each in genuine VLC and Winamp 5.9.2 x86 playback with a
  private silent output fixture. Keep HDR Experimental/off; final-source,
  remaining player/recording/stability checks and deferred Mac gates stay open.

## Earlier native migration milestones

- Add native SDL/Dawn windows and bounded GPU submission, resize/fullscreen
  handling, menu closure and controlled device-loss reporting.
- Port float PCM analysis and Terrain orchestration with JS reference checks.
- Build Dawn from pinned source on Windows/macOS; prepare Linux source builds
  and preserve the existing Electron candidate until native parity is complete.
- Use a static Windows C runtime and collect native dependency provenance and
  notices. All validation remains on the owner's machines.
- Correct C++20 UTF-8 control-record serialization and free ignored SDL drop
  event allocations in the shared native host.

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


## 0.2.0-rc.1 security hold — unresolved

Hold addon 0.2.0-rc.1 after reported Microsoft Defender behavioral quarantine.
Preserve original artifacts; gate packaging; add read-only diagnostics and
prepared Microsoft submission details. Detection remains unresolved.

## 0.2.0-rc.1

Reversible version-gated VLC launcher patch, native Visualizers menu, decoded-PCM transport, persistent JSON scanning and click-time checks, backup/restore journal, bundled optional Geiss/MilkDrop installers, and reproducible validation tools.

Independent repository, README, pinned dependencies/notices, build CI,
local release artifacts and SHA-256 checksums. Remotes and public download
URLs remain unset until supplied. See STABILITY.md where applicable.
