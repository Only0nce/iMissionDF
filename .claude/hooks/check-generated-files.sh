#!/usr/bin/env bash
set -u

payload=$(sed -n '1,$p')

if ! command -v python3 >/dev/null 2>&1; then
    echo "Blocked: python3 is required to parse the Claude hook payload safely." >&2
    exit 2
fi

file_path=$(
    printf '%s' "$payload" |
        python3 -c 'import json,sys; print(json.load(sys.stdin).get("tool_input", {}).get("file_path", ""))'
) || {
    echo "Blocked: could not parse the Claude file hook payload." >&2
    exit 2
}

[ -n "$file_path" ] || exit 0

case "$file_path" in
    */.qtc_clangd/*|*/build/*|*/build-*/*|*/Build/*|*/out/*|*/bin/*|*/obj/*|\
    */Makefile|*/Makefile.*|*/.qmake.stash|*.pro.user|*.pro.user.*|\
    */moc_*.cpp|*/moc_*.h|*/qrc_*.cpp|*/qrc_*.h|*/ui_*.h)
        echo "Blocked by project policy: generated or machine-specific file: $file_path" >&2
        exit 2
        ;;
esac

exit 0
