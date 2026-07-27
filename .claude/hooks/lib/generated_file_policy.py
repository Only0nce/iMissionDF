#!/usr/bin/env python3
"""Detect generated/machine-specific file paths in a Claude Edit|Write
tool_input.file_path, regardless of whether the path was given as absolute,
relative, "./"-prefixed, or containing ".." components.

Reads a Claude PreToolUse hook JSON payload from stdin.

Exit codes:
  0 - path is allowed
  2 - path is blocked by policy (or the payload could not be parsed)
"""
import fnmatch
import json
import os
import sys

# Patterns are matched against the normalized *absolute* path, so every
# pattern can assume a leading "/" is always present - this is what lets a
# bare relative name like "Makefile" match "*/Makefile" the same way
# "/repo/Makefile" does.
BLOCKED_PATTERNS = [
    "*/.qtc_clangd/*",
    "*/build/*",
    "*/build-*/*",
    "*/Build/*",
    "*/out/*",
    "*/bin/*",
    "*/obj/*",
    "*/Makefile",
    "*/Makefile.*",
    "*/.qmake.stash",
    "*.pro.user",
    "*.pro.user.*",
    "*/moc_*.cpp",
    "*/moc_*.h",
    "*/qrc_*.cpp",
    "*/qrc_*.h",
    "*/ui_*.h",
]


def main():
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        print("Blocked: could not parse the Claude file hook payload.", file=sys.stderr)
        sys.exit(2)

    file_path = payload.get("tool_input", {}).get("file_path", "") or ""
    if not file_path:
        sys.exit(0)

    # normpath collapses "./" and ".." components lexically without touching
    # the filesystem, so this works for files that don't exist yet.
    normalized = os.path.abspath(os.path.normpath(file_path))

    for pattern in BLOCKED_PATTERNS:
        if fnmatch.fnmatch(normalized, pattern):
            print(
                f"Blocked by project policy: generated or machine-specific file: {file_path}",
                file=sys.stderr,
            )
            sys.exit(2)

    sys.exit(0)


if __name__ == "__main__":
    main()
