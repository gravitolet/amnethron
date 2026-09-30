---
name: build-throne-win64
description: Build, stage, package, run, smoke-test, and publish the AmneThron/Throne Windows x64 app from the local repository. Use when the user asks to compile Throne/AmneThron for Win64, rebuild the runnable Windows app, refresh deployment/windows-amd64, build or publish the Windows x64 installer to GitHub Releases, finish a releasable AmneThron source change, test ThroneCore startup, or verify an Amnezia-WG profile without enabling TUN or system proxy.
---

# Build Throne Win64

## Defaults

- Repository: `D:\Projects\Git\AmneThron`
- Runnable/staging directory: `D:\Projects\Git\AmneThron\deployment\windows-amd64`
- Do not use repo-local `build-local`; build out-of-tree under `%LOCALAPPDATA%\CodexBuild\AmneThron-win64`.
- Launch only `deployment\windows-amd64\Throne.exe`.
- Do not enable TUN or system proxy unless the user explicitly asks.
- Treat `deployment\windows-amd64\ThroneCore.exe` as required runtime input; if it is missing, stop and report that the runtime staging folder is incomplete.

## Default Completion Policy

- Treat a successful portable/staging build as an intermediate result, not completion, after a releasable AmneThron source change.
- When no user-only verification is required, continue automatically through installer build, artifact verification, source/version commit, push, and GitHub Release publication. Do not stop to request confirmation between these stages.
- Require user verification only when the result depends on visual, interactive, network, or environment-specific evidence that cannot be checked safely with the available tools, or when the user explicitly requests a review checkpoint.
- If user verification is required, state the exact check before building the installer. After the user confirms it, resume the installer and publication workflow without asking again.
- Stop after a portable build only when the user explicitly requests a local/staging/preview build or explicitly says not to publish.
- При отправке анонсов в Telegram через UI/CDP автоматизацию проверять очистку поля ввода и выдерживать паузу до рендера бабла, не допуская повторной слепой отправки ([L-20260930-1](references/lessons.md)).


## Failure Learning Rule

- When a new build, packaging, installer, staging, or runtime-smoke problem is encountered and solved, update this skill before finishing the task. Record the symptom, root cause when known, and the working fix in `Known Failure Modes` or the nearest relevant section.


## Where to look next

| Need | File |
|---|---|
| Compile and stage the app | [references/build.md](references/build.md) |
| Package or publish the installer | [references/installer.md](references/installer.md) |
| Something broke in a way seen before | [references/failures.md](references/failures.md) |
| Smoke-test, verify, commit | [references/verification.md](references/verification.md) |

Read `references/failures.md` **before** retrying a failed build, not after
the second failure.

When the owner corrects a result or a method, load `self-learning-core`
and record the lesson in this skill's `references/`, at project level.
