param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$versionHeader = Join-Path $projectRoot 'src\config\version.h'
$buildDirectory = Join-Path $projectRoot '.pio\build\waveshare-esp32s3-touch-amoled-164'
$firmwareDirectory = Join-Path $projectRoot 'docs\firmware'
$otaManifest = Join-Path $projectRoot 'docs\ota.json'
$webManifest = Join-Path $projectRoot 'docs\manifest.json'

$versionText = Get-Content -Raw -LiteralPath $versionHeader
$versionMatch = [regex]::Match($versionText, '#define\s+FIRMWARE_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+)"')
if (-not $versionMatch.Success) {
    throw "Could not read FIRMWARE_VERSION from $versionHeader"
}
$version = $versionMatch.Groups[1].Value

if (-not $SkipBuild) {
    $platformio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
    if (-not (Test-Path -LiteralPath $platformio)) {
        throw "PlatformIO was not found at $platformio"
    }
    & $platformio run --project-dir $projectRoot
    if ($LASTEXITCODE -ne 0) { throw "PlatformIO build failed with exit code $LASTEXITCODE" }
}

$builtFirmware = Join-Path $buildDirectory 'firmware.bin'
if (-not (Test-Path -LiteralPath $builtFirmware)) {
    throw "Built firmware was not found at $builtFirmware"
}

$versionedName = "pourbot-$version.bin"
$versionedFirmware = Join-Path $firmwareDirectory $versionedName
Copy-Item -LiteralPath $builtFirmware -Destination $versionedFirmware -Force
Copy-Item -LiteralPath $builtFirmware -Destination (Join-Path $firmwareDirectory 'firmware.bin') -Force

$ota = [ordered]@{
    version = $version
    url = "https://kalendor.github.io/PourBot/firmware/$versionedName"
}
$ota | ConvertTo-Json | Set-Content -LiteralPath $otaManifest -Encoding utf8

$manifest = Get-Content -Raw -LiteralPath $webManifest | ConvertFrom-Json
$manifest.version = $version
$appPart = $manifest.builds[0].parts | Where-Object { $_.offset -eq 65536 }
if (-not $appPart) { throw 'Could not find the application entry in docs/manifest.json' }
$appPart.path = "firmware/$versionedName"
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $webManifest -Encoding utf8

$publishedVersion = (Get-Content -Raw -LiteralPath $otaManifest | ConvertFrom-Json).version
$publishedPath = (Get-Content -Raw -LiteralPath $webManifest | ConvertFrom-Json).builds[0].parts |
    Where-Object { $_.offset -eq 65536 } |
    Select-Object -ExpandProperty path
if ($publishedVersion -ne $version -or $publishedPath -ne "firmware/$versionedName") {
    throw 'OTA metadata verification failed'
}

Write-Host "Prepared PourBot $version for OTA publishing."
Write-Host "Commit and push docs/ota.json, docs/manifest.json, and docs/firmware/$versionedName."
