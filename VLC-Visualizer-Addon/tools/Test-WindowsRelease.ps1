[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Version,[string]$RunId='')
$ErrorActionPreference='Stop'
$global:ProgressPreference='SilentlyContinue'
$base=Join-Path $env:USERPROFILE 'visualizer-release-checks'
$installer=Join-Path $base "VLC-Visualizer-Addon-$Version-setup.exe"
$preference=Get-MpPreference
$status=Get-MpComputerStatus
if ($preference.MAPSReporting -ne 0 -or $preference.SubmitSamplesConsent -ne 2) { throw 'Local-only privacy settings are required.' }
if (!$status.AntivirusEnabled -or !$status.RealTimeProtectionEnabled -or !$status.BehaviorMonitorEnabled) { throw 'Local protection is not fully enabled.' }
$started=Get-Date
$report=[ordered]@{version=$Version;started=$started.ToUniversalTime().ToString('o');installer_sha256=(Get-FileHash -LiteralPath $installer).Hash;local_only=$true;protections_enabled=$true;signature_version=$status.AntivirusSignatureVersion;checks=@()}
$run=$Version+$(if($RunId){'-'+$RunId}else{''})
$test=Join-Path $base $run
if (Test-Path -LiteralPath $test) { throw 'A test directory already exists; preserve its evidence.' }
$names=@('Geiss64','PrismFlow64','MilkDrop2-64','Terrain64','VLCVisualizerAddon')
foreach($name in $names) { if(Test-Path -LiteralPath "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$name") { throw "A pre-existing package must be preserved: $name" } }
$zip=Join-Path $base 'vlc-3.0.24-win64.zip'
if ((Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant() -ne 'fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323') { throw 'Official VLC ZIP checksum mismatch.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($zip,$test)
$root=Join-Path $test 'vlc-3.0.24'
$original=@{}
foreach($name in @('vlc.exe','libvlccore.dll','plugins\gui\libqt_plugin.dll')) { $original[$name]=(Get-FileHash -LiteralPath (Join-Path $root $name)).Hash }
Start-MpScan -ScanType CustomScan -ScanPath $installer
$process=Start-Process -FilePath $installer -ArgumentList @('/S',('/D='+$root)) -WindowStyle Hidden -Wait -PassThru
$report.install_exit=$process.ExitCode
if ($process.ExitCode -ne 0) { throw "Installer failed with $($process.ExitCode)" }
$ctl=Join-Path $root 'visualizers\VisualizersCtl.exe'
& $ctl verify --root $root
if ($LASTEXITCODE -ne 0) { throw 'Installed payload verification failed.' }
$report.checks+= 'NSIS install and payload hashes'
$catalog=Get-Content -LiteralPath (Join-Path $root 'visualizers\installed-visualizers.json') -Raw | ConvertFrom-Json
$report.visualizers=@($catalog.visualizers | ForEach-Object {$_.id})
$expected=if($Version.StartsWith('0.3.')){2}else{3}
if ($report.visualizers.Count -ne $expected) { throw 'A bundled default component is missing.' }
$report.checks+='default bundled components'
$scan=Start-Process -FilePath (Join-Path $root 'vlc.exe') -ArgumentList '--scan-visualizers' -WindowStyle Hidden -Wait -PassThru
if ($scan.ExitCode -ne 0) { throw 'VLC scan command failed.' }
$report.checks+='VLC scan command'
# Child components must be removed before their containing addon runtime.
foreach($name in @('Geiss64','PrismFlow64','MilkDrop2-64','Terrain64')) {
 $path=Join-Path $env:ProgramFiles "$name\Uninstall.exe"
 if(Test-Path -LiteralPath $path) {
  $uninstall=Start-Process -FilePath $path -ArgumentList @('/S',('_?='+(Split-Path -Parent $path))) -WindowStyle Hidden -Wait -PassThru
  if($uninstall.ExitCode -ne 0) { throw "Component uninstall failed: $name" }
 }
}
$uninstaller=Join-Path $root 'visualizers\Uninstall-Addon.exe'
$process=Start-Process -FilePath $uninstaller -ArgumentList @('/S',('_?='+(Split-Path -Parent $uninstaller))) -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0) { throw 'Addon uninstall failed.' }
foreach($name in $original.Keys) { if((Get-FileHash -LiteralPath (Join-Path $root $name)).Hash -ne $original[$name]) { throw "Original restoration mismatch: $name" } }
$report.checks+='byte-identical VLC executable, Qt module and core restoration'
foreach($name in $names) { if(Test-Path -LiteralPath "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$name") { throw "Uninstall registration remains: $name" } }
$report.checks+='uninstall registrations removed'
$report.defender_events=@(Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Windows Defender/Operational';Id=1116,1117;StartTime=$started} -ErrorAction SilentlyContinue | Select-Object TimeCreated,Id,Message)
$report.threats=@(Get-MpThreatDetection | Where-Object {$_.InitialDetectionTime -ge $started} | Select-Object ThreatID,InitialDetectionTime,ActionSuccess,Resources)
$report.result=if($report.defender_events.Count -eq 0 -and $report.threats.Count -eq 0){'pass-local-only'}else{'detected'}
$report.ended=(Get-Date).ToUniversalTime().ToString('o')
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $base "result-$run.json") -Encoding UTF8
$report | ConvertTo-Json -Depth 8
if($report.result -ne 'pass-local-only'){exit 11}
