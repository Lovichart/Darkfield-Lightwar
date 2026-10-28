# Build a self-contained Windows x64 distribution.
# Usage: powershell -ExecutionPolicy Bypass -File .\package.ps1
param(
    [string]$Version = 'v2.2.0'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$game = Join-Path $root 'Darkfield Lightwar'
$distRoot = Join-Path $root 'dist'
$name = "Darkfield Lightwar $Version win64"
$dist = Join-Path $distRoot $name

Write-Host '=== Build Release x64 ==='
& (Join-Path $root 'build.ps1')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $root 'x64\Release\Darkfield Lightwar.exe'
if (-not (Test-Path $exe)) { throw "Missing exe: $exe" }

if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force $dist | Out-Null

Write-Host "=== Copy runtime to $dist ==="
Copy-Item $exe $dist
foreach ($dll in @('sfml-graphics-2.dll', 'sfml-window-2.dll', 'sfml-system-2.dll')) {
    Copy-Item (Join-Path $game $dll) $dist
}

# App-local MSVC runtime: makes the folder work without a system VC++ install.
$crt = 'H:\VS2022\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT'
foreach ($dll in @('msvcp140.dll', 'msvcp140_1.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    $src = Join-Path $crt $dll
    if (Test-Path $src) { Copy-Item $src $dist }
    else { Write-Warning "CRT DLL not found, system runtime will be required: $src" }
}

# Assets: only the OFL font is redistributed; proprietary system fonts are not.
New-Item -ItemType Directory -Force (Join-Path $dist 'fonts') | Out-Null
Copy-Item (Join-Path $game 'fonts\LXGWWenKaiTC-Light.ttf') (Join-Path $dist 'fonts')
Copy-Item (Join-Path $game 'names') (Join-Path $dist 'names') -Recurse

Copy-Item (Join-Path $game '玩家说明.md') (Join-Path $dist 'README.md')
Copy-Item (Join-Path $game '沙盘说明书.txt') $dist
Copy-Item (Join-Path $game '更新日志.md') $dist
Copy-Item (Join-Path $root 'LICENSE') $dist
Copy-Item (Join-Path $root 'LICENSE-ASSETS') $dist
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') $dist
Copy-Item (Join-Path $root 'licenses') (Join-Path $dist 'licenses') -Recurse
Copy-Item (Join-Path $root 'licenses\SFML.txt') (Join-Path $dist 'SFML-LICENSE.md')

$play = @'
@echo off
cd /d "%~dp0"
start "" "Darkfield Lightwar.exe" %*
'@
Set-Content -Path (Join-Path $dist 'Play.bat') -Value $play -Encoding ASCII

Write-Host '=== Verify layout ==='
Get-ChildItem $dist | Select-Object Name, Length | Out-String | Write-Host

$zip = Join-Path $distRoot "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Write-Host "=== Compress $zip ==="
Compress-Archive -Path $dist -DestinationPath $zip -CompressionLevel Optimal
Get-Item $zip | Select-Object FullName, @{n = 'MB'; e = { [math]::Round($_.Length / 1MB, 1) } }
