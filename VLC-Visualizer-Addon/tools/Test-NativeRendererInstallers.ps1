[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PlanFile,
    [Parameter(Mandatory=$true)][string]$WorkDirectory,
    [Parameter(Mandatory=$true)][string]$ReportPath
)
# Owner-local, silent NSIS filesystem tests. Synthetic Winamp executables are
# architecture fixtures and are never launched. This is not real playback QA.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$preference=Get-MpPreference;$protection=Get-MpComputerStatus
if ([int]$preference.MAPSReporting -ne 0 -or [int]$preference.SubmitSamplesConsent -ne 2 -or
    !$protection.AntivirusEnabled -or !$protection.RealTimeProtectionEnabled -or !$protection.BehaviorMonitorEnabled) { throw 'Verified local-only Defender mode with active protections is required.' }
$base=Split-Path -Parent ([IO.Path]::GetFullPath($PlanFile))
$plan=Get-Content -Encoding UTF8 -LiteralPath $PlanFile -Raw | ConvertFrom-Json
$work=[IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $work) { throw 'Use a fresh work directory.' }
$addonKey='HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCVisualizerAddon'
if (Test-Path $addonKey) { throw 'An addon already owns the machine uninstall key.' }
$menu=Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::CommonPrograms)) 'VLC Visualizers'
foreach ($product in $plan.products) {
    if (Test-Path ('HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\'+$product.name)) { throw 'An existing renderer owns its product key.' }
    if (Test-Path ('HKLM:\Software\VLCVisualizers\Packages\'+$product.id)) { throw 'An existing renderer owns its package key.' }
    foreach ($suffix in @('.lnk',' settings.lnk')) {
        if (Test-Path -LiteralPath (Join-Path $menu ($product.label+$suffix))) { throw 'An existing Start menu shortcut was preserved; inspect ownership before testing.' }
    }
}
New-Item -ItemType Directory -Path $work | Out-Null
$checks=New-Object 'System.Collections.Generic.List[object]'
$start=Get-Date
$addonInstalled=$false
function Hash([string]$Path) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $algorithm.Dispose();$stream.Dispose() }
}
function Owned-Path($Journal,$Row) { return [IO.Path]::Combine([string]$Journal.roots.($Row.anchor),([string]$Row.path).Replace('/','\')) }
function Owned-Snapshot($Journal) {
    $result=@{}
    foreach ($row in $Journal.files) {
        if (!$row.published) { continue }
        $file=Owned-Path $Journal $row
        if ([IO.File]::Exists($file)) { $result[$file]=Hash $file }
    }
    return $result
}
function Assert-Snapshot([hashtable]$Before) {
    foreach ($file in $Before.Keys) { if (!(Test-Path -LiteralPath $file) -or (Hash $file) -ne $Before[$file]) { throw "A protected file changed: $file" } }
}
function Record([string]$Test,$Product,$Detail) {
    $checks.Add([pscustomobject]@{test=$Test;id=$Product.id;detail=$Detail;result='pass'})
    Write-Output "Passed $($Product.id): $Test $Detail"
}
function Run-Installer([string]$File,[string]$Arguments,[bool]$Success=$true) {
    $process=Start-Process -FilePath $File -ArgumentList $Arguments -Wait -PassThru -WindowStyle Hidden
    if (($process.ExitCode -eq 0) -ne $Success) { throw "Unexpected installer exit $($process.ExitCode): $Arguments" }
}
function Start-Product($Product,[string]$Root,[string]$Options,[string]$Vlc,[string]$Winamp,[bool]$Success=$true) {
    $arguments='/S '+$Options+' /VLCROOT="'+$Vlc+'" /WINAMPROOT="'+$Winamp+'" /D='+$Root
    Run-Installer (Join-Path $base $Product.installer) $arguments $Success
}
function Remove-Product($Product,[string]$Root) {
    Run-Installer (Join-Path $Root 'Uninstall.exe') '/S'
    $deadline=(Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        if (!(Test-Path -LiteralPath (Join-Path $Root 'Uninstall.exe')) -and !(Test-Path ('HKLM:\Software\VLCVisualizers\Packages\'+$Product.id))) { return }
        Start-Sleep -Milliseconds 100
    }
    throw 'NSIS renderer uninstall did not finish.'
}
function Reject-Removal($Product,[string]$Root) {
    $journal=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $Root '.package\transaction.json') -Raw | ConvertFrom-Json
    $before=Owned-Snapshot $journal
    Run-Installer (Join-Path $Root 'Uninstall.exe') ('/S _?='+$Root) $false
    Assert-Snapshot $before
    $after=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $Root '.package\transaction.json') -Raw | ConvertFrom-Json
    if ($after.status -ne 'installed') { throw 'Refusal changed transaction status.' }
    if (!(Test-Path ('HKLM:\Software\VLCVisualizers\Packages\'+$Product.id))) { throw 'Refusal removed registry ownership.' }
}
function Make-Winamp([string]$Directory,[int]$Machine) {
    New-Item -ItemType Directory -Path $Directory | Out-Null
    $bytes=New-Object byte[] 512
    [Array]::Copy([BitConverter]::GetBytes([int16]0x5a4d),0,$bytes,0,2)
    [Array]::Copy([BitConverter]::GetBytes([int32]128),0,$bytes,60,4)
    [Array]::Copy([BitConverter]::GetBytes([int32]0x4550),0,$bytes,128,4)
    [Array]::Copy([BitConverter]::GetBytes([uint16]$Machine),0,$bytes,132,2)
    [IO.File]::WriteAllBytes((Join-Path $Directory 'winamp.exe'),$bytes)
}
try {
    foreach ($artifact in $plan.artifacts) {
        $file=Join-Path $base $artifact.file
        if ((Hash $file) -ne $artifact.sha256) { throw 'Transferred artifact checksum mismatch.' }
        if ([IO.Path]::GetExtension($file) -eq '.exe') { Start-MpScan -ScanType CustomScan -ScanPath $file }
    }
    $vlc=Join-Path $work 'VLC'
    Expand-Archive -LiteralPath (Join-Path $base $plan.fixture) -DestinationPath $vlc
    $originals=@{}
    Get-ChildItem -LiteralPath $vlc -Recurse -File | ForEach-Object { $originals[$_.FullName]=Hash $_.FullName }
    Run-Installer (Join-Path $base $plan.addon) ('/S /WITHOUTGEISS /WITHOUTTERRAIN /WITHOUTMILKDROP /D='+$vlc)
    $addonInstalled=$true
    $win32=Join-Path $work 'Winamp32';$win64=Join-Path $work 'Winamp64'
    Make-Winamp $win32 0x14c;Make-Winamp $win64 0x8664
    $winampOriginals=@{(Join-Path $win32 'winamp.exe')=(Hash (Join-Path $win32 'winamp.exe'));(Join-Path $win64 'winamp.exe')=(Hash (Join-Path $win64 'winamp.exe'))}
    $modes=@{1='/MODE=STANDALONE';2='/MODE=VLC';3='/MODE=ALL /WITHOUTWINAMP';4='/MODE=WINAMP /WITHOUTSTANDALONE';5='/MODE=WINAMP';6='/MODE=ALL /WITHOUTSTANDALONE';7=''}
    foreach ($product in $plan.products) {
        $payload=Join-Path $work ('payload-'+$product.id)
        Expand-Archive -LiteralPath (Join-Path $base $product.payload) -DestinationPath $payload
        foreach ($case in @(@{mask=1;arch='x86'},@{mask=2;arch='x86'},@{mask=3;arch='x86'},@{mask=4;arch='x86'},@{mask=5;arch='x86'},@{mask=6;arch='x86'},@{mask=7;arch='x86'},@{mask=4;arch='x64'})) {
            $mask=$case.mask;$winamp=if ($case.arch -eq 'x64') { $win64 } else { $win32 }
            $root=Join-Path $work ($product.id+'-'+$mask+'-'+$case.arch)
            Start-Product $product $root $modes[$mask] $vlc $winamp
            $journal=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $root '.package\transaction.json') -Raw | ConvertFrom-Json
            $expected=@();if ($mask -band 1) { $expected+='standalone' };if ($mask -band 2) { $expected+='vlc' };if ($mask -band 4) { $expected+='winamp' }
            if ((($expected|Sort-Object)-join ',') -ne (($journal.selected|Sort-Object)-join ',')) { throw 'NSIS selected components differ from expected.' }
            foreach ($row in $journal.files) { if ((Hash (Owned-Path $journal $row)) -ne $row.sha256) { throw 'Installed journal hash differs.' } }
            $localRuntime=(($mask -band 1) -ne 0 -or ($mask -band 4) -ne 0)
            if (@(Get-ChildItem -LiteralPath $root -Filter webgpu_dawn.dll -Recurse -File).Count -ne [int]$localRuntime) { throw 'Local native runtime copy count differs.' }
            if ($mask -band 4) {
                if ($journal.bridge_arch -ne $case.arch) { throw 'Wrong Winamp bridge architecture.' }
                $registered=(Get-ItemProperty ('HKLM:\Software\VLCVisualizers\Renderers\'+$product.id)).Host
                if ($registered -ne (Join-Path $root 'Visualizers.exe')) { throw 'Wrong x64 rendering host registration.' }
            }
            if ($mask -band 1) {
                $shell=New-Object -ComObject WScript.Shell
                $link=$shell.CreateShortcut((Join-Path $menu ($product.label+'.lnk')))
                if ($link.TargetPath -ne (Join-Path $root 'Visualizers.exe') -or $link.Arguments -ne ('--visualizer '+$product.id)) { throw 'Standalone shortcut target/arguments differ.' }
                [void][Runtime.InteropServices.Marshal]::ReleaseComObject($link);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($shell)
            }
            if ($mask -band 2) {
                $scan=& (Join-Path $vlc 'visualizers\VisualizersCtl.exe') scan --suite (Join-Path $vlc 'visualizers') | ConvertFrom-Json
                if ($LASTEXITCODE -ne 0 -or @($scan.visualizers|Where-Object id -eq $product.id).Count -ne 1) { throw 'VLC scanner did not retain the installed visualizer.' }
            }
            $owned=@($journal.files|ForEach-Object { Owned-Path $journal $_ })
            Remove-Product $product $root
            foreach ($file in $owned) { if ([IO.File]::Exists($file)) { throw 'Owned file remains after uninstall.' } }
            Assert-Snapshot $winampOriginals
            Record 'NSIS selection, hashes, registration and removal' $product ($mask.ToString()+'-'+$case.arch)
        }
        $root=Join-Path $work ($product.id+'-default-missing')
        Start-Product $product $root '' (Join-Path $work 'missing-vlc') (Join-Path $work 'missing-winamp')
        $journal=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $root '.package\transaction.json') -Raw | ConvertFrom-Json
        if (($journal.selected -join ',') -ne 'standalone') { throw 'Absent players were not deselected by default.' }
        Remove-Product $product $root
        Record 'default deselection of absent players' $product 'standalone only'
    }
    $product=@($plan.products|Where-Object id -eq 'terrain')[0]
    $bridge=Join-Path $win32 'Plugins\vis_terrain_bridge.dll'
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $bridge) | Out-Null
    [IO.File]::WriteAllText($bridge,'pre-existing user bridge')
    $root=Join-Path $work 'occupied-bridge'
    Start-Product $product $root '/MODE=ALL' $vlc $win32 $false
    if ([IO.File]::ReadAllText($bridge) -ne 'pre-existing user bridge' -or (Test-Path -LiteralPath (Join-Path $root '.package\transaction.json'))) { throw 'Occupied bridge preflight changed files.' }
    Remove-Item -LiteralPath $bridge
    Record 'occupied external bridge refused before mutation' $product ''
    New-Item -ItemType Directory -Force -Path $menu | Out-Null
    $shortcut=Join-Path $menu ($product.label+'.lnk');[IO.File]::WriteAllText($shortcut,'pre-existing user shortcut')
    Start-Product $product (Join-Path $work 'occupied-shortcut') '/MODE=ALL' $vlc $win32 $false
    if ([IO.File]::ReadAllText($shortcut) -ne 'pre-existing user shortcut') { throw 'Existing shortcut was overwritten.' }
    Remove-Item -LiteralPath $shortcut
    Record 'occupied shortcut refused' $product ''
    $root=Join-Path $work 'safety-all'
    Start-Product $product $root '/MODE=ALL' $vlc $win32
    $journal=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $root '.package\transaction.json') -Raw | ConvertFrom-Json
    $before=Owned-Snapshot $journal
    Start-Product $product (Join-Path $work 'blocked-upgrade') '/MODE=STANDALONE' $vlc $win32 $false
    Assert-Snapshot $before
    Record 'occupied product/same-version reinstall refused' $product ''
    foreach ($file in @((Join-Path $root 'plugins\terrain\terrain_dawn.dll'),$bridge,$shortcut)) {
        $saved=[IO.File]::ReadAllBytes($file);$stream=[IO.File]::Open($file,[IO.FileMode]::Append,[IO.FileAccess]::Write)
        try { $stream.WriteByte(0x5a) } finally { $stream.Dispose() }
        Reject-Removal $product $root
        [IO.File]::WriteAllBytes($file,$saved)
        Record 'modified owned file refused before any removal' $product ([IO.Path]::GetFileName($file))
    }
    $held=[IO.File]::Open($bridge,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try { Reject-Removal $product $root } finally { $held.Dispose() }
    Record 'in-use bridge refused before any removal' $product ''
    Remove-Product $product $root
    $a=Join-Path $work 'concurrent-a';$b=Join-Path $work 'concurrent-b';$installer=Join-Path $base $product.installer
    $p1=Start-Process -FilePath $installer -ArgumentList ('/S /MODE=STANDALONE /D='+$a) -PassThru -WindowStyle Hidden
    $p2=Start-Process -FilePath $installer -ArgumentList ('/S /MODE=VLC /VLCROOT="'+$vlc+'" /D='+$b) -PassThru -WindowStyle Hidden
    if (!$p1.WaitForExit(120000) -or !$p2.WaitForExit(120000)) { throw 'A concurrent installer exceeded the observation timeout; inspect its live process before retrying.' }
    $p1.Refresh();$p2.Refresh()
    if (($p1.ExitCode -eq 0) -eq ($p2.ExitCode -eq 0)) { throw 'Concurrent same-ID installers did not select exactly one owner.' }
    $winner=if ($p1.ExitCode -eq 0) { $a } else { $b }
    Remove-Product $product $winner
    Record 'concurrent standalone/VLC installers serialized' $product ''

    # The addon can be removed while an independently owned plugin remains.
    # Its reinstall must refuse taking ownership of that package's files.
    $root=Join-Path $work 'external-vlc-owner'
    Start-Product $product $root '/MODE=VLC' $vlc $win32
    Run-Installer (Join-Path $vlc 'visualizers\Uninstall-Addon.exe') '/S'
    $deadline=(Get-Date).AddSeconds(30)
    while ((Test-Path $addonKey) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path $addonKey) { throw 'Core addon did not uninstall.' }
    $addonInstalled=$false
    $snapshot=@{}
    Get-ChildItem -LiteralPath $vlc -Recurse -File | ForEach-Object { $snapshot[$_.FullName]=Hash $_.FullName }
    Run-Installer (Join-Path $base $plan.addon) ('/S /D='+$vlc) $false
    Assert-Snapshot $snapshot
    if (Test-Path $addonKey) { throw 'Refused addon install claimed registry ownership.' }
    Remove-Product $product $root
    Record 'addon refuses independently owned bundled component' $product ''
    Assert-Snapshot $originals;Assert-Snapshot $winampOriginals
    $detections=@(Get-MpThreatDetection|Where-Object {$_.InitialDetectionTime -ge $start})
    if ($detections.Count) { throw 'Defender reported a detection; preserve local evidence.' }
    $after=Get-MpComputerStatus
    if (!$after.AntivirusEnabled -or !$after.RealTimeProtectionEnabled -or !$after.BehaviorMonitorEnabled) { throw 'A local protection changed.' }
    [ordered]@{result='pass';checks=$checks;artifacts=$plan.artifacts;started=$start.ToUniversalTime().ToString('o');finished=[DateTime]::UtcNow.ToString('o');
        defender_signature=$after.AntivirusSignatureVersion;defender_local_only=$true;
        scope='Actual NSIS plus native filesystem/scanner/shortcut/registration tests. Synthetic Winamp PE fixtures only; no real player playback or HDR/soak claim.'} |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    Write-Output "Passed $($checks.Count) native renderer installer checks."
} catch {
    [ordered]@{result='fail';error=$_.Exception.Message;completed_checks=$checks;addon_installed=$addonInstalled} |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    throw
}
