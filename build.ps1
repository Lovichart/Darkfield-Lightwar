# Darkfield Lightwar build helper
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\build.ps1
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Bench 10000 -Seconds 8
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Run
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -SfmlRoot C:\SFML-2.6.2
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$Run,
    [int]$Bench = 0,
    [int]$Seconds = 8,
    [string]$SfmlRoot = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$sln = Join-Path $root 'Darkfield Lightwar.sln'

$msbuild = 'H:\VS2022\MSBuild\Current\Bin\amd64\MSBuild.exe'
if (-not (Test-Path $msbuild)) {
    $vswhere = "$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($install) { $msbuild = Join-Path $install 'MSBuild\Current\Bin\amd64\MSBuild.exe' }
    }
}
if (-not (Test-Path $msbuild)) {
    throw "MSBuild not found. Edit build.ps1 and set `$msbuild."
}

Write-Host "Building $Configuration x64 ..."
$msbuildArgs = @($sln, "/p:Configuration=$Configuration", "/p:Platform=x64", "/m", "/v:minimal")
if ($SfmlRoot) { $msbuildArgs += "/p:SFML_ROOT=$SfmlRoot" }
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $root "x64\$Configuration\Darkfield Lightwar.exe"
if (-not (Test-Path $exe)) { throw "Build finished but exe not found: $exe" }

if ($Bench -gt 0) {
    & $exe --bench $Bench --bench-seconds $Seconds --bench-asteroids 400
    exit $LASTEXITCODE
}

if ($Run) {
    Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe)
    Write-Host "Started: $exe"
}
