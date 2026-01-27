# C++ / Unreal Engine (.instructions.md)

## Output
- Unified diff only.
- Do not change formatting unrelated to the fix.

## UE patterns
- Use UE types (`TArray`, `TMap`, `FString`, `FName`) and reflection macros correctly.
- Keep UPROPERTY/UFUNCTION specifiers correct (BlueprintReadOnly, EditDefaultsOnly, Replicated, etc.).
- Network: server authority checks where needed; avoid client-side authority assumptions.
- Prefer `UE_LOG` with meaningful categories, but do not add excessive logging.

## Performance
- Avoid per-tick allocations.
- Prefer references / `const&` where safe.
- Avoid copying large arrays.

## Comments
- Only for non-obvious logic; keep short unless explicitly requested.
