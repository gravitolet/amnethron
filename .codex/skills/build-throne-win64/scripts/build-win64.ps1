param(
    [string]$RepoRoot = 'D:\Projects\Git\AmneThron',
    [string]$BuildDir = (Join-Path $env:LOCALAPPDATA 'CodexBuild\AmneThron-win64'),
    [string]$Configuration = 'RelWithDebInfo',
    [int]$Parallel = 4,
    [switch]$Clean,
    [switch]$Run,
    [switch]$StopExisting,
    [int]$StartProfileId = -1,
    [switch]$ProxyTest,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'

function Stop-StagedThrone {
    param([string]$StageDir)

    foreach ($name in 'Throne', 'ThroneCore') {
        Get-Process $name -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -like "$StageDir*" } |
            Stop-Process -Force
    }
}

function Set-TestSettings {
    param(
        [string]$DbPath,
        [int]$ProfileId,
        [bool]$Remember
    )

    if (-not (Test-Path -LiteralPath $DbPath)) {
        throw "Portable DB not found: $DbPath"
    }

    $rememberValue = if ($Remember) { 'true' } else { 'false' }
    $rememberId = if ($Remember) { "$ProfileId" } else { '-1919' }

    $script = @"
import sqlite3
path = r'''$DbPath'''
updates = {
    'remember_enable': '$rememberValue',
    'remember_id': '$rememberId',
    'tun_mode_enabled': 'false',
    'system_proxy_enabled': 'false',
    'system_dns_set': 'false',
    'enable_tun_routing': 'false',
    'active_routing': 'Default',
}
con = sqlite3.connect(path)
for key, value in updates.items():
    con.execute('insert into settings(key,value) values(?,?) on conflict(key) do update set value=excluded.value', (key, value))
con.commit()
con.close()
"@
    $script | python -
}

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$StageDir = Join-Path $RepoRoot 'deployment\windows-amd64'
$StageExe = Join-Path $StageDir 'Throne.exe'
$StageCore = Join-Path $StageDir 'ThroneCore.exe'
$DbPath = Join-Path $StageDir 'config\throne.db'
$QtCMakePrefix = Join-Path $RepoRoot '.qt\6.10.1\msvc2022_64\lib\cmake'
$OpenSslRoot = Join-Path $RepoRoot '.tools\openssl-x64\openssl'
$OpenSslInclude = Join-Path $OpenSslRoot 'include'
$OpenSslCryptoLib = Join-Path $OpenSslRoot 'lib\libcrypto.lib'
$OpenSslSslLib = Join-Path $OpenSslRoot 'lib\libssl.lib'
$SrsListUrl = 'https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h'
$SrsListPath = Join-Path $BuildDir 'srslist.h'
$SrsListTempPath = Join-Path $BuildDir 'srslist.h.tmp'

if (-not (Test-Path -LiteralPath $StageCore)) {
    throw "Required runtime core is missing: $StageCore"
}

$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $cmake)) { throw "CMake not found: $cmake" }
if (-not (Test-Path -LiteralPath $vsDevCmd)) { throw "VsDevCmd not found: $vsDevCmd" }
if (-not (Test-Path -LiteralPath $QtCMakePrefix)) { throw "Qt CMake prefix not found: $QtCMakePrefix" }
if (-not (Test-Path -LiteralPath $OpenSslCryptoLib)) { throw "OpenSSL crypto lib not found: $OpenSslCryptoLib" }
if (-not (Test-Path -LiteralPath $OpenSslSslLib)) { throw "OpenSSL ssl lib not found: $OpenSslSslLib" }

if ($ValidateOnly) {
    Write-Output "RepoRoot: $RepoRoot"
    Write-Output "BuildDir: $BuildDir"
    Write-Output "StageExe: $StageExe"
    Write-Output "StageCore: $StageCore"
    Write-Output "QtCMakePrefix: $QtCMakePrefix"
    Write-Output "OpenSslRoot: $OpenSslRoot"
    return
}

if ($StopExisting) {
    Stop-StagedThrone -StageDir $StageDir
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    $resolvedBuild = (Resolve-Path -LiteralPath $BuildDir).Path
    if ($resolvedBuild -notlike (Join-Path $env:LOCALAPPDATA 'CodexBuild*')) {
        throw "Refusing to clean unexpected build directory: $resolvedBuild"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
$oldErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & curl.exe -fsSL --retry 3 --connect-timeout 20 -o $SrsListTempPath $SrsListUrl
    $curlExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $oldErrorActionPreference
}
if ($curlExitCode -ne 0 -or -not (Test-Path -LiteralPath $SrsListTempPath)) {
    $cachedSrsList = Get-Item -LiteralPath $SrsListPath -ErrorAction SilentlyContinue
    if ($cachedSrsList -and $cachedSrsList.Length -gt 0) {
        Write-Warning "Failed to download srslist.h from $SrsListUrl; using cached $SrsListPath"
        Remove-Item -LiteralPath $SrsListTempPath -Force -ErrorAction SilentlyContinue
    } else {
        throw "Failed to download srslist.h from $SrsListUrl and no cached file exists at $SrsListPath"
    }
} else {
    if ((Test-Path -LiteralPath $SrsListPath) -and
        ((Get-FileHash -Algorithm SHA256 -LiteralPath $SrsListPath).Hash -eq (Get-FileHash -Algorithm SHA256 -LiteralPath $SrsListTempPath).Hash)) {
        Remove-Item -LiteralPath $SrsListTempPath -Force
    } else {
        Move-Item -LiteralPath $SrsListTempPath -Destination $SrsListPath -Force
    }
}

$buildCmd = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && `"$cmake`" -G `"Ninja`" -S `"$RepoRoot`" -B `"$BuildDir`" -DCMAKE_BUILD_TYPE=$Configuration -DCMAKE_PREFIX_PATH=`"$QtCMakePrefix`" -DOPENSSL_ROOT_DIR=`"$OpenSslRoot`" -DOPENSSL_INCLUDE_DIR=`"$OpenSslInclude`" -DOPENSSL_CRYPTO_LIBRARY=`"$OpenSslCryptoLib`" -DOPENSSL_SSL_LIBRARY=`"$OpenSslSslLib`" && `"$cmake`" --build `"$BuildDir`" --target Throne --parallel $Parallel"
Push-Location $RepoRoot
try {
    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & cmd.exe /c $buildCmd
        $buildExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($buildExitCode -ne 0) { throw "Build failed with exit code $buildExitCode" }
} finally {
    Pop-Location
}

$versionText = (Get-Content -LiteralPath (Join-Path $RepoRoot 'VERSION.txt') -Raw).Trim()
$fileVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($StageExe)
$numericReleaseVersion = ($versionText -split '[-+]')[0]
$releaseParts = $numericReleaseVersion.Split('.')
$expectedResourceVersion = if ($releaseParts.Count -eq 3) { "$numericReleaseVersion.0" } else { $numericReleaseVersion }
if ($fileVersion.FileVersion -ne $expectedResourceVersion) {
    throw "Throne.exe Windows resource version '$($fileVersion.FileVersion)' does not encode release '$versionText'."
}

Write-Output "Built: $StageExe"
Write-Output "ReleaseVersion: $versionText"
Write-Output "Core: $StageCore"

if ($Run) {
    if ($StartProfileId -ge 0) {
        Set-TestSettings -DbPath $DbPath -ProfileId $StartProfileId -Remember $true
    } elseif (Test-Path -LiteralPath $DbPath) {
        Set-TestSettings -DbPath $DbPath -ProfileId -1 -Remember $false
    }

    $proc = Start-Process -FilePath $StageExe -WorkingDirectory $StageDir -PassThru
    Start-Sleep -Seconds 12

    $core = Get-Process ThroneCore -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $StageCore }
    if (-not $core) {
        throw "ThroneCore did not start from $StageCore"
    }
    Write-Output "Started Throne PID=$($proc.Id), ThroneCore PID=$($core.Id)"

    if ($ProxyTest) {
        $response = & curl.exe --connect-timeout 10 --max-time 20 -x socks5h://127.0.0.1:2080 http://ip-api.com/json
        Write-Output $response
    }

    if ($StartProfileId -ge 0 -and (Test-Path -LiteralPath $DbPath)) {
        Set-TestSettings -DbPath $DbPath -ProfileId -1 -Remember $false
    }
}
