#!/usr/bin/env bash
set -u

payload=$(sed -n '1,$p')

if ! command -v python3 >/dev/null 2>&1; then
    echo "Secret hook warning: python3 is unavailable; payload was not inspected." >&2
    exit 1
fi

file_path=$(
    printf '%s' "$payload" |
        python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input", {}).get("file_path", ""))'
) || {
    echo "Secret hook warning: could not parse file path." >&2
    exit 1
}

[ -n "$file_path" ] || exit 0
[ -f "$file_path" ] || exit 0

strong_pattern='BEGIN [A-Z ]*PRIVATE KEY|github_pat_[A-Za-z0-9_]+|ghp_[A-Za-z0-9]{20,}|sk-[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}|AIza[0-9A-Za-z_-]{30,}|xox[baprs]-[0-9A-Za-z-]{10,}'
password_hash_pattern='(password|passwd|secret)[^[:cntrl:]]{0,40}[=:][[:space:]]*["'\'']?[a-fA-F0-9]{64}["'\'']?'

if LC_ALL=C grep -IqiE "$strong_pattern|$password_hash_pattern" "$file_path"; then
    echo "Possible secret detected in $file_path; value withheld. Review before continuing." >&2
    exit 2
fi

exit 0
