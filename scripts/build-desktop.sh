#!/usr/bin/env bash
set -euo pipefail

project_root=$(git rev-parse --show-toplevel)
qmake_bin=${QMAKE_BIN:-qmake}
build_dir=${BUILD_DIR:-"$project_root/build-claude-desktop"}
jobs=${BUILD_JOBS:-$(nproc)}

command -v "$qmake_bin" >/dev/null 2>&1 || {
    echo "ERROR: qmake not found: $qmake_bin" >&2
    exit 1
}

mkdir -p "$build_dir"

echo "Project: $project_root/iScanMR10.pro"
echo "Build directory: $build_dir"
echo "qmake: $qmake_bin"
echo "Qt: $("$qmake_bin" -query QT_VERSION)"
echo "Active hardware selector:"
grep -nE '^[[:space:]]*CONFIG[[:space:]]*\+=[[:space:]]*HW_(5G|NONE_5G)[[:space:]]*$' \
    "$project_root/iScanMR10.pro" || true

(
    cd "$build_dir"
    "$qmake_bin" "$project_root/iScanMR10.pro" -spec linux-g++
    make -j"$jobs"
)
