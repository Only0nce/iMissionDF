#!/usr/bin/env bash
set -u

payload=$(sed -n '1,$p')

if ! command -v python3 >/dev/null 2>&1; then
    echo "Blocked: python3 is required to parse the Claude hook payload safely." >&2
    exit 2
fi

command_text=$(
    printf '%s' "$payload" |
        python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input", {}).get("command", ""))'
) || {
    echo "Blocked: could not parse the Claude Bash hook payload." >&2
    exit 2
}

git_prefix='(^|[;&|[:space:]])([^;&|[:space:]]*/)?git[[:space:]]+'
blocked_pattern="${git_prefix}reset[[:space:]]+--hard|\
${git_prefix}clean[^;&|]*(--force|-[A-Za-z]*f[A-Za-z]*)|\
${git_prefix}checkout[[:space:]]+--[[:space:]]+\\.|\
${git_prefix}restore[[:space:]]+\\.|\
${git_prefix}push[^;&|]*--force(-with-lease)?|\
${git_prefix}add[[:space:]]+-A"

if printf '%s\n' "$command_text" | grep -Eq "$blocked_pattern"; then
    echo "Blocked by project policy: destructive Git or bulk staging command." >&2
    exit 2
fi

exit 0
