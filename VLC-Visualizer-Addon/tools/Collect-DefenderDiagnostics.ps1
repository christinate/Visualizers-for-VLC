# Read-only collection: never executes the installer, restores quarantine,
# changes Defender settings, or uploads data. Writes one local JSON report.
[CmdletBinding()]
param(
    [string]$InstallerPath = (Join-Path $env:USERPROFILE 'Downloads\VLC-Visualizer-Addon-0.2.0-rc.1-setup.exe'),
    [string]$VlcRoot = (Join-Path $env:ProgramFiles 'VideoLAN\VLC'),
    [string]$OutputDirectory = (Join-Path (Get-Location).Path 'defender-diagnostics')
)
$ErrorActionPreference = 'Stop'
$collectionErrors = New-Object 'System.Collections.Generic.List[string]'
$expected = 'e675bc212ad802f85887e92373df75ff3389557887561efd4f0bc04f69ebbabe'
function Read-FileIdentity([string]$Path) {
    $value = [ordered]@{ path = $Path; exists = (Test-Path -LiteralPath $Path -PathType Leaf) }
    if ($value.exists) {
        try {
            $value['sha256'] = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
            $value['signature_status'] = [string](Get-AuthenticodeSignature -LiteralPath $Path).Status
            $value['bytes'] = (Get-Item -LiteralPath $Path).Length
        } catch { $collectionErrors.Add('File identity: ' + $_.Exception.Message) }
    }
    return [pscustomobject]$value
}
$installer = Read-FileIdentity $InstallerPath
$report = [ordered]@{
    schema_version = 1
    collected_at = (Get-Date).ToString('o')
    powershell_version = $PSVersionTable.PSVersion.ToString()
    expected_installer_sha256 = $expected
    installer = $installer
    installer_matches_release = $null
    defender = $null
    related_events = @()
    related_detections = @()
    vlc_files = @()
    patch_journal = $null
}
if ($installer.PSObject.Properties['sha256']) { $report.installer_matches_release = ($installer.sha256 -eq $expected) }
try {
    $report.defender = Get-MpComputerStatus | Select-Object AMServiceEnabled, AntivirusEnabled, RealTimeProtectionEnabled, BehaviorMonitorEnabled, AMEngineVersion, AntivirusSignatureVersion, @{n='AntivirusSignatureLastUpdated';e={$_.AntivirusSignatureLastUpdated.ToString('o')}}
} catch { $collectionErrors.Add('Defender status: ' + $_.Exception.Message) }
try {
    $report.related_events = @(Get-WinEvent -FilterHashtable @{
        LogName = 'Microsoft-Windows-Windows Defender/Operational'
        Id = 1116,1117
        StartTime = (Get-Date).AddDays(-7)
    } -MaxEvents 200 | Where-Object {
        $_.Message -match 'DefenseEvasion|VLC-Visualizer-Addon-0\.2\.0-rc\.1-setup\.exe'
    } | Select-Object Id, @{n='TimeCreated';e={$_.TimeCreated.ToString('o')}}, Message)
} catch { $collectionErrors.Add('Defender events: ' + $_.Exception.Message) }
try {
    $report.related_detections = @(Get-MpThreatDetection | Where-Object {
        ($_.Resources -join ' ') -match 'VLC-Visualizer-Addon-0\.2\.0-rc\.1-setup\.exe'
    } | Select-Object ThreatID, ProcessName, Resources, ActionSuccess,
        @{n='InitialDetectionTime';e={$_.InitialDetectionTime.ToString('o')}})
} catch { $collectionErrors.Add('Defender detections: ' + $_.Exception.Message) }
foreach ($name in @('vlc.exe','vlc.visualizers.original.exe','libvlccore.dll','visualizers\VisualizersCtl.exe')) {
    $report.vlc_files += Read-FileIdentity (Join-Path $VlcRoot $name)
}
$journal = Join-Path $VlcRoot '.visualizers-backup\transaction.json'
try {
    if (Test-Path -LiteralPath $journal -PathType Leaf) {
        if ((Get-Item -LiteralPath $journal).Length -gt 1048576) { throw 'Journal exceeds 1 MiB' }
        $value = Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
        $report.patch_journal = [ordered]@{ path=$journal; status=$value.status; version=$value.version; files_recorded=@($value.files).Count }
    }
} catch { $collectionErrors.Add('Patch journal: ' + $_.Exception.Message) }
$report['collection_errors'] = @($collectionErrors.ToArray())
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$destination = Join-Path $OutputDirectory ('defender-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.json')
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $destination -Encoding UTF8
Write-Output ('Saved local diagnostic report: ' + $destination)
Write-Output 'Review file paths and event messages before sharing. Nothing was uploaded or installed.'
