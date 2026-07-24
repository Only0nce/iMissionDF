#!/usr/bin/env bash
set -u

project_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "ERROR: run this script inside the project repository." >&2
    exit 1
}

failures=0

row() {
    printf '%-24s %-10s %s\n' "$1" "$2" "$3"
}

version_or_path() {
    command -v "$1" 2>/dev/null || echo "Not found"
}

printf '%-24s %-10s %s\n' "Component" "Status" "Detected"
printf '%-24s %-10s %s\n' "------------------------" "----------" "------------------------------"

if command -v git >/dev/null 2>&1; then
    row "Git" "PASS" "$(git --version)"
else
    row "Git" "FAIL" "Not found"
    failures=$((failures + 1))
fi

if command -v claude >/dev/null 2>&1; then
    row "Claude Code" "PASS" "$(claude --version 2>/dev/null)"
else
    row "Claude Code" "WARN" "Not found"
fi

row "Architecture" "PASS" "$(uname -m)"

if command -v qmake >/dev/null 2>&1; then
    qt_version=$(qmake -query QT_VERSION 2>/dev/null || echo unknown)
    case "$qt_version" in
        5.*) row "Qt" "PASS" "$qt_version" ;;
        *) row "Qt" "FAIL" "$qt_version (Qt 5 required)"; failures=$((failures + 1)) ;;
    esac
    row "qmake" "PASS" "$(version_or_path qmake)"
    qmake_spec=$(qmake -query QMAKE_SPEC 2>/dev/null || echo unknown)
    if [ "$qmake_spec" = "linux-g++" ]; then
        row "Desktop build profile" "PASS" "$qmake_spec"
    else
        row "Desktop build profile" "WARN" "$qmake_spec"
    fi
else
    row "Qt" "FAIL" "qmake not found"
    row "qmake" "FAIL" "Not found"
    failures=$((failures + 1))
fi

if command -v g++ >/dev/null 2>&1; then
    row "Compiler" "PASS" "$(g++ --version | sed -n '1p')"
else
    row "Compiler" "FAIL" "g++ not found"
    failures=$((failures + 1))
fi

if command -v pkg-config >/dev/null 2>&1; then
    missing_qt_modules=
    for module_name in \
        Qt5Core Qt5Quick Qt5WebSockets Qt5Sql Qt5Widgets Qt5Multimedia \
        Qt5OpenGL Qt5Concurrent; do
        if ! pkg-config --exists "$module_name" 2>/dev/null; then
            missing_qt_modules="${missing_qt_modules}${missing_qt_modules:+,}$module_name"
        fi
    done
    if [ -n "$missing_qt_modules" ]; then
        row "Desktop Qt modules" "FAIL" "Missing: $missing_qt_modules"
        failures=$((failures + 1))
    else
        row "Desktop Qt modules" "PASS" "Required Qt 5 modules detected"
    fi

    missing_native_modules=
    for module_name in openssl alsa libgps geographiclib; do
        if ! pkg-config --exists "$module_name" 2>/dev/null; then
            missing_native_modules="${missing_native_modules}${missing_native_modules:+,}$module_name"
        fi
    done
    if [ -n "$missing_native_modules" ]; then
        row "Native libraries" "FAIL" "Missing: $missing_native_modules"
        failures=$((failures + 1))
    else
        row "Native libraries" "PASS" "Required pkg-config modules detected"
    fi
else
    row "pkg-config modules" "FAIL" "pkg-config not found"
    failures=$((failures + 1))
fi

if [ -n "${TARGET_QMAKE:-}" ]; then
    row "Target build profile" "WARN" "Configured but not executed"
else
    row "Target build profile" "SKIP" "TARGET_QMAKE is unset"
fi

if git -C "$project_root" diff --quiet --ignore-submodules -- &&
   git -C "$project_root" diff --cached --quiet --ignore-submodules -- &&
   [ -z "$(git -C "$project_root" ls-files --others --exclude-standard)" ]; then
    row "Repository state" "PASS" "Clean"
else
    row "Repository state" "WARN" "Local changes exist"
fi

tracked_local_count=$(
    git -C "$project_root" ls-files |
        grep -Ec '(^|/)(\.qmake\.stash|Makefile(\..*)?|.*\.pro\.user(\..*)?|\.qtc_clangd/)' ||
        true
)
if [ "$tracked_local_count" -gt 0 ]; then
    row "Tracked local artifacts" "WARN" "$tracked_local_count historical file(s)"
else
    row "Tracked local artifacts" "PASS" "None"
fi

strong_pattern='BEGIN [A-Z ]*PRIVATE KEY|github_pat_|ghp_[A-Za-z0-9]{20,}|sk-[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}'
if git -C "$project_root" grep -IlE "$strong_pattern" -- ':!*.lock' >/dev/null 2>&1; then
    row "Secrets scan" "FAIL" "Strong tracked indicator found; value hidden"
    failures=$((failures + 1))
elif git -C "$project_root" grep -q 'kLegacyNetworkPasswordSha256' -- NetworkSecurityController.cpp 2>/dev/null; then
    row "Secrets scan" "WARN" "Legacy password hash fallback tracked; value hidden"
else
    row "Secrets scan" "PASS" "No obvious tracked indicators"
fi

if [ -n "${ISCAN_NETWORK_ADMIN_PASSWORD_SHA256:-}" ]; then
    row "Network password env" "PASS" "Set; value hidden"
else
    row "Network password env" "WARN" "Unset"
fi

exit "$failures"
