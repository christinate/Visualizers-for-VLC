$ErrorActionPreference='Stop'
$global:ProgressPreference='SilentlyContinue'
$base=Join-Path $env:USERPROFILE 'visualizer-release-checks'
$preference=Get-MpPreference
$status=Get-MpComputerStatus
if($preference.MAPSReporting -ne 0 -or $preference.SubmitSamplesConsent -ne 2 -or !$status.RealTimeProtectionEnabled -or !$status.BehaviorMonitorEnabled){throw 'Local-only Defender with real-time/behavior protection is required.'}
$started=Get-Date
$results=@()
function Run-Setup([string]$exe,[string[]]$arguments,[bool]$success=$true){
 $process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
 if(($process.ExitCode -eq 0) -ne $success){throw "Unexpected installer exit: $($process.ExitCode)"}
}
foreach($package in @(@{Name='PrismFlow64';Id='geiss'},@{Name='MilkDrop2-64';Id='milkdrop'},@{Name='Terrain64';Id='terrain'})){
 $name=$package.Name;$id=$package.Id;$installer=Join-Path $base "$name-0.4.0-rc.1-setup.exe"
 $key="HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$name"
 if(Test-Path -LiteralPath $key){throw "Existing package preserved: $name"}
 # A synthetic registration verifies refusal before changing installed files.
 $guardRoot=Join-Path $base "renderer-guard-$id"
 try{
  New-Item -Path $key -Force | Out-Null
  New-ItemProperty -Path $key -Name DisplayVersion -Value '0.3.0-rc.1' -PropertyType String -Force | Out-Null
  Run-Setup $installer @('/S','/MODE=STANDALONE',('/D='+$guardRoot)) $false
  if(Test-Path -LiteralPath (Join-Path $guardRoot 'Visualizers.exe')){throw 'Upgrade guard wrote a runtime before refusing.'}
 }finally{Remove-Item -LiteralPath $key -ErrorAction SilentlyContinue}
 if($id -eq 'geiss'){
  $legacy='HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Geiss64'
  if(Test-Path -LiteralPath $legacy){throw 'Pre-existing legacy registration preserved.'}
  try{
   New-Item -Path $legacy -Force | Out-Null
   New-ItemProperty -Path $legacy -Name DisplayVersion -Value '0.3.0-rc.1' -PropertyType String -Force | Out-Null
   Run-Setup $installer @('/S','/MODE=STANDALONE',('/D='+$guardRoot)) $false
  }finally{Remove-Item -LiteralPath $legacy -ErrorAction SilentlyContinue}
 }
 foreach($mode in @('STANDALONE','WINAMP-x86','WINAMP-x64')){
  $root=Join-Path $base "renderer-$id-$mode"
  if(Test-Path -LiteralPath $root){throw 'A prior renderer test must be preserved.'}
  $arguments=@('/S',('/MODE='+$(if($mode -eq 'STANDALONE'){'STANDALONE'}else{'WINAMP'})))
  $winamp=''
  if($mode -ne 'STANDALONE'){
   $arch=$mode.Split('-')[1];$winamp=Join-Path $base "synthetic-winamp-$id-$arch"
   New-Item -ItemType Directory -Path $winamp | Out-Null
   # Only the installer architecture route is under test; no player is executed.
   Copy-Item -LiteralPath (Join-Path $base "probe-$arch.exe") -Destination (Join-Path $winamp 'winamp.exe')
   $arguments+=('/WINAMPROOT='+$winamp)
  }
  $arguments+=('/D='+$root);Run-Setup $installer $arguments
  $ctl=Join-Path $root 'VisualizersCtl.exe'
  $scan=(& $ctl scan --suite $root) | ConvertFrom-Json
  if($LASTEXITCODE -ne 0 -or $scan.visualizers.Count -ne 1 -or $scan.visualizers[0].id -ne $id){throw 'Standalone runtime catalog mismatch.'}
  if(!(Test-Path -LiteralPath (Join-Path $root 'browser\runtime\electron.exe'))){throw 'Recording/renderer runtime missing.'}
  if($winamp){
   $bridge=Join-Path $winamp "Plugins\vis_${id}_bridge.dll"
   & $ctl architecture --file $bridge | Out-Null
   $expected=if($mode -eq 'WINAMP-x64'){1}else{0}
   if($LASTEXITCODE -ne $expected){throw 'Incorrect Winamp bridge architecture.'}
   $registered=(Get-ItemProperty -LiteralPath "HKLM:\Software\VLCVisualizers\Renderers\$id").Host
   if($registered -ne (Join-Path $root 'Visualizers.exe')){throw 'Bridge host registration mismatch.'}
  }
  Set-Content -LiteralPath (Join-Path $root 'user-preset.txt') -Value 'preserve user files'
  Run-Setup (Join-Path $root 'Uninstall.exe') @('/S',('_?='+$root))
  if(Test-Path -LiteralPath $key){throw 'Uninstall registration remains.'}
  if(Test-Path -LiteralPath (Join-Path $root 'Visualizers.exe')){throw 'Owned runtime remains after uninstall.'}
  if(!(Test-Path -LiteralPath (Join-Path $root 'user-preset.txt'))){throw 'An unowned file was deleted.'}
  if($winamp -and (Test-Path -LiteralPath $bridge)){throw 'Winamp bridge remains after uninstall.'}
  $results+=@{package=$name;mode=$mode;result='pass'}
 }
}
$events=@(Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Windows Defender/Operational';Id=1116,1117;StartTime=$started} -ErrorAction SilentlyContinue | Select-Object TimeCreated,Id,Message)
$report=@{result=$(if($events.Count){'detected'}else{'pass-local-only'});cases=$results;defender_events=$events;note='Synthetic Winamp executable layouts test installer routing only; real player UI remains separate.'}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $base 'renderer-installer-results.json') -Encoding UTF8
if($events.Count){exit 11}
exit 0
