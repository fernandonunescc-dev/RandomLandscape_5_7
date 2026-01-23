# Git Commit Instructions

## Format
- Use **Conventional Commits**:
  - `feat: ...`, `fix: ...`, `refactor: ...`, `perf: ...`, `test: ...`, `docs: ...`, `chore: ...`
- Subject line:
  - imperative mood
  - <= 72 chars
  - no trailing period

## Body (only if needed)
- 1 blank line after subject.
- Bullet list of what/why (not how).
- Mention breaking changes explicitly:
  - `BREAKING CHANGE: ...`

## Content rules
- No fluff. No emojis.
- If multiple scopes, pick the dominant one or omit scope.
- Prefer describing user-visible behavior changes.

## Examples
- `fix: prevent null crash in PlayerSpawn`
- `refactor: extract sprint stamina regen timer`
- `feat: add biome color-map exporter`
