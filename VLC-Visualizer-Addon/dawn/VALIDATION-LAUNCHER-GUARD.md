# Windows original-executable guard

Observed locally on 2026-11-16 while preparing the real VLC menu checks.

## Reproduced fault and correction

The first private test setup copied the already-patched installed `vlc.exe`
over its private `vlc.visualizers.original.exe`. That made the backup another
addon wrapper. The previous launcher checked only that the backup existed,
then started it; recursive parent/child chains resulted. All processes from
that failed private setup were stopped by verified executable path and a
separate check confirmed zero remaining. Installed VLC files were untouched.

`src/vlc_original.hpp` now checks the backup before any normal launch:

- It must be a regular file distinct from the launcher, including hard-link
  identity; this also rejects a backup pointing to the launcher itself.
- Its bounded Windows version resource must identify a VLC 3 application
  whose OriginalFilename is `vlc.exe`.
- Its complete version must match the adjacent `libvlccore.dll` version,
  with application/DLL file types checked.

A wrapper, absent/corrupt resource or mismatched core now gives one repair
error before child creation. The verified current pairing is official VLC
3.0.24 AMD64. These metadata checks address the observed recursion; release
installers still require their independent exact hash/provenance checks.

The new read-only `vlc.exe --verify-original` command uses the same guard,
returns 0/1 and prints its result without opening VLC or a dialog. Scan remains
independent; combining verify and scan is refused so verification stays
read-only. Ordinary user launches retain the repair dialog on failure.

## Validation

`tools/check_launcher_guard.py` runs the actual rebuilt launcher with
`--verify-original` against 12 isolated cases: valid VLC, Unicode/spaced path,
old wrapper, current wrapper copy, same-file hard link, missing/empty/invalid
backup, DLL as backup, missing core, mismatched core and mixed verify/scan.
All 12 pass; every invalid case returns error 1 instead of executing its
backup. Input hashes stay unchanged. Protected system/installed files are
copied if Windows refuses a hard link. The initial hard-link setup failure
remains separate from the passing run.

```text
python tools/check_launcher_guard.py --launcher build/bin/Release/VlcVisualizerLauncher.exe --original <real-vlc.exe> --core <matching-libvlccore.dll> --old-launcher <preserved-wrapper.exe> --output <fresh-output>
```

Final guard evidence is in `build/launcher-guard-windows-v3/result.json`; the failed initial
setup is retained under `build/native-vlc-ui-windows-v1/`. The corrected private
player uses the backed-up original, not the installed wrapper. An unsupported
test-only `--no-qt-updates-notif` flag was also removed from that test invocation
after headless LibVLC identified it; this did not require a product change.

The same final launcher passes an independent scan with no original/core
present (`build/launcher-scan-regression-v1/result.json`) and a normal headless
launch through real VLC with exit 0
(`build/launcher-real-vlc-headless-v1/result-final.json`). Its SHA-256 is
`03b884dd431a60bcfa7749bd9fab056f21955b438a12908645b9b44d4787c10f`.
The AMD64 import audit finds only kernel32.dll, shell32.dll, user32.dll and
version.dll; the new guard adds no non-system runtime dependency.

The guard is Windows-specific. These tools made no Mac connection, antivirus
setting change, external analysis upload or release/tag replacement. Desktop
Defender automatic reporting was not verified before these runs; see the
privacy follow-up in VALIDATION-LIVE-MENU.md. Final
installer/Defender checks must use binaries rebuilt with this guard. This is
not evidence that a historical kernel/driver crash or Defender detection is
resolved.

The subsequent rebuilt draft passes 12 guard cases and 14 actual NSIS checks,
including installed-launcher verification for all eight selections. See
VALIDATION-PACKAGING-GUARD.md for its distinct hashes and local-only Defender
results. These drafts are not promoted to a final release.
