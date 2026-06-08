param(
    [string]$VersionFile = (Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..')).Path 'VERSION.txt'),
    [ValidateSet('Major', 'Minor', 'Patch', 'Build')]
    [string]$Part = 'Patch',
    [switch]$NoWrite
)

$versionPath = (Resolve-Path -LiteralPath $VersionFile -ErrorAction Stop).Path
$version = (Get-Content -LiteralPath $versionPath -Raw).Trim()

if ($version -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?$') {
    throw "Invalid version '$version' in $versionPath. Expected MAJOR.MINOR.PATCH[.BUILD]."
}

$major = [int]$Matches[1]
$minor = [int]$Matches[2]
$patch = [int]$Matches[3]
$build = if ($Matches[4]) { [int]$Matches[4] } else { $null }

switch ($Part) {
    'Major' {
        $major++
        $minor = 0
        $patch = 0
        $build = $null
    }
    'Minor' {
        $minor++
        $patch = 0
        $build = $null
    }
    'Patch' {
        $patch++
        $build = $null
    }
    'Build' {
        if ($null -eq $build) { $build = 0 }
        $build++
    }
}

$next = if ($null -eq $build) {
    "$major.$minor.$patch"
} else {
    "$major.$minor.$patch.$build"
}

if (-not $NoWrite) {
    Set-Content -LiteralPath $versionPath -Value $next -NoNewline
}
Write-Output $next
