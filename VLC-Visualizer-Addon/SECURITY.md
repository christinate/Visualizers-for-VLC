# Release security hold: 0.2.0-rc.1

The combined NSIS installer is **blocked for release** following a reported
Microsoft Defender detection on another computer on 2026-11-14. Keep the
affected copy quarantined. Do not add an exclusion, disable protection, or
restore it to retry installation while the detection is unresolved.

## What is established

The screenshots identify `VLC-Visualizer-Addon-0.2.0-rc.1-setup.exe` as the
affected file and process PID 17900, categorized as suspicious behavior with
severity Severe. The detection name is
`Behavior:Win32/DefenseEvasion.A!ml`; the action is Quarantined. The affected
computer's Windows/Defender versions, file hash, detailed events, and exact
installation stage have not been collected. The detection is not established
as a false positive, and a detection label alone does not identify the action
that triggered it.

The exact local release artifact has SHA-256:

```text
e675bc212ad802f85887e92373df75ff3389557887561efd4f0bc04f69ebbabe
```

It matches the original release checksum and is not Authenticode-signed.
The original binary, payload, source archive and Git tag are preserved for
reproduction. Changing a checksum, renaming/repacking a program, or getting a
clean file scan would not resolve this behavioral detection.

## Local check and its limits

On the development computer, a Defender custom file scan of the exact installer
returned 0 and reported no threats. This did **not** execute the installer.
Definitions were 1.463.302.0, engine 1.1.26100.4, platform 4.18.26100.4-0, with
antivirus, real-time protection and behavior monitoring enabled. This result
does not contradict or clear the runtime detection on the affected computer.
`validation/defender-file-scan.json` records this scoped result.

## Reviewed behavior

The NSIS setup requests administrator rights, extracts the patch controller and
two optional renderer installers into its temporary directory, then executes
them. The patch controller backs up the original VLC files, replaces `vlc.exe`
with this project's unsigned launcher, installs a VLC module and renderer host,
and records hashes for restoration. The renderer installers execute that new
launcher to scan package manifests and write uninstall registration.

The reviewed installer/controller/launcher/bridge sources contain no changes
to Defender configuration, security services, exclusions, AMSI, or security
policy. They do not download instructions or payloads. The VLC menu module uses
a thread-specific Windows hook inside VLC's own process to attach its menu;
that code normally runs when VLC starts, rather than during the launcher's
scan-only command. The old Geiss x86 self-modifying warp is excluded from the
native x64 build.

Replacing another application's executable and executing an elevated chain of
temporary helpers are **hypotheses to investigate**, not confirmed causes of
this alert. A source review of these paths is not a malware-analysis verdict
for the affected machine or every bundled dependency.

## Collect affected-computer evidence

`tools/Collect-DefenderDiagnostics.ps1` reads relevant Defender status/events,
the installer identity if still present, VLC file identities, and the patch
journal's status. It saves one local JSON report. It never executes the
installer, restores quarantined files, changes protection, or uploads data.

Review the script first. If local script policy permits it, run it in **Windows
PowerShell 5.1** on the affected computer, with a custom VLC path if necessary:

```powershell
.\Collect-DefenderDiagnostics.ps1 -VlcRoot 'C:\Program Files\VideoLAN\VLC'
```

Do not change execution policy to run it. If scripts are blocked, the relevant
read-only commands can instead be entered directly in Windows PowerShell:

```powershell
Get-MpComputerStatus | Select-Object AMEngineVersion, AntivirusSignatureVersion, AntivirusSignatureLastUpdated
Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Windows Defender/Operational';Id=1116,1117;StartTime=(Get-Date).AddDays(-7)} -MaxEvents 200 | Where-Object {$_.Message -match 'DefenseEvasion|VLC-Visualizer-Addon'} | Format-List TimeCreated,Id,Message
```

A quarantined/missing installer is expected; do not restore it just to compute
its hash. The script records read-access failures instead of modifying system
permissions. Reports can contain usernames and paths; review them before
sharing. Preserve `.visualizers-backup` if installation stopped partway.

## Review and release requirements

The owner declined external submission on 2026-11-14. Keep the installer,
source, and diagnostic reports local. Do not submit them to Microsoft or any
other analysis service without a later explicit authorization. The prepared
notes in `security/defender-submission.md` are retained only as a local draft.
No file has been uploaded and no Microsoft determination has been requested.

Microsoft documents that its
[behavior engine analyzes process trees and behavior sequences](https://www.microsoft.com/security/blog/2019/10/08/in-hot-pursuit-of-elusive-threats-ai-driven-behavior-based-blocking-stops-attacks-in-their-tracks/).
This is why a standalone file scan is insufficient evidence here.

Before publishing a replacement candidate, resolve the detection, review the
installation design (including executable replacement), and test the complete
install/scan/launch/uninstall process in a disposable Windows VM with Defender
enabled. Publisher signing and accurate executable metadata are distribution
work, not a guarantee of a clean antivirus verdict. Do not claim this issue is
fixed until the behavioral installation test has actually passed.

`release-status.json` blocks regenerating this version through the release
tools so its original evidence cannot silently be replaced. Normal source
builds remain available for investigation. The hold is separate from the
earlier rendering, audio, and backup/restore test results.

## Owner follow-up and 0.3 development candidate

The owner reported independently checking VirusTotal, overriding Defender and
successfully installing the original candidate, including its VLC version
check. The supplied VirusTotal text describes a generic Sigma/Sysmon executable
creation rule. File creation is expected during an installer, but that rule
neither identifies the triggering Defender behavior sequence nor resolves the
reported Behavior:Win32/DefenseEvasion.A!ml detection. The affected-machine
installer hash and full Defender event sequence are still unavailable locally.

Version 0.3.0-rc.1 replaces the separate Win32 menu row with VLC's own Qt menu
and adds native Cocoa/Qt integration on macOS/Linux. It has local rendering and
file-restoration tests, but is not a malware-analysis determination or a fix
verified against the original Defender event. It remains a local test candidate
pending the complete Windows installation test with Defender enabled. No
external submission is authorized and no installer or report was uploaded.

## Local-only Windows installation evidence (2026-11-15)

The owner authorized testing on vpntest1 and temporarily disabling cloud
reporting/sample submission for privacy. The separate test helper saved the
original settings, selected MAPSReporting=0 and SubmitSamplesConsent=2, and
verified antivirus, real-time protection and behavior monitoring stayed on.
This helper is not shipped or executed by an installer. No exclusion was added.

The preserved 0.3.0-rc.1 installer, SHA256
`89af0b9e422fab232a30395a150035d549c02f9d1a87131edbeb5d8ff81881e9`,
passed a custom local scan and full NSIS install/uninstall on an official,
hash-verified portable VLC 3.0.24 copy. Both default plugins were registered;
scan worked, uninstall registrations were removed, and VLC executable/core/Qt
module hashes exactly matched their originals afterward. No Defender threat
or detection/remediation event was recorded during the bounded test.
The installed VLC 3.0.23 on vpntest1 was not modified.

This is local-only behavior evidence for that hash and signature version
1.463.313.0. It does not establish a cloud verdict, diagnose the historical
0.2 detection, or clear a differently built installer. The original 0.2 hold
and prohibition on external sample/report submission remain in place.

## Final 0.4 candidate installer

The frozen 0.4.0-rc.1 combined installer SHA256 is
`9f2ed3e19d515524392b81ad7087ea1e8cc201e9fef55c3184270bb640d7d9cd`. Its local-only Defender install/uninstall
test completed successfully with all three default plugins, zero recorded
detections/remediation events, and byte-identical restoration of VLC
executable/core/Qt files. Signatures: 1.463.313.0. The nine-case
standalone/Winamp installer matrix also passed; its Winamp layouts were
synthetic, so real player UI validation remains separate. The raw reports
are retained locally under the ignored build directory, not in releases.
The historical 0.2 hold and all cloud-verdict limitations above still apply.

After collecting the reports locally, the test-created binaries/copies and
empty uninstaller directories were removed from vpntest1. Its exact saved
cloud-reporting/sample preferences were restored, with antivirus, real-time
protection and behavior monitoring all still enabled.
