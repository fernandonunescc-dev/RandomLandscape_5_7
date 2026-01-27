# Copilot Instructions (Global)

## Output style (token-efficient)
- Output **only what is needed** to complete the task.
- **No preambles** (no “I found…”, “Sure…”, “Here’s…”).
- Prefer **code first**. Use a **single short sentence** only when code alone is ambiguous.
- Avoid re-stating the question, narration, or step-by-step commentary.
- If uncertain, make **one best assumption** and proceed. Note assumptions **in one line** at the end.

## Code format
- Return changes as:
  - **Unified diff** when editing existing code, OR
  - **Full file content** when creating new files.
- Keep diffs minimal; do not reformat unrelated code.
- Do not paste large unchanged files; include only relevant sections/diff.

## Reasoning & analysis
- Do not output chain-of-thought.
- Debugging responses must include:
  - **Most likely root cause**
  - **Exact code change(s)** to fix it
  - **Optional**: 1–3 verification commands/steps (max)

## Quality bar
- Compile-safe / type-safe.
- Handle nullability, edge cases, and error paths.
- Prefer clear naming and small functions.
- Add comments only where non-obvious; keep them short unless user explicitly requests detailed comments.

## When blocked
- If you must ask for info, ask **one** high-signal question, and propose a default path if unanswered.

## Language/tooling preferences
- Kotlin: idiomatic Kotlin, prefer immutable data, sealed interfaces/classes where useful.
- Android: MVVM, Compose, Flow/Coroutines; avoid anti-patterns (GlobalScope, leaking Context).
- C++/UE: UE-style macros, UPROPERTY/UFUNCTION correctness, replication-aware code.
