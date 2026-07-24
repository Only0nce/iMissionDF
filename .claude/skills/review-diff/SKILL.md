---
name: review-diff
description: Review the actual staged and unstaged diff for defects, regressions, secrets, generated files, and missing validation.
---

# Procedure

1. Read repository instructions and status.
2. Inspect unstaged and staged diffs plus relevant surrounding source.
3. Verify C++/QML, thread, protocol, persistence, and hardware contracts.
4. Check machine-specific paths, credentials, hashes, tokens, generated files,
   unrelated changes, and accidental deletions.
5. Compare claims with build/test evidence.
6. Do not edit or approve your own implementation.

# Required Output

List findings by severity with file/component, evidence, impact, correction,
and validation. Then list rejected concerns, validation gaps, and release recommendation.
