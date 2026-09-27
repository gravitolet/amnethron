# Installer: producing it and its invariants

Part of the `build-throne-win64` skill. Level: project.

## Installer

Build the Windows x64 installer only after the staged app build is healthy:

```powershell
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-installer.ps1" -StopExisting
```

Useful options:

```powershell
# Clean the external CMake cache before compiling and packaging.
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-installer.ps1" -Clean -StopExisting

# Bump a different version component.
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-installer.ps1" -Increment Minor -StopExisting
```

The installer script:

- Reads the current three-part release version from `VERSION.txt`, computes the next version without writing it, and passes that next version to CMake and NSIS. For example, with `VERSION.txt` at `0.1.8`, a patch build must compile and package release `0.1.9`.
- Builds target `Throne` with VS 2022 x64/Ninja into `%LOCALAPPDATA%\CodexBuild\AmneThron-win64`.
- Downloads upstream `srslist.h` into the build directory before CMake, matching Throne CI.
- Packages only `deployment\windows-amd64` for Win10/Win11 x64.
- Includes `deployment\windows-amd64\rulesets\*.srs` in the installer as local fallback geoip/geosite assets for first launch or blocked update attempts.
- Outputs `deployment\installer\ThroneSetup-<version>.exe` and removes unversioned `ThroneSetup.exe`.
- Writes the next version to `VERSION.txt` only after the final versioned installer exists and is non-empty. A failed compile, `makensis` run, or final artifact copy must leave `VERSION.txt` unchanged.

After a successful build, always publish the installer and its Russian release description to `gravitolet/amnethron` GitHub Releases. Do not copy release artifacts or descriptions to Yandex Disk.

1. Verify the installer and staged runtime.
2. Commit `VERSION.txt` and all matching source/build-script changes, then push the commit to `origin` so the release tag points to the exact source used for the binary.
3. Write a non-empty UTF-8 Markdown release-notes file outside tracked source (for example under `deployment\installer`).
4. Publish with the bundled wrapper:

```powershell
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\publish-github-release.ps1" `
  -ReleaseNotesPath 'D:\Projects\Git\AmneThron\deployment\installer\release-notes-<version>.md'
```

The publisher uses `GH_TOKEN`/`GITHUB_TOKEN` when present and otherwise uses the existing Git Credential Manager login. It refuses to publish from a dirty tracked worktree or from a commit that is not present on an `origin/*` branch. It creates or updates tag/release `v<version>`, puts the notes in the release body, appends the installer SHA-256, and uploads the versioned installer asset.

## Installer Invariants

Keep these constraints intact when editing installer/build code:

- The installer filename must include the version: `ThroneSetup-<version>.exe`.
- Treat the three-part SemVer value (`0.1.9`) as the release version. Windows PE fixed-version resources require a fourth numeric field and may expose `0.1.9.0`; that transport detail is not the release version and must never be reported in place of `0.1.9`.
- Never use a preliminary `build-win64.ps1` build of the old `VERSION.txt` value to validate or announce the upcoming release version. The installer build must compute the next value, rebuild `Throne.exe` with `NKR_VERSION` set to it, and package that same value.
- `VERSION.txt` must be changed only after `deployment\installer\ThroneSetup-<version>.exe` has been created successfully and verified non-empty. Failed installer builds must not consume a version.
- Publish every finished installer as `ThroneSetup-<version>.exe` in GitHub Release `v<version>` for `gravitolet/amnethron`.
- Put the concise Russian version description in the GitHub Release body and include the SHA-256. Do not maintain a local `versions.txt` as the release-description source.
- Tag only a pushed commit containing the matching `VERSION.txt` and source code.
- The installer is currently Win10/Win11 x64 only and packages `deployment\windows-amd64`. Do not restore `windowslegacy-*` or `windows-arm64` packaging unless those staging directories are rebuilt and verified.
- The installer must include local rule-set fallback files from `deployment\windows-amd64\rulesets\*.srs`. This is required because first launch may not be able to download geo assets through direct/chernetinternet routing.
- At minimum, verify these files are inside the installer archive: `rulesets\geoip-ru.srs`, `rulesets\geosite-ru.srs`, `rulesets\geosite-category-ru.srs`, and `rulesets\geosite-ru-available-only-inside.srs`.
- Do not rely on repo-root relative paths in NSIS. Pass/use `PROJECT_ROOT` so icons, deployment files, plugins, and output paths resolve correctly regardless of the process working directory.
- Keep the uninstaller behavior that removes `$INSTDIR`; this also removes installed fallback `rulesets`.
