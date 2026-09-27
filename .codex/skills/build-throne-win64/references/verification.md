# Verification and git hygiene

Part of the `build-throne-win64` skill. Level: project.

## Verification

After a build, check:

- `deployment\windows-amd64\Throne.exe` timestamp changed.
- `deployment\windows-amd64\ThroneCore.exe` exists.
- `VERSION.txt`, CMake's compiled `NKR_VERSION`, the app's displayed version, the installer filename, and the GitHub tag all match the same three-part release version. Compare only the first three numeric fields when inspecting the mandatory four-part Windows PE fixed-version resource.
- Installer filename includes the version, for example `deployment\installer\ThroneSetup-0.1.1.exe`.
- GitHub Release `v<version>` exists at `https://github.com/gravitolet/amnethron/releases/tag/v<version>` and targets the pushed matching commit.
- The release body contains the version description and SHA-256, and its `ThroneSetup-<version>.exe` asset hash matches the local installer.
- Installer archive contains `rulesets\geoip-ru.srs`, `rulesets\geosite-ru.srs`, `rulesets\geosite-category-ru.srs`, and `rulesets\geosite-ru-available-only-inside.srs`.

Verify installer contents with 7-Zip when available:

```powershell
$seven = 'C:\Program Files\7-Zip\7z.exe'
$installer = 'D:\Projects\Git\AmneThron\deployment\installer\ThroneSetup-<version>.exe'
$list = & $seven l -slt $installer
$paths = $list | Where-Object { $_ -like 'Path = *' } | ForEach-Object { $_.Substring(7) }
$srs = $paths | Where-Object { $_ -like '*rulesets*' -and $_ -like '*.srs' }
$geoip = $srs | Where-Object { [IO.Path]::GetFileName($_) -like 'geoip*.srs' }
$required = @('geoip-ru.srs','geosite-ru.srs','geosite-category-ru.srs','geosite-ru-available-only-inside.srs')
[pscustomobject]@{
    TotalSrs = $srs.Count
    GeoipSrs = $geoip.Count
    RequiredRuFiles = ($srs | Where-Object { $required -contains [IO.Path]::GetFileName($_) }).Count
}
```

Expected current shape: `TotalSrs` is about `2121`, `GeoipSrs` is about `278`, and `RequiredRuFiles` must be `4`.

For runtime verification without TUN/system proxy:

1. Ensure portable DB settings are false: `tun_mode_enabled`, `system_proxy_enabled`, `system_dns_set`, `enable_tun_routing`.
2. Start `deployment\windows-amd64\Throne.exe`.
3. Confirm `ThroneCore.exe` is running from the same directory.
4. If testing a profile, use proxy mode only and probe through:

```powershell
curl.exe --connect-timeout 10 --max-time 20 -x socks5h://127.0.0.1:2080 http://ip-api.com/json
```

5. Stop `Throne.exe` and `ThroneCore.exe` after automated tests unless the user asks to leave them running.

## Git Hygiene

- Do not stage `deployment/`, build caches, screenshots, or runtime DB files; they are ignored artifacts.
- Commit only source/build-script changes such as `CMakeLists.txt`, `script/*.ps1`, or code under `src/` and `include/`.
- When the user asks to commit fixes made while using this skill, write the commit message in Russian and describe what was fixed. Keep it specific and factual, for example: `Исправить запуск all-users установки без прав администратора`.
