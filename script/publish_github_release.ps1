param(
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

function Get-GitHubToken {
    foreach ($name in 'GH_TOKEN', 'GITHUB_TOKEN') {
        $value = [Environment]::GetEnvironmentVariable($name)
        if (-not [string]::IsNullOrWhiteSpace($value)) { return $value.Trim() }
    }

    $credentialInput = "protocol=https`nhost=github.com`n`n"
    $credentialLines = $credentialInput | git credential fill 2>$null
    if ($LASTEXITCODE -ne 0) { return '' }
    foreach ($line in $credentialLines) {
        if ($line -like 'password=*') { return $line.Substring('password='.Length) }
    }
    return ''
}

function Invoke-GitHubApi {
    param(
        [Parameter(Mandatory = $true)][string]$Method,
        [Parameter(Mandatory = $true)][string]$Uri,
        [object]$Body,
        [string]$InFile = '',
        [string]$ContentType = 'application/json'
    )

    $headers = @{
        Accept = 'application/vnd.github+json'
        Authorization = "Bearer $script:GitHubToken"
        'User-Agent' = 'AmneThron-release-publisher'
        'X-GitHub-Api-Version' = '2022-11-28'
    }
    $params = @{
        Method = $Method
        Uri = $Uri
        Headers = $headers
        UseBasicParsing = $true
    }
    if ($InFile) {
        $params.InFile = $InFile
        $params.ContentType = $ContentType
    } elseif ($null -ne $Body) {
        $json = $Body | ConvertTo-Json -Depth 10 -Compress
        # Windows PowerShell 5.1 may otherwise encode a string body using the
        # system code page, which makes JSON with Russian release notes invalid.
        $params.Body = [Text.Encoding]::UTF8.GetBytes($json)
        $params.ContentType = 'application/json; charset=utf-8'
    }
    Invoke-RestMethod @params
}

function Get-GitHubAssetUploadUri {
    param(
        [Parameter(Mandatory = $true)][string]$UploadUrlTemplate,
        [Parameter(Mandatory = $true)][string]$AssetName
    )

    $uploadBase = $UploadUrlTemplate -replace '\{\?name,label\}$', ''
    $encodedName = [Uri]::EscapeDataString($AssetName)
    $uri = "${uploadBase}?name=$encodedName"
    if (-not [Uri]::IsWellFormedUriString($uri, [UriKind]::Absolute)) {
        throw "Invalid GitHub asset upload URI: $uri"
    }
    return $uri
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$versionFile = Join-Path $repoRoot 'VERSION.txt'
$versionInSource = (Get-Content -LiteralPath $versionFile -Raw).Trim()
if (-not $Version) {
    $Version = $versionInSource
}
if ($Version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw "Invalid release version '$Version'."
}
if ($Version -ne $versionInSource) {
    throw "VERSION.txt contains '$versionInSource', not release version '$Version'."
}

if (-not $InstallerPath) {
    $InstallerPath = Join-Path $repoRoot "deployment\installer\ThroneSetup-$Version.exe"
}
$InstallerPath = (Resolve-Path -LiteralPath $InstallerPath).Path
$ReleaseNotesPath = (Resolve-Path -LiteralPath $ReleaseNotesPath).Path
$releaseNotes = (Get-Content -LiteralPath $ReleaseNotesPath -Raw -Encoding UTF8).Trim()
if ([string]::IsNullOrWhiteSpace($releaseNotes)) {
    throw "Release notes are empty: $ReleaseNotesPath"
}

$expectedName = "ThroneSetup-$Version.exe"
if ([IO.Path]::GetFileName($InstallerPath) -ne $expectedName) {
    throw "Installer name must be '$expectedName', got '$([IO.Path]::GetFileName($InstallerPath))'."
}

Push-Location $repoRoot
try {
    $dirty = git status --porcelain --untracked-files=no
    if ($dirty) {
        throw 'Tracked source changes are not committed. Commit the matching source and VERSION.txt before publishing.'
    }

    $headSha = (git rev-parse HEAD).Trim()
    if (-not $TargetCommitish) { $TargetCommitish = $headSha }
    $containsHead = git branch -r --contains $headSha
    if (-not ($containsHead | Where-Object { $_ -match '^\s*origin/' })) {
        throw "Commit $headSha is not present on an origin branch. Push it before publishing."
    }
} finally {
    Pop-Location
}

$script:GitHubToken = Get-GitHubToken
if ([string]::IsNullOrWhiteSpace($script:GitHubToken)) {
    throw 'GitHub authentication was not found. Set GH_TOKEN/GITHUB_TOKEN or sign in through Git Credential Manager.'
}

$apiBase = "https://api.github.com/repos/$Repository"
$null = Invoke-GitHubApi -Method Get -Uri $apiBase
$sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $InstallerPath).Hash.ToLowerInvariant()
$body = "$releaseNotes`n`n**SHA-256 (``$expectedName``):** ``$sha256``"
$tag = "v$Version"

if ($ValidateOnly) {
    # Exercise the authenticated UTF-8 JSON POST path without creating or
    # changing a release, so encoding failures are caught during validation.
    $null = Invoke-GitHubApi -Method Post -Uri 'https://api.github.com/markdown' -Body @{
        text = $releaseNotes
        mode = 'gfm'
        context = $Repository
    }
    $null = Get-GitHubAssetUploadUri `
        -UploadUrlTemplate "https://uploads.github.com/repos/$Repository/releases/1/assets{?name,label}" `
        -AssetName $expectedName
    Write-Output "Validated GitHub release $tag for $Repository at $TargetCommitish"
    Write-Output "Installer: $InstallerPath"
    Write-Output "SHA-256: $sha256"
    return
}

$release = $null
try {
    $release = Invoke-GitHubApi -Method Get -Uri "$apiBase/releases/tags/$tag"
} catch {
    $statusCode = $_.Exception.Response.StatusCode.value__
    if ($statusCode -ne 404) { throw }
}

if ($null -eq $release) {
    $release = Invoke-GitHubApi -Method Post -Uri "$apiBase/releases" -Body @{
        tag_name = $tag
        target_commitish = $TargetCommitish
        name = "AmneThron $Version"
        body = $body
        draft = [bool]$Draft
        prerelease = [bool]$Prerelease
        generate_release_notes = $false
    }
} else {
    $release = Invoke-GitHubApi -Method Patch -Uri "$apiBase/releases/$($release.id)" -Body @{
        name = "AmneThron $Version"
        body = $body
        draft = [bool]$Draft
        prerelease = [bool]$Prerelease
    }
}

foreach ($asset in $release.assets) {
    if ($asset.name -eq $expectedName) {
        $null = Invoke-GitHubApi -Method Delete -Uri "$apiBase/releases/assets/$($asset.id)"
    }
}

$assetUploadUri = Get-GitHubAssetUploadUri -UploadUrlTemplate $release.upload_url -AssetName $expectedName
$uploadedAsset = Invoke-GitHubApi -Method Post -Uri $assetUploadUri -InFile $InstallerPath -ContentType 'application/octet-stream'
$installerSize = (Get-Item -LiteralPath $InstallerPath).Length
$expectedDigest = "sha256:$sha256"
if ($uploadedAsset.state -ne 'uploaded' -or $uploadedAsset.size -ne $installerSize) {
    throw "GitHub asset verification failed: state='$($uploadedAsset.state)', size='$($uploadedAsset.size)', expected size='$installerSize'."
}
if ($uploadedAsset.digest -and $uploadedAsset.digest -ne $expectedDigest) {
    throw "GitHub asset digest '$($uploadedAsset.digest)' does not match '$expectedDigest'."
}

Write-Output "Published: $($release.html_url)"
Write-Output "Asset: $expectedName"
Write-Output "SHA-256: $sha256"
