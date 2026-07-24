#!/usr/bin/env bash
set -u

project_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "ERROR: run this script inside the project repository." >&2
    exit 1
}

echo "iScanMR10 development bootstrap check"
echo

if [ -r /etc/os-release ]; then
    ( . /etc/os-release; printf 'OS: %s %s\n' "$NAME" "$VERSION_ID" )
else
    printf 'OS: %s\n' "$(uname -s)"
fi
printf 'Architecture: %s\n' "$(uname -m)"
printf 'Repository: %s\n\n' "$project_root"

missing=0
for tool_name in git claude qmake make g++ python3; do
    if command -v "$tool_name" >/dev/null 2>&1; then
        printf 'PASS  %-12s %s\n' "$tool_name" "$(command -v "$tool_name")"
    else
        printf 'MISS  %-12s not found\n' "$tool_name"
        missing=$((missing + 1))
    fi
done

if command -v qmake >/dev/null 2>&1; then
    qt_version=$(qmake -query QT_VERSION 2>/dev/null || echo unknown)
    qmake_spec=$(qmake -query QMAKE_SPEC 2>/dev/null || echo unknown)
    printf 'INFO  %-12s Qt %s, spec %s\n' "qmake" "$qt_version" "$qmake_spec"
fi

if command -v pkg-config >/dev/null 2>&1; then
    for module_name in \
        Qt5Core Qt5Quick Qt5WebSockets Qt5Sql Qt5Widgets Qt5Multimedia \
        Qt5OpenGL Qt5Concurrent openssl alsa libgps geographiclib; do
        if pkg-config --exists "$module_name" 2>/dev/null; then
            printf 'PASS  pkg:%-16s %s\n' "$module_name" "$(pkg-config --modversion "$module_name" 2>/dev/null)"
        else
            printf 'MISS  pkg:%-16s development package not detected\n' "$module_name"
            missing=$((missing + 1))
        fi
    done
else
    printf 'MISS  %-12s not found\n' "pkg-config"
    missing=$((missing + 1))
fi

for hook_path in \
    .claude/hooks/block-destructive-git.sh \
    .claude/hooks/check-generated-files.sh \
    .claude/hooks/check-secrets.sh; do
    if [ -x "$project_root/$hook_path" ]; then
        printf 'PASS  hook         %s\n' "$hook_path"
    else
        printf 'MISS  hook         %s is absent or not executable\n' "$hook_path"
        missing=$((missing + 1))
    fi
done

if [ -n "${ISCAN_NETWORK_ADMIN_PASSWORD_SHA256:-}" ]; then
    printf 'PASS  security     ISCAN_NETWORK_ADMIN_PASSWORD_SHA256 is set (value hidden)\n'
else
    printf 'WARN  security     password-hash override is unset; legacy source fallback remains active\n'
fi

if [ -n "${TARGET_QMAKE:-}" ]; then
    printf 'INFO  target       TARGET_QMAKE is configured (value hidden)\n'
else
    printf 'SKIP  target       TARGET_QMAKE is unset; target toolchain is optional on desktop\n'
fi

echo
if [ "$missing" -gt 0 ]; then
    echo "$missing required or recommended component(s) are missing."
    if [ -r /etc/os-release ] && ( . /etc/os-release; [ "${ID:-}" = ubuntu ] || [ "${ID_LIKE:-}" = debian ] ); then
        echo "Review and run manually if appropriate:"
        echo "  sudo apt update"
        echo "  sudo apt install git make g++ python3 pkg-config qt5-qmake qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev libqt5websockets5-dev libqt5opengl5-dev libqt5sql5-sqlite libssl-dev libasound2-dev libgps-dev libgeographic-dev"
    fi
else
    echo "Core desktop development checks passed."
fi

echo "No packages were installed and no global Claude settings were modified."
exit 0
