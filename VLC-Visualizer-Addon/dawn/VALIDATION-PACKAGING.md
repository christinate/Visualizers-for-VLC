# Native candidate packaging — Windows combined installer

Candidate source version: `0.5.0-rc.1`. Checked on 2026-11-16. This is a
development candidate; the remaining checklist still gates release.

## Layout and provenance

`tools/release.py` now stages explicit native file plans from
`sdk/native_packaging.py`. It does not call the browser packager. The combined
NSIS installer includes plugin-only Geiss, MilkDrop and Terrain components,
with one shared `VisualizerNative`/Dawn/SDL/FXC runtime. Component selections
are included in the existing reversible VLC patch journal, so restoration
also covers the selected bundled plugin files. Nested standalone installers
are no longer embedded or executed.

The packager checks the pinned source-built Dawn provenance and actual DLL
hashes, the pinned SDL SDK archive and its extracted AMD64 DLL, and stages
runtime/plugin licenses and notices. All 36 staged PE images pass normal and
delay-import inspection. The private Qt VLC backend explicitly requires the
selected installed VLC core; no VLC core is redistributed. Static inspection
does not establish every dynamic load or real rendering path. The final
clean-machine player/GPU checks remain open.

Fresh staging/output directories are required. A repeated invocation against
the existing staging directory was refused before creating output or changing
any staged hash. `tools/check_preserved_releases.py` verifies all 62 recorded
pre-migration artifacts, metadata/checksum files and prior tags unchanged.
Their original security-held artifacts and QA results remain separate from
the native candidate.

## Bounded installer and Defender test

Exact test installer:

`f731d180baf824f45533d5f2214346a2ee250b8ca4bf354155b09cb74d33d5ef`

`tools/Test-NativeCombinedInstaller.ps1` passed 14 checks on owner machine
`vpntest1` between 20:21:39 and 20:27:33 UTC:

- All eight combinations of the three plugin selections, including default
  all and none, with installed journal hashes and actual scanner results.
- One installed Dawn DLL for every selection.
- Actual NSIS uninstall and byte-identical original-file restoration in all
  eight cases; owned selected plugin files are removed.
- Modified Terrain DLL causes refusal before any installed file is changed;
  restoring its bytes permits ordinary uninstall.
- An in-use VLC executable triggers rollback with all components selected;
  originals are unchanged and no patch temporary files remain.
- Four unknown/repeated/empty component-list forms fail before mutation.

The test used isolated copies of VLC 3.0.24 files. The machine's installed
VLC 3.0.23 was untouched. No real player playback or GPU presentation was
exercised by this filesystem test.

Defender signature `1.463.329.0` performed a custom installer scan, with
antivirus, real-time protection and behavior monitoring enabled throughout.
Owner-authorized settings were verified as MAPS 0 / sample consent 2 before
transferring any candidate binaries. There were no detections during the
bounded test. All remote test copies were removed before restoring and
verifying saved MAPS 2 / sample consent 1; protections stayed enabled. No
sample or diagnostic report was submitted to a third party. This result does
not resolve the historical 0.2 installer detection or predict a cloud verdict.

Private local evidence is under `build/native-combined-installer-vpntest1-0.5`.
The tested installer is under `build/native-candidate-0.5-windows`; it is not
promoted to a final release directory.

## Measured draft sizes

| Measurement | Bytes |
| --- | ---: |
| Combined native NSIS installer | 20,149,163 |
| Preserved Electron NSIS candidate | 654,552,819 |
| Extracted native payload, including bootstrap/manifest | 69,991,890 |
| Manifest-owned installed core files | 61,223,349 |
| Manifest-owned core plus all three plugins | 69,360,974 |
| Prism Flow / Classic component | 1,409,479 |
| MilkDrop component | 6,325,257 |
| Terrain component | 402,889 |

Installed figures exclude NSIS-generated uninstallers, backup originals,
settings, journals and filesystem allocation. Peak temporary disk use is not
yet measured. These figures describe this exact draft, not the eventual final
release; D55 remains open.

## Remaining work

Independent Windows renderer/Winamp installers now use native transactions;
see VALIDATION-RENDERER-INSTALLERS.md for the later exact artifacts and results.
Linux/macOS production release scripts still require migration. Do not use
those old scripts as final native release builders. D45/D50-D55/D64 remain open until the
complete package matrix and exact final artifacts pass their checks. Real
VLC/Winamp playback, native HDR capability/fallback, soaks, unresolved NVK
findings and final committed-source provenance remain separate gates. Mac
follow-ups remain deferred under M01-M09 until the owner restores availability.
