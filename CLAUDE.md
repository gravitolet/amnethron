# AmneThron — Claude Code

@AGENTS.md

Всё содержательное — в `AGENTS.md`. Здесь только то, что специфично для
Claude Code.

## Навыки

Проектные навыки лежат в `.codex/skills/` и видны через junction
`.claude/skills` -> `.codex/skills`. Отдельная копия для Claude Code не
создаётся: правь оригинал в `.codex/skills/build-throne-win64`.

`.claude/skills/` добавлен в `.gitignore`: иначе git пройдёт по junction и
продублирует те же файлы в индексе.

Новый или только что подключённый навык появляется в следующей сессии —
каталоги сканируются при её старте.

## Обучение на коррекции

Человек поправил — загрузи `self-learning-core` и следуй ему.
