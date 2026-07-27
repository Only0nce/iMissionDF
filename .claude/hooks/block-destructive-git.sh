#!/usr/bin/env bash
# Thin wrapper: the actual command parsing/policy lives in
# lib/destructive_git_policy.py (regex-on-raw-text was too easy to bypass
# via git wrappers, global options, and shell separators - see
# docs/CLAUDE-TEAM-MIGRATION-REPORT.md for the bypasses this replaced).
set -u

if ! command -v python3 >/dev/null 2>&1; then
    echo "Blocked: python3 is required to parse the Claude hook payload safely." >&2
    exit 2
fi

hook_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "$hook_dir/lib/destructive_git_policy.py"
exit $?
