param(
    [string]$BuildDir = 'build-local',
    [string]$Configuration = 'RelWithDebInfo',
    [ValidateSet('Major', 'Minor', 'Patch', 'Build')]
    [string]$Increment = 'Patch',
    [string]$MakensisPath = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$versionFile = Join-Path $repoRoot 'VERSION.txt'
$version = & (Join-Path $PSScriptRoot 'bump_version.ps1') -Part $Increment -NoWrite
$versionParts = $version.Split('.')
if ($versionParts.Count -eq 3) {
    $versionResource = "$version.0"
} elseif ($versionParts.Count -eq 4) {
    $versionResource = $version
} else {
    throw "Invalid bumped version '$version'."
}

if (-not $MakensisPath) {
    $cmd = Get-Command makensis -ErrorAction SilentlyContinue
    if ($cmd) {
        $MakensisPath = $cmd.Source
    } elseif (Test-Path 'C:\Program Files (x86)\NSIS\makensis.exe') {
        $MakensisPath = 'C:\Program Files (x86)\NSIS\makensis.exe'
    } elseif (Test-Path 'C:\Program Files (x86)\NSIS\Bin\makensis.exe') {
        $MakensisPath = 'C:\Program Files (x86)\NSIS\Bin\makensis.exe'
    } else {
        throw 'makensis.exe was not found. Install NSIS or pass -MakensisPath.'
    }
}

$buildPath = Join-Path $repoRoot $BuildDir
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path $cmake)) { throw "CMake not found: $cmake" }
if (-not (Test-Path $vsDevCmd)) { throw "VsDevCmd not found: $vsDevCmd" }

$env:INPUT_VERSION = $version
$buildCmd = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && `"$cmake`" -S `"$repoRoot`" -B `"$buildPath`" -DCMAKE_BUILD_TYPE=$Configuration && `"$cmake`" --build `"$buildPath`" --target Throne --parallel 4"
Push-Location $repoRoot
try {
    & cmd.exe /c $buildCmd
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

    $deployDir = Join-Path $repoRoot 'deployment\windows-amd64'
    Copy-Item -LiteralPath (Join-Path $buildPath 'Throne.exe') -Destination (Join-Path $deployDir 'Throne.exe') -Force
    Copy-Item -LiteralPath (Join-Path $buildPath 'Throne.pdb') -Destination (Join-Path $deployDir 'Throne.pdb') -Force -ErrorAction SilentlyContinue

    & $MakensisPath "/DAPP_VERSION=$version" "/DAPP_VERSION_RESOURCE=$versionResource" (Join-Path $repoRoot 'script\windows_installer.nsi')
    if ($LASTEXITCODE -ne 0) { throw "makensis failed with exit code $LASTEXITCODE" }

    Set-Content -LiteralPath $versionFile -Value $version -NoNewline

    $installerDir = Join-Path $repoRoot 'deployment\installer'
    New-Item -ItemType Directory -Path $installerDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot 'ThroneSetup.exe') -Destination (Join-Path $installerDir 'ThroneSetup.exe') -Force
} finally {
    Pop-Location
}

Write-Output "Built installer version $version"
