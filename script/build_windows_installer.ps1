param(
    [string]$BuildDir = (Join-Path $env:LOCALAPPDATA 'CodexBuild\AmneThron-win64'),
    [string]$Configuration = 'RelWithDebInfo',
    [ValidateSet('Major', 'Minor', 'Patch', 'Build')]
    [string]$Increment = 'Patch',
    [string]$MakensisPath = '',
    [switch]$Clean,
    [switch]$StopExisting
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

if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $buildPath = $BuildDir
} else {
    $buildPath = Join-Path $repoRoot $BuildDir
}
$qtCMakePrefix = Join-Path $repoRoot '.qt\6.10.1\msvc2022_64\lib\cmake'
$opensslRoot = Join-Path $repoRoot '.tools\openssl-x64\openssl'
$opensslInclude = Join-Path $opensslRoot 'include'
$opensslCryptoLib = Join-Path $opensslRoot 'lib\libcrypto.lib'
$opensslSslLib = Join-Path $opensslRoot 'lib\libssl.lib'
$srsListUrl = 'https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h'
$srsListPath = Join-Path $buildPath 'srslist.h'
$srsListTempPath = Join-Path $buildPath 'srslist.h.tmp'
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path $cmake)) { throw "CMake not found: $cmake" }
if (-not (Test-Path $vsDevCmd)) { throw "VsDevCmd not found: $vsDevCmd" }
if (-not (Test-Path $qtCMakePrefix)) { throw "Qt CMake prefix not found: $qtCMakePrefix" }
if (-not (Test-Path $opensslCryptoLib)) { throw "OpenSSL crypto lib not found: $opensslCryptoLib" }
if (-not (Test-Path $opensslSslLib)) { throw "OpenSSL ssl lib not found: $opensslSslLib" }

if ($Clean -and (Test-Path -LiteralPath $buildPath)) {
    $resolvedBuildPath = (Resolve-Path -LiteralPath $buildPath).Path
    if ($resolvedBuildPath -notlike (Join-Path $env:LOCALAPPDATA 'CodexBuild*')) {
        throw "Refusing to clean unexpected build directory: $resolvedBuildPath"
    }
    Remove-Item -LiteralPath $resolvedBuildPath -Recurse -Force
}

if ($StopExisting) {
    $deployDir = Join-Path $repoRoot 'deployment\windows-amd64'
    Get-Process Throne,ThroneCore -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -like "$deployDir*" } |
        Stop-Process -Force
}

New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
$oldErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & curl.exe -fsSL --retry 3 --connect-timeout 20 -o $srsListTempPath $srsListUrl
    $curlExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $oldErrorActionPreference
}
if ($curlExitCode -ne 0 -or -not (Test-Path -LiteralPath $srsListTempPath)) {
    $cachedSrsList = Get-Item -LiteralPath $srsListPath -ErrorAction SilentlyContinue
    if ($cachedSrsList -and $cachedSrsList.Length -gt 0) {
        Write-Warning "Failed to download srslist.h from $srsListUrl; using cached $srsListPath"
        Remove-Item -LiteralPath $srsListTempPath -Force -ErrorAction SilentlyContinue
    } else {
        throw "Failed to download srslist.h from $srsListUrl and no cached file exists at $srsListPath"
    }
} else {
    if ((Test-Path -LiteralPath $srsListPath) -and
        ((Get-FileHash -Algorithm SHA256 -LiteralPath $srsListPath).Hash -eq (Get-FileHash -Algorithm SHA256 -LiteralPath $srsListTempPath).Hash)) {
        Remove-Item -LiteralPath $srsListTempPath -Force
    } else {
        Move-Item -LiteralPath $srsListTempPath -Destination $srsListPath -Force
    }
}

$env:INPUT_VERSION = $version
$buildCmd = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && `"$cmake`" -G `"Ninja`" -S `"$repoRoot`" -B `"$buildPath`" -DCMAKE_BUILD_TYPE=$Configuration -DCMAKE_PREFIX_PATH=`"$qtCMakePrefix`" -DOPENSSL_ROOT_DIR=`"$opensslRoot`" -DOPENSSL_INCLUDE_DIR=`"$opensslInclude`" -DOPENSSL_CRYPTO_LIBRARY=`"$opensslCryptoLib`" -DOPENSSL_SSL_LIBRARY=`"$opensslSslLib`" && `"$cmake`" --build `"$buildPath`" --target Throne --parallel 4"
Push-Location $repoRoot
try {
    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & cmd.exe /c $buildCmd
        $buildExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($buildExitCode -ne 0) { throw "CMake build failed with exit code $buildExitCode" }

    $deployDir = Join-Path $repoRoot 'deployment\windows-amd64'
    Copy-Item -LiteralPath (Join-Path $buildPath 'Throne.exe') -Destination (Join-Path $deployDir 'Throne.exe') -Force
    Copy-Item -LiteralPath (Join-Path $buildPath 'Throne.pdb') -Destination (Join-Path $deployDir 'Throne.pdb') -Force -ErrorAction SilentlyContinue

    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $MakensisPath "/DAPP_VERSION=$version" "/DAPP_VERSION_RESOURCE=$versionResource" "/DPROJECT_ROOT=$repoRoot" (Join-Path $repoRoot 'script\windows_installer.nsi')
        $makensisExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($makensisExitCode -ne 0) { throw "makensis failed with exit code $makensisExitCode" }

    $installerDir = Join-Path $repoRoot 'deployment\installer'
    New-Item -ItemType Directory -Path $installerDir -Force | Out-Null
    $versionedInstaller = Join-Path $installerDir "ThroneSetup-$version.exe"
    $unversionedInstaller = Join-Path $repoRoot 'ThroneSetup.exe'
    $builtInstaller = Get-Item -LiteralPath $unversionedInstaller -ErrorAction SilentlyContinue
    if (-not $builtInstaller -or $builtInstaller.Length -eq 0) {
        throw "makensis did not produce a non-empty installer: $unversionedInstaller"
    }

    Copy-Item -LiteralPath $unversionedInstaller -Destination $versionedInstaller -Force
    $finalInstaller = Get-Item -LiteralPath $versionedInstaller -ErrorAction SilentlyContinue
    if (-not $finalInstaller -or $finalInstaller.Length -ne $builtInstaller.Length) {
        throw "Failed to create the final versioned installer: $versionedInstaller"
    }

    Remove-Item -LiteralPath (Join-Path $installerDir 'ThroneSetup.exe') -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $unversionedInstaller -Force -ErrorAction SilentlyContinue

    # Consume the release version only after the final versioned installer exists.
    Set-Content -LiteralPath $versionFile -Value $version -NoNewline

} finally {
    Pop-Location
}

Write-Output "Built installer version $version"
Write-Output $versionedInstaller
Write-Output "Publish this installer with script\publish_github_release.ps1 after committing and pushing the matching source/version."
