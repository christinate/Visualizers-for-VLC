# Native Windows renderer installer validation

Checked 2026-11-16. Source candidate `0.5.0-rc.1`; development drafts only.
The Mac return checklist remains deferred. No artifact was uploaded for
analysis, no Git release was published, and prior release artifacts/tags remain
preserved.

## Implementation

`sdk/renderer-release.py` stages one native runtime, its renderer/plugin
assets and both Winamp bridges. It verifies the pinned SDK and Dawn/SDL
dependency hashes, stages notices and checks normal/delay PE imports. There
are 10/11/9 PE images in the Prism Flow/MilkDrop/Terrain payloads, respectively;
only the explicit x86 Winamp bridge is 32-bit. No Electron, Chromium, Node or
N-API executable/resource enters these Windows payloads.

The standalone portable archives now contain a usable runtime plus plugin,
rather than separated installer extraction directories. The Windows NSIS
template uses a native multi-root transaction for standalone, VLC and Winamp
files. Standalone/Winamp selections share one runtime per product installation;
VLC-only installs add just the plugin to the existing shared addon runtime.

All destinations and source hashes are checked before publishing any file.
Existing destinations are refused. Files are staged, checked again and
published without replacement. Rollback tracks only files it actually
published, including a competing identical file appearing after preflight.
Generated shortcuts are staged privately and included in the journal with
their actual hashes. Uninstall binds the journal to trusted payload metadata,
checks roots and all owned files, and obtains deletion access before removal.
Modified/in-use files preserve the transaction for repair. Same-ID NSIS
install/uninstall operations are serialized across the file/registry work.
Existing products must be uninstalled before upgrading.

The combined patch also refuses taking over files already owned by an
independent renderer component. Its ordinary VLC file backups/restoration
remain part of the separate core patch transaction.

## Definitive findings

- The new hash reader initially passed mixed separators to the extended
  Windows namespace. The first remote test failed opening that path. The
  reader now normalizes separators; an explicit regression plus Unicode and
  paths longer than 260 characters pass in the native transaction tests.
- The inherited NSIS default-selection pattern jumped over only part of an
  expanded `UnselectSection` macro. The V2 draft passed six explicit selections
  but selected only standalone when both players were available by default.
  The V3 template uses complete conditional blocks. All default/present and
  default/absent player cases pass afterward. Failed-draft evidence is retained
  locally; its candidate artifacts were not replaced.

## Bounded evidence

On owner machine `vpntest1`:

- **34 native filesystem cases:** all seven component combinations against
  x86/x64 PE fixtures, occupied/modified/locked destinations, root/journal
  binding, nine controlled publication faults, a competing identical target,
  generated shortcut preservation, mixed separators and Unicode/long paths.
- **36 actual NSIS cases:** all seven selections for each renderer, an
  additional x64 Winamp-only case per renderer, defaults with players absent,
  installed hashes, one/zero local runtime as appropriate, scanner persistence,
  actual shortcut targets/arguments, host registration, owned-file removal,
  occupied destinations, same-version reinstall refusal, modified module/
  bridge/shortcut refusal, an in-use bridge, concurrent same-ID installs and
  addon refusal of an independently owned VLC plugin.
- An additional actual Terrain NSIS install/hash/uninstall passes with spaces
  and Japanese characters in the installation directory. This does not claim
  NSIS support for every arbitrarily long installation directory.
- The rebuilt combined installer passes all **14** existing core installer
  checks, including eight plugin selections, exact original restoration,
  modified-file refusal, in-use rollback and invalid component lists.

The Winamp executables in these installer checks are minimal architecture
fixtures and are never launched. Actual Winamp playback/UI and complete
rendering/capture behavior remain separate release gates. Controlled fault
tests do not prove arbitrary power-loss recovery or indefinite stability.

All three independent repositories also compile their updated control tool
from their own SDK snapshot. All four source snapshots match 123 indexed
hashes. The shared runtime/render algorithms are unchanged by this packaging
work.

## Exact draft installers and measured sizes

| Draft | NSIS bytes | Portable ZIP bytes | Installer SHA-256 |
| --- | ---: | ---: | --- |
| Prism Flow V3 | 5,357,025 | 7,590,699 | `716c59d161932cc19eb5b00dadbe55b6523bc5882ebc88f219317f595443e5a9` |
| MilkDrop V3 | 7,005,636 | 9,597,721 | `1dbe49daf46ba6b190ceb1b5d05b97b0eb3196b1a49da1390ec2b6d59e6786f9` |
| Terrain V3 | 5,095,009 | 7,128,763 | `5c528315ed6108c8078cebd64837ff371a59311f42b21a3db7d8870b57209f6e` |
| Combined V2 | 20,174,468 | — | `3cfbc98017b00dc6099cf2f0a517c2354d573045daac56a70158688d1e6d866c` |

Individual drafts are under each repository's
`build/native-candidate-0.5-windows-v3`. The combined draft is under the
addon's `build/native-candidate-0.5-windows-v2`. These are not final committed-
source release artifacts. Complete installed/temporary allocation accounting
and final candidate provenance still keep D55/D72-D74 open.

## Local-only Defender and cleanup

The exact installers were transferred only after verifying the previously
authorized MAPS 0 / sample consent 2 mode. Antivirus, real-time protection and
behavior monitoring remained enabled. Signature `1.463.329.0` completed nine
custom scans across the combined/failed/fixed candidate runs. There were no
new threat events or threat detections during the full test session. This
does not determine a cloud verdict or resolve the historical 0.2 detection.

Tests uninstalled all product/package/bridge registrations. All remote test
copies were removed before restoring and verifying saved MAPS 2 / sample
consent 1; protections stayed enabled. The deliberately long native fixture
needed literal extended-path .NET cleanup because PowerShell 5's recursive
provider failed on it. Cleanup then completed. No audio was recorded.

Private reports, failed V2 evidence, scan events and restoration confirmation
are under `build/native-renderer-installers-vpntest1` on the owner workstation.

## Remaining gates

Windows packaging now uses the native path, but Linux/Mac production staging
still needs migration. D45/D50-D55/D64 remain open for the complete platform
matrix, final installers and outstanding fault/interactive checks. Real
VLC/Winamp playback, HDR capability/fallback, soaks, unresolved NVK findings,
licenses/source provenance and final Git release preparation remain open.
