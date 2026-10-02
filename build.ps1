<#
.SYNOPSIS
  Local Windows build script for the modernized HRCBot Metamod:Source plugin.

.DESCRIPTION
  Builds hrcbot_mm.dll (x86) and hrcbot_mm.x64.dll (x64) for Half-Life 2:
  Deathmatch using MSVC and AMBuild.  Run from a "x64 Native Tools Command
  Prompt for Visual Studio" (the CI uses ilammy/msvc-dev-cmd which sets this
  up automatically).

.PARAMETER Arch
  Target architecture: x86, x64 or all (default: all).

.PARAMETER Setup
  Clone hl2sdk (hl2dm) and metamod-source (1.12-dev) into .\deps.

.EXAMPLE
  .\build.ps1 -Setup
  .\build.ps1
  .\build.ps1 -Arch x64
#>
param(
    [ValidateSet('x86', 'x64', 'all')]
    [string]$Arch = 'all',
    [switch]$Setup
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$DepsDir   = Join-Path $Root 'deps'
$Hl2Sdk    = if ($env:HL2SDK_HL2DM) { $env:HL2SDK_HL2DM } else { Join-Path $DepsDir 'hl2sdk-hl2dm' }
$MmsPath   = if ($env:MMS_PATH) { $env:MMS_PATH } else { Join-Path $DepsDir 'metamod-source' }
$BuildDir  = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $Root 'obj-windows' }

if ($Setup) {
    New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null
    if (-not (Test-Path $Hl2Sdk)) {
        Write-Host '[setup] cloning hl2sdk (hl2dm)...'
        git clone --depth 1 -b hl2dm https://github.com/alliedmodders/hl2sdk.git $Hl2Sdk
    }
    if (-not (Test-Path $MmsPath)) {
        Write-Host '[setup] cloning metamod-source (1.12-dev)...'
        git clone --depth 1 -b 1.12-dev https://github.com/alliedmodders/metamod-source.git $MmsPath
    }
}

if (-not (Test-Path (Join-Path $Hl2Sdk 'lib\public\x64\tier1.lib'))) {
    throw "hl2sdk hl2dm not found at $Hl2Sdk. Run .\build.ps1 -Setup or set HL2SDK_HL2DM."
}
if (-not (Test-Path (Join-Path $MmsPath 'core'))) {
    throw "metamod-source not found at $MmsPath. Run .\build.ps1 -Setup or set MMS_PATH."
}

# AMBuild / Python.
python -c "import ambuild2" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'AMBuild not found; installing with pip...'
    python -m pip install ambuild
}

$AmArch = switch ($Arch) { 'x86' { 'x86' } 'x64' { 'x64' } default { 'x86,x64' } }

if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

Write-Host "[configure] arch=$AmArch"
python "$Root\configure.py" --sdks hl2dm `
    --hl2sdk-hl2dm "$Hl2Sdk" `
    --mms-path "$MmsPath" `
    --target-arch "$AmArch"

Write-Host '[build] ambuild'
ambuild

Write-Host ''
Write-Host 'Build finished. Artifacts:'
Get-ChildItem -Recurse -Filter 'hrcbot_mm*.dll' | ForEach-Object { $_.FullName }
