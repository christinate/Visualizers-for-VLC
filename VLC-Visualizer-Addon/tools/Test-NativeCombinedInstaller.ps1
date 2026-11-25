[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$FixtureZip,
    [Parameter(Mandatory=$true)][string]$PayloadZip,
    [Parameter(Mandatory=$true)][string]$WorkDirectory,
    [Parameter(Mandatory=$true)][string]$ReportPath,
    [switch]$VerifyOriginalLauncher
)
# Owner-local test machine only. Works on copies, never the installed VLC.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$preference=Get-MpPreference
$protection=Get-MpComputerStatus
if ([int]$preference.MAPSReporting -ne 0 -or [int]$preference.SubmitSamplesConsent -ne 2 -or
    !$protection.AntivirusEnabled -or !$protection.RealTimeProtectionEnabled -or !$protection.BehaviorMonitorEnabled) {
    throw 'Prepare and verify owner-authorized local-only Defender mode before transferring/running a candidate.'
}
$registry='HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCVisualizerAddon'
if (Test-Path $registry) { throw 'An addon is already installed; refusing to share its uninstall registry key.' }
$work=[IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $work) { throw 'Use a new, isolated work directory.' }
New-Item -ItemType Directory -Path $work | Out-Null
$start=Get-Date
$results=New-Object 'System.Collections.Generic.List[object]'
function Hash([string]$File) { (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash.ToLowerInvariant() }
function Snapshot([string]$Directory) {
    $values=@{}
    Get-ChildItem -LiteralPath $Directory -Recurse -File | ForEach-Object {
        $values[$_.FullName.Substring($Directory.Length+1)]=Hash $_.FullName
    }
    return $values
}
function New-Fixture([string]$Name) {
    $directory=Join-Path $work $Name
    Expand-Archive -LiteralPath $FixtureZip -DestinationPath $directory
    return $directory
}
function Install([string]$Directory,[string]$Options='') {
    $process=Start-Process -FilePath $Installer -ArgumentList "/S $Options /D=$Directory" -PassThru -Wait -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw "Installer failed with $($process.ExitCode)." }
}
function Uninstall([string]$Directory) {
    $uninstaller=Join-Path $Directory 'visualizers\Uninstall-Addon.exe'
    Start-Process -FilePath $uninstaller -ArgumentList '/S' -Wait -WindowStyle Hidden
    $deadline=(Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $journal=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $Directory '.visualizers-backup\transaction.json') -Raw | ConvertFrom-Json
        if ($journal.status -eq 'restored' -and !(Test-Path -LiteralPath $uninstaller)) { return }
        Start-Sleep -Milliseconds 100
    }
    throw 'The actual NSIS uninstall process did not finish restoration/removal.'
}
function Assert-Original([string]$Directory,[hashtable]$Before) {
    foreach ($name in $Before.Keys) {
        if ((Hash (Join-Path $Directory $name)) -ne $Before[$name]) { throw "Original differs: $name" }
    }
    if (Test-Path -LiteralPath (Join-Path $Directory 'vlc.visualizers.original.exe')) { throw 'Reserved original-executable copy remains.' }
    if (Test-Path $registry) { throw 'Uninstall registry key remains.' }
}
try {
    Start-MpScan -ScanType CustomScan -ScanPath $Installer
    # All eight combinations, including ordinary default selection and none.
    for ($mask=0;$mask -lt 8;$mask++) {
        $directory=New-Fixture "selection-$mask"
        $before=Snapshot $directory
        $options=@();$expected=@()
        $ids=@('geiss','terrain','milkdrop')
        $switches=@('/WITHOUTGEISS','/WITHOUTTERRAIN','/WITHOUTMILKDROP')
        for ($index=0;$index -lt 3;$index++) {
            if (($mask -band (1 -shl $index)) -ne 0) { $expected += $ids[$index] }
            else { $options += $switches[$index] }
        }
        Install $directory ($options -join ' ')
        if ($VerifyOriginalLauncher) {
            # The launcher is a GUI-subsystem executable. PowerShell's direct
            # invocation can return before exit and leave LASTEXITCODE stale.
            $verification=Start-Process -FilePath (Join-Path $directory 'vlc.exe') -ArgumentList '--verify-original' -PassThru -Wait -WindowStyle Hidden
            if ($verification.ExitCode -ne 0) { throw 'Installed launcher rejected its backed-up VLC/core.' }
        }
        $control=Join-Path $directory 'visualizers\VisualizersCtl.exe'
        & $control verify --root $directory
        if ($LASTEXITCODE -ne 0) { throw 'Installed files failed their journal hashes.' }
        $scan=& $control scan --suite (Join-Path $directory 'visualizers') | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0 -or $scan.rejected.Count -ne 0) { throw 'Scanner rejected the installed selection.' }
        $actual=@($scan.visualizers | ForEach-Object { $_.id })
        if ((($expected | Sort-Object) -join ',') -ne (($actual | Sort-Object) -join ',')) { throw 'Installed component selection differs from requested selection.' }
        $dawn=@(Get-ChildItem -LiteralPath $directory -Filter webgpu_dawn.dll -Recurse -File)
        if ($dawn.Count -ne 1) { throw 'Expected one installed Dawn runtime.' }
        Uninstall $directory
        Assert-Original $directory $before
        foreach ($id in $expected) {
            if (Test-Path -LiteralPath (Join-Path $directory "visualizers\plugins\$id\manifest.json")) { throw 'Owned plugin remains after uninstall.' }
        }
        $results.Add([pscustomobject]@{test='NSIS selection/install/scan/exact restoration';mask=$mask;ids=$expected;result='pass'})
        Write-Output "Passed native selection $mask."
    }
    # Refuse removal of a changed bundled module before removing any file.
    $directory=New-Fixture 'modified'
    $before=Snapshot $directory
    Install $directory
    $module=Join-Path $directory 'visualizers\plugins\terrain\terrain_dawn.dll'
    $saved=[IO.File]::ReadAllBytes($module)
    $stream=[IO.File]::Open($module,[IO.FileMode]::Append,[IO.FileAccess]::Write)
    try { $stream.WriteByte(0x5a) } finally { $stream.Dispose() }
    $snapshot=Snapshot $directory
    # _?= avoids NSIS relaunch so the refusal exit status is observable.
    $process=Start-Process -FilePath (Join-Path $directory 'visualizers\Uninstall-Addon.exe') -ArgumentList "/S _?=$directory\visualizers" -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -eq 0) { throw 'Modified-file uninstall unexpectedly succeeded.' }
    $after=Snapshot $directory
    if ($after.Count -ne $snapshot.Count) { throw 'A file changed during modified-file refusal.' }
    foreach ($name in $snapshot.Keys) { if ($after[$name] -ne $snapshot[$name]) { throw "Refusal changed $name" } }
    [IO.File]::WriteAllBytes($module,$saved)
    Uninstall $directory
    Assert-Original $directory $before
    $results.Add([pscustomobject]@{test='modified bundled module refusal before any removal';result='pass'})

    Expand-Archive -LiteralPath $PayloadZip -DestinationPath (Join-Path $work 'payload')
    $control=Join-Path $work 'payload\bootstrap\VisualizersCtl.exe'
    $directory=New-Fixture 'locked'
    $before=Snapshot $directory
    $held=[IO.File]::Open((Join-Path $directory 'vlc.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        & $control patch --root $directory --payload (Join-Path $work 'payload') --components 'geiss,terrain,milkdrop'
        if ($LASTEXITCODE -eq 0) { throw 'Locked-file patch unexpectedly succeeded.' }
    } finally { $held.Dispose() }
    Assert-Original $directory $before
    if (@(Get-ChildItem -LiteralPath $directory -Recurse -Filter '*.patch.tmp' -File).Count -ne 0) { throw 'Patch temporary files remain.' }
    $results.Add([pscustomobject]@{test='in-use destination rollback with all native components';result='pass'})
    foreach ($selection in @('foreign','geiss,geiss','geiss,','geiss,,terrain')) {
        $directory=New-Fixture ('invalid-'+$results.Count)
        $before=Snapshot $directory
        & $control patch --root $directory --payload (Join-Path $work 'payload') --components $selection
        if ($LASTEXITCODE -eq 0) { throw 'Invalid component selection was accepted.' }
        $after=Snapshot $directory
        if ($after.Count -ne $before.Count) { throw 'Invalid selection changed files.' }
        foreach ($name in $before.Keys) { if ($after[$name] -ne $before[$name]) { throw 'Invalid selection changed originals.' } }
        $results.Add([pscustomobject]@{test='invalid selection rejected before mutation';selection=$selection;result='pass'})
    }
    $detections=@(Get-MpThreatDetection | Where-Object { $_.InitialDetectionTime -ge $start })
    if ($detections.Count -ne 0) { throw 'Defender reported a detection during this bounded test; preserve local evidence.' }
    $afterProtection=Get-MpComputerStatus
    if (!$afterProtection.AntivirusEnabled -or !$afterProtection.RealTimeProtectionEnabled -or !$afterProtection.BehaviorMonitorEnabled) {
        throw 'Required Defender protections changed during testing.'
    }
    [ordered]@{result='pass';installer_sha256=Hash $Installer;installer_bytes=(Get-Item -LiteralPath $Installer).Length;
        machine=$env:COMPUTERNAME;started=$start.ToUniversalTime().ToString('o');finished=[DateTime]::UtcNow.ToString('o');
        defender_signature=$afterProtection.AntivirusSignatureVersion;defender_local_only=$true;
        original_launcher_guard_verified=[bool]$VerifyOriginalLauncher;checks=$results;
        limits='Bounded silent filesystem/NSIS test on isolated VLC copies. No real player playback, HDR or indefinite stability claim. Historical Defender runtime detection remains unresolved.'
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    Write-Output "Passed $($results.Count) checks."
} catch {
    [ordered]@{result='fail';error=$_.Exception.Message;completed_checks=$results;installer_sha256=Hash $Installer} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    throw
}
