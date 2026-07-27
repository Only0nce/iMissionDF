#!/usr/bin/env bash
# Thin wrapper: the actual path normalization/matching lives in
# lib/generated_file_policy.py (a bare `case` glob on the raw string missed
# relative paths like "Makefile" or "build/generated.cpp" that lack a
# leading "/" - see docs/CLAUDE-TEAM-MIGRATION-REPORT.md).
set -u

if ! command -v python3 >/dev/null 2>&1; then
    echo "Blocked: python3 is required to parse the Claude hook payload safely." >&2
    exit 2
fi

hook_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "$hook_dir/lib/generated_file_policy.py"
exit $?
