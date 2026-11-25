# Native settings and window preferences

Development evidence observed 2026-11-15 on the native migration branch.
Production launcher/menu integration and installer validation remain separate
checklist work. No analysis or diagnostic data was uploaded.

## Storage and migration

`SettingsStore` reads the existing per-visualizer JSON files and retains unknown
plugin fields and the saved input identifier. Valid preferences retain their
values, including fractional camera durations. The editor submits changed
fields only, merging them with the latest file under a bounded per-ID lock.
Concurrent recording-input saves therefore do not discard other preferences.

When JSON does not exist, the store imports the legacy Windows `settings.ini`
section. UTF-8/BOM and UTF-16 encodings are supported. Saving creates JSON;
the original INI remains untouched and cannot override subsequent JSON edits.
Malformed, oversized or excessively nested JSON produces an error and is never
silently replaced. Files are bounded to 1 MiB and nesting to 32 levels.

Settings are flushed to a new file and atomically renamed into place. A
Windows concurrency test reproduced `MoveFileEx` error 5 while a reader held
the destination open, including with `FILE_SHARE_DELETE`. The writer now uses
`FileRenameInfoEx` with replacement/POSIX semantics so existing readers retain
their old snapshot and new readers receive the complete replacement. The
fallback for filesystems without that operation reports failed saves without
truncating the previous file. Defender settings were not changed for this fix.

The core checks pass on Windows, Mac and Linux: JSON/unknown-field preservation,
INI/UTF-16 migration, Unicode profile paths and device values, range checks,
corruption protection, size/depth rejection, 80 concurrent saves with a
continuous reader, and removal of temporary files. Linux ASan/UBSan with leak
detection also passes these cases. These checks do not establish absence of
all filesystem or driver faults.

Browser-generated recording device IDs are retained in the profile, but do not
identify native audio endpoints. The native picker requires a new selection;
after activation it saves the native backend's stable ID.

## Native controls

The editor uses Win32 controls, Cocoa controls and Linux Qt 6 widgets. It
exposes window size, frame limit, startup fullscreen, Experimental HDR and peak
brightness. Prism Flow adds engine/supersampling choices; Terrain adds camera
duration and beats between color changes. Settings apply on the next launch.
Cancel and owner-close cancellation discard edits. Saving an unchanged numeric
control preserves the original precision despite display rounding.

Five dialog fixtures pass on all three systems: unchanged precision,
display/HDR/engine/supersampling edits, Terrain edits, Cancel and owner-close.
The native editor does not initialize a GPU or open a recording input.

## Next-launch behavior

`tools/check_dawn_settings.py` imports an 800x600, 30 FPS, fullscreen INI
profile through the native editor, then launches the API 4 Terrain module.
It verifies borderless startup, the saved size/frame limit/camera/color values,
double-click exit from initial fullscreen, exact window restoration, another
fullscreen round trip and Escape closure. A 70,000-character unknown plugin
value survives unchanged and is not passed through the renderer's bounded
settings interface. All three systems pass; Linux window tests use lavapipe
because its current hardware X session has the documented DRI3 limitation.

HDR requests remain saved, but this workbench reports SDR presentation and
`hdr_active=false` until native HDR capability/presentation work is complete.
No physical HDR display check is claimed.

The five existing resize/close/device-loss window cases also pass on all three
systems: double-click restoration, fullscreen Escape, fullscreen right-click,
nonce-protected menu-close requests and controlled device loss. Actual VLC
menu clicks remain the production integration task D42.

At this settings milestone, CTest passed Windows 9/9, macOS 9/9 and Linux 8/8.
Real recording reselection passes on all three systems after settings
integration, including the Mac visualizer app after separate OS microphone
approval. The capture helper passes separately.
