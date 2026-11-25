[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Prepare','Restore')][string]$Action,
    [Parameter(Mandatory=$true)][string]$StateFile
)
# Test-machine privacy setup only. Never shipped or called by an installer.
# Requires the owner's approval. Does not disable antivirus, real-time
# protection, behavior monitoring, tamper protection, or add exclusions.
$ErrorActionPreference='Stop'
$statePath=[IO.Path]::GetFullPath($StateFile)
if ($Action -eq 'Prepare') {
    if (Test-Path -LiteralPath $statePath) { throw 'A saved state already exists; inspect it before another test.' }
    $status=Get-MpComputerStatus
    if (!$status.AntivirusEnabled -or !$status.RealTimeProtectionEnabled -or !$status.BehaviorMonitorEnabled) {
        throw 'The required local Defender protections are not enabled.'
    }
    $before=Get-MpPreference
    $parent=Split-Path -Parent $statePath
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    [pscustomobject]@{
        ComputerName=$env:COMPUTERNAME
        MAPSReporting=[int]$before.MAPSReporting
        SubmitSamplesConsent=[int]$before.SubmitSamplesConsent
        CreatedUtc=[DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
    Set-MpPreference -SubmitSamplesConsent 2
    Set-MpPreference -MAPSReporting 0
    $after=Get-MpPreference
    if ([int]$after.SubmitSamplesConsent -ne 2 -or [int]$after.MAPSReporting -ne 0) {
        throw 'Windows did not apply the local-only settings. No installer may be transferred or run.'
    }
} else {
    $before=Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ($before.ComputerName -ne $env:COMPUTERNAME) { throw 'Saved settings belong to a different computer.' }
    Set-MpPreference -MAPSReporting ([int]$before.MAPSReporting)
    Set-MpPreference -SubmitSamplesConsent ([int]$before.SubmitSamplesConsent)
}
$status=Get-MpComputerStatus
if (!$status.AntivirusEnabled -or !$status.RealTimeProtectionEnabled -or !$status.BehaviorMonitorEnabled) {
    throw 'A required local protection is unexpectedly disabled; stop testing.'
}
Get-MpPreference | Select-Object MAPSReporting,SubmitSamplesConsent | ConvertTo-Json
