# Changelog

## 0.5.0-rc.1 — native candidate, validation pending

- Run waveform history, beat-height terrain, palette changes and all four
  timed camera modes through native C++/Dawn with native windows and input UI.
- Remove the recurring decorative ring; fix the diagonal white-pixel artifact
  in raw GPU output and retain animated pixel regressions.
- Preserve borderless/fullscreen preferences, double-click restoration and
  Escape/right-click/VLC-menu closure without a browser production runtime.
- Keep reading live PCM while the native device chooser is open.
- Add independent native installers/archives and shared-runtime packaging.
  Pass bounded actual VLC PCM checks and Mac ten-format rendering checks;
  the latter do not qualify beat counts for arbitrary music.
- Keep HDR Experimental/off by default. Fresh Mac drafts target 11.0 but are
  executed only on 26.6.2. Final-source, real Winamp UI, recording and remaining
  platform/stability/security gates remain open.

## Earlier native migration milestones

- Add the native C++ waveform-history, palette and four-mode camera controller.
- Match 24 browser-controller fixtures, including exact RGBA16F history data
  and camera/palette uniforms.
- Render through the Addon native window workbench on Windows, macOS and
  Linux; recording input and player integration remain in progress.
- Remove recurring decorative ring/arch geometry from the shared Terrain
  shader while retaining waveform shape, beat height and camera movement.

The preserved 0.4.0-rc.1 artifacts are unchanged.
