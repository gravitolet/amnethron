---
name: build-throne-win64
description: Build, stage, package, run, smoke-test, and publish the AmneThron/Throne Windows x64 app from the local repository. Use when the user asks to compile Throne/AmneThron for Win64, rebuild the runnable Windows app, refresh deployment/windows-amd64, build or publish the Windows x64 installer to GitHub Releases, finish a releasable AmneThron source change, test ThroneCore startup, or verify an Amnezia-WG profile without enabling TUN or system proxy.
---

# Build Throne Win64

## Defaults

- Repository: `D:\Projects\Git\AmneThron`
- Runnable/staging directory: `D:\Projects\Git\AmneThron\deployment\windows-amd64`
- Do not use repo-local `build-local`; build out-of-tree under `%LOCALAPPDATA%\CodexBuild\AmneThron-win64`.
- Launch only `deployment\windows-amd64\Throne.exe` — by full path (`Start-Process`). Never computer-use `open_application "Throne"`: the name resolves to the installed `C:\Program Files\Throne` with its own TUN/system-proxy settings ([L-20261018-1](references/lessons.md)).
- Read Throne's state (windows, dialogs, log) via PowerShell/UI Automation, not screenshots: computer-use hides every non-allowlisted window, the Claude app included ([L-20261018-1](references/lessons.md)).
- Do not enable TUN or system proxy unless the user explicitly asks.
- Никогда не устанавливать собранный инсталлятор и не добавлять Throne (ни установленный, ни staging) в автозагрузку — это делает только владелец. Первый запуск копии со свежим `config` сам создаёт задачу `Throne AutoRun <hash>` (`mainwindow.cpp`, `autorun_initialized`): после смоук-запуска staging проверить `Get-ScheduledTask | ? TaskName -match 'Throne'` и, если появилась задача на `deployment\windows-amd64`, сообщить владельцу ([L-20261024-1](references/lessons.md)).
- Treat `deployment\windows-amd64\ThroneCore.exe` as required runtime input; if it is missing, stop and report that the runtime staging folder is incomplete.

## Default Completion Policy

- For bug fixes, connection debugging, and protocol additions, stop at the portable/staging build (`deployment\windows-amd64\`), run the smoke test, and wait for the user to verify functionality before building an installer.
- Do not run `build-installer.ps1`, bump `VERSION.txt`, or publish GitHub Releases until the user confirms the fix works or explicitly requests an installer/release build.
- When an installer or release is explicitly requested, or after user confirmation, proceed through installer build, artifact verification, commit, push, and release publication.
- При отправке анонсов в Telegram через UI/CDP автоматизацию проверять очистку поля ввода и выдерживать паузу до рендера бабла, не допуская повторной слепой отправки ([L-20260930-1](references/lessons.md)).
- Анонс релиза — в Telegram «Доступ к GPT» → тема General (отдельной темы «АмнеТрон» нет; анонсы 0.1.13–0.1.15 там, решение владельца 2026-11-26), через Chrome CDP (навык `vk-cdp-automation`); сначала использовать уже открытую вкладку Telegram Web, новую открывать только если её нет (L-20261018-2 там).
- Не запускать сборку инсталлятора повторно до подтверждения работы исправлений пользователем ([L-20261017-1](references/lessons.md)).


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
