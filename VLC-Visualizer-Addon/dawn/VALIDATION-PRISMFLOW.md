# Prism Flow native window integration

Measured 2026-11-16. See the renderer's detailed
[frame/scene validation](../../Geiss64/native/prismflow/VALIDATION-FRAME.md)
for original-engine numeric comparisons and retained cross-adapter failures.

The workbench now accepts `--visualizer geiss --plugin <native-module>
--assets <absolute-Prism-Flow-shader-directory>`. It loads the `geiss` settings
profile, passes saved renderer settings to the module and registers window
controls under that visualizer ID. Terrain remains the default workbench
renderer. This is not production launcher dispatch or a rebuilt installer.

## Observed native window behavior

Windows D3D12, Mac Metal and Linux lavapipe each present 90-frame Prism Flow
windows and pass resize plus double-click fullscreen/restoration exercises.
Separate closure cases observe Escape (Windows), right click (Mac), and the
shared menu-close protocol (Linux). An additional effect-key run observes
eight actual SDL events, forwarding W/P/D/S/M/L/Backspace/Space with Num Lock
enabled; the lock-state bit is removed without removing real accelerator
modifiers. That run closes through menu protocol (Windows), Escape (Mac)
and right click (Linux). Mac/Linux also start fullscreen and restore the
original window bounds. At most two frames remain in flight.

The direct/module scene probe separately exercises Enter/hard navigation and
each effect operation. Native module errors reject invalid supersampling,
zero dimensions and nonfinite waveform data. The same runtime remains usable.

The workbench retains standalone input selection / Ctrl-L / Command-L,
player audio isolation and player-lifetime watches from earlier milestones.
VLC's actual menu and player dispatch are still D40/D42/D44 checks.

## Capture results and limits

Windows Prism Flow starts from `--line-in default`, opens the native input
picker four times and activates five capture streams in the bounded run.
The final stream delivers 74,752 frames and Escape closes normally. The
report records **2,560 FIFO drops** during the run; this is not a zero-drop
or uninterrupted-capture claim. No audio is saved. Further render/capture
stress and warm-up measurement remain D61/D62.

The rebuilt Mac app's new capture check is **pending OS permission**.
Launching the executable directly over SSH returned NotDetermined; launching
the app through Launch Services correctly remained responsive while waiting
for permission, but did not record or complete reselection within its
20-second bound. It is a retained failed/pending test, not a capture pass.
Earlier owner-approved native capture/reselection results are documented in
[VALIDATION-INPUT.md](VALIDATION-INPUT.md); development rebuilds change the
ad hoc app identity. No TCC record was edited/reset or permission bypassed.
Defer renewed capture approval until the next stable candidate to avoid
repeated prompts for changing development builds.

After the owner's latest approval, another Launch Services retry still reports
Pending with zero streams/pickers over 25 seconds. Its stderr is retained under
`~/vlc-visualizers-work/prismflow-capture-mac-approved`. Mac disk exhaustion
also prevented that attempt from saving result.json; the stderr contains the
failed result. Redundant hash-verified transfer fixtures and regenerable Dawn
build intermediates were subsequently cleared for production staging. The
retry remains a failed/pending capture check, not a pass.

The native window still uses SDR presentation. Linear offscreen output
greater than 1 is tested separately; HDR remains Experimental/off by default
and no physical HDR display result is claimed.

## Evidence and remaining work

Local Windows reports live under `build/prismflow-window-windows`,
`build/prismflow-keys-windows`, `build/prismflow-capture-windows` and
`build/prism-scene-comparison-windows-final`. Owner-machine reports use
`~/vlc-visualizers-work/prismflow-{window,keys}-<platform>` and
`prism-scene-comparison-*`. Reports copied back into `build/` stay local.

Retained NVK cross-adapter numerical failures and the separate unresolved
64-byte unsuppressed driver allocation report remain recorded. The later
`Geiss64/native/prismflow/VALIDATION-NATIVE-REFERENCE.md` comparison matches
24 presentation cases and 36 frames exactly on D3D12/NVK/lavapipe/SwiftShader;
it reproduces the old title/first-feedback cross-backend errors using two
original-JavaScript outputs. The old cross-adapter failures are not relabeled
as passes. Passing software sanitizer checks does not erase hardware findings. Original
Classic/MilkDrop/projectM dispatch, production manifests/launcher integration,
soaks and final install/security
validation remain open. No prior tag or release artifact is replaced.

Native image drop and cancellable palette publication now pass separate
worker, optional-control ABI, lifetime and native-window checks on all three
systems. See the renderer's VALIDATION-IMAGE-IMPORT.md. These checks complete
D33 implementation, while D35 comparison limits and release gates remain.
