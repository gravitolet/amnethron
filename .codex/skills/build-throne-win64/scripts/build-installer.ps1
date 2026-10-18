param(
    [string]$RepoRoot = 'D:\Projects\Git\AmneThron',
    [string]$BuildDir = (Join-Path $env:LOCALAPPDATA 'CodexBuild\AmneThron-win64'),
    [string]$Configuration = 'RelWithDebInfo',
    [ValidateSet('Major', 'Minor', 'Patch', 'Build')]
    [string]$Increment = 'Patch',
    [string]$MakensisPath = '',
    [switch]$Clean,
    [switch]$StopExisting
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$installerScript = Join-Path $RepoRoot 'script\build_windows_installer.ps1'
if (-not (Test-Path -LiteralPath $installerScript)) {
    throw "Installer build script not found: $installerScript"
}

$params = @{
    BuildDir = $BuildDir
    Configuration = $Configuration
    Increment = $Increment
}

if ($MakensisPath) {
    $params.MakensisPath = $MakensisPath
}
if ($Clean) {
    $params.Clean = $true
}
if ($StopExisting) {
    $params.StopExisting = $true
}

& $installerScript @params
