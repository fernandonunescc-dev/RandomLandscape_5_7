# Kotlin / Android (.instructions.md)

## Output
- Prefer **patch/diff**.
- If multiple files change, output in file order with clear `--- a/...` / `+++ b/...`.

## Kotlin style
- Prefer `val`, data classes, sealed hierarchies.
- Prefer `when` exhaustiveness.
- Prefer `Result`/sealed outcome types over exceptions for expected failures.
- Use `@Immutable`/`@Stable` where relevant in Compose.

## Coroutines / Flow
- No `GlobalScope`.
- Prefer structured concurrency (`viewModelScope`, `lifecycleScope`, custom `CoroutineScope` owned by a class).
- Flow: avoid collecting in Composables without lifecycle awareness; prefer `collectAsStateWithLifecycle`.

## Compose
- Stateless composables by default.
- Hoist state; pass events as lambdas.
- Avoid recomposition traps (unstable params, creating objects in composition).

## Gradle / dependencies
- Prefer version catalogs (`libs.versions.toml`) when present.
- Keep dependency changes minimal and consistent.

## Tests
- Prefer JUnit + Turbine for Flow when appropriate.
- Make tests deterministic (no real delays; use `runTest`).
