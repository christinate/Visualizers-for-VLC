# Guarded Windows draft installer

Checked on 2026-11-17. This is a new local development draft of
`0.5.0-rc.1`; no prior artifact/tag was replaced and no release was published.

## Exact inputs and package

`build/native-candidate-0.5-windows-v3/` contains the combined NSIS installer:

```text
SHA-256 adedec1a2b5e54e192562646a07839cc43d52d544374f34881580e7babf8228c
Bytes   20,199,104
```

It includes the original/core guard, scanner Unicode/NUL corrections and the
current native runtime, shaders/assets and Qt interface. The packager verified
the pinned Dawn/SDL provenance, all 36 PE normal/delay import sets, one shared
Dawn runtime and plugin-only components. It rejects browser/Node/Electron
payload files. Extracted payload size is 70,296,766 bytes, including metadata
and bootstrap; this is not a measurement of peak temporary disk usage.

The rebuilt launcher SHA-256 is
`2579a00ff46cc775083407abdc9e2b156080d9d62bab2939d20fdcc3a48502a2`.
All 12 read-only guard cases pass for this exact binary in
`build/launcher-guard-windows-v5/result.json`. The restricted v4 attempt could
not create its same-file hard-link fixture; that setup is retained separately.
The new report supplements the earlier normal headless launch evidence in
VALIDATION-LAUNCHER-GUARD.md rather than replacing its different binary hash.

Binary staging used clean source heads recorded in the local
`input-provenance.json`, with Addon at `a68b34b`. During that installer check,
the later code change was to the test helper: `18b4be6` fixes its explicit wait
for GUI-subsystem executables. Subsequent README/report edits are not inputs
to this already-built draft.
The package and helper hashes are recorded separately.
Desktop Defender automatic reporting was not verified during this build;
the later observation and pending desktop privacy choice are recorded in
VALIDATION-LIVE-MENU.md. The verified local-only results below apply to
vpntest1 after transfer, not to the earlier desktop build/tests.

## Installer and local Defender results

On owner machine `vpntest1`, the corrected test passed all 14 checks from
08:12:41 to 08:18:18 UTC:

- Eight component selections, including all defaults and none. Each installed
  launcher additionally passed `--verify-original`; scanning, journal hashes,
  one shared Dawn DLL and actual NSIS exact-original restoration passed.
- Modified bundled-module refusal before removal, followed by successful
  uninstall after restoring the module bytes.
- In-use destination rollback and four invalid component selections rejected
  before mutation.

This uses isolated VLC 3.0.24 fixtures, not the machine's installed player.
It does not exercise real playback, physical HDR or real Winamp UI.

Defender signature `1.463.337.0` reports zero detections since preparation.
Two custom scans of this exact installer have matching start/completion scan
IDs in the retained local events (one for each harness run). Antivirus,
real-time protection and behavior monitoring remained enabled. MAPS 0 and
sample consent 2 were verified before transferring any candidate, and no
external analysis submission occurred. This does not establish a cloud
verdict or resolve the historical 0.2 behavior detection.

All private QA candidates, payloads, fixture copies and helpers were removed
before restoring the saved MAPS 2 / sample consent 1. No QA process, uninstall
key or recent visualizer-named temporary executable remained. Restoration
and active protections were verified at 08:23:26 UTC. Only settings/cleanup
JSON records remain in the remote QA root.

## Retained harness observations

Windows first refused the saved `.ps1` under its execution policy before
installation began. The reviewed commands ran through standard SSH command
input; the execution policy was not changed.

The first added guard assertion incorrectly used PowerShell's direct GUI
executable invocation and stale `LASTEXITCODE`. Waiting explicitly gave exit
0 for that installed launcher. The helper now uses `Start-Process -Wait
-PassThru`; the failed fixture was uninstalled and all three original hashes
verified before the fresh passing run. Both reports are retained.

The restricted multi-worker build also spawned waiting MSBuild workers with
no compiler progress. Only that verified process tree was stopped; a
single-worker rebuild outside the restricted environment passed. A long
remote cleanup command failed before execution; current files/settings were
checked before the same guarded cleanup ran through command input.

Owner-local evidence is under
`build/native-combined-installer-vpntest1-guard-v1/`, including input provenance,
both test reports/logs, scan events and cleanup. This draft leaves all remaining
`todo.md` gates open, including final-source artifacts, real-player matrices,
extended resource measurements, Mac return work and final installer/security
validation.
