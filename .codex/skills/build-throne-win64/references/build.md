# Building the app

Part of the `build-throne-win64` skill. Level: project.

## Build

Use the bundled script:

```powershell
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-win64.ps1"
```

The script configures CMake with VS 2022 x64, builds target `Throne`, and relies on the project `POST_BUILD` step to copy the fresh `Throne.exe` and `Throne.pdb` into `deployment\windows-amd64`.

Useful options:

```powershell
# Check VS/CMake/Qt/runtime paths without compiling.
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-win64.ps1" -ValidateOnly

# Clean external build cache, rebuild, and update deployment/windows-amd64.
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-win64.ps1" -Clean

# Build, launch the staged app, and verify ThroneCore starts.
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-win64.ps1" -Run -StopExisting

# Build, launch, auto-start a profile, and test proxy mode through 127.0.0.1:2080.
& "$env:USERPROFILE\.codex\skills\build-throne-win64\scripts\build-win64.ps1" -Run -StopExisting -StartProfileId 1 -ProxyTest
```
