param(
    [string]$RepoRoot = 'D:\Projects\Git\AmneThron',
    [string]$Repository = 'gravitolet/amnethron',
    [string]$Version = '',
    [string]$InstallerPath = '',
    [Parameter(Mandatory = $true)]
    [string]$ReleaseNotesPath,
    [string]$TargetCommitish = '',
    [switch]$Prerelease,
    [switch]$Draft,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$publisher = Join-Path $RepoRoot 'script\publish_github_release.ps1'
if (-not (Test-Path -LiteralPath $publisher)) {
    throw "GitHub release publisher not found: $publisher"
}

$params = @{
    Repository = $Repository
    ReleaseNotesPath = $ReleaseNotesPath
}
if ($Version) { $params.Version = $Version }
if ($InstallerPath) { $params.InstallerPath = $InstallerPath }
if ($TargetCommitish) { $params.TargetCommitish = $TargetCommitish }
if ($Prerelease) { $params.Prerelease = $true }
if ($Draft) { $params.Draft = $true }
if ($ValidateOnly) { $params.ValidateOnly = $true }

& $publisher @params
