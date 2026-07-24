#!/usr/bin/env bash
set -euo pipefail

project_root=$(git rev-parse --show-toplevel)
target_qmake=${TARGET_QMAKE:-qmake}
target_spec=${TARGET_QMAKE_SPEC:-linux-jetson-orin-g++}
build_dir=${TARGET_BUILD_DIR:-"$project_root/build-claude-target"}
jobs=${BUILD_JOBS:-$(nproc)}

command -v "$target_qmake" >/dev/null 2>&1 || {
    echo "ERROR: target qmake not found: $target_qmake" >&2
    echo "Set TARGET_QMAKE to the target-toolchain qmake executable." >&2
    exit 1
}

mkdir -p "$build_dir"

echo "WARNING: this configures/compiles only; it does not validate physical hardware."
echo "Project: $project_root/iScanMR10.pro"
echo "Build directory: $build_dir"
echo "Target qmake: $target_qmake"
echo "Target mkspec: $target_spec"
echo "Active hardware selector:"
grep -nE '^[[:space:]]*CONFIG[[:space:]]*\+=[[:space:]]*HW_(5G|NONE_5G)[[:space:]]*$' \
    "$project_root/iScanMR10.pro" || true

(
    cd "$build_dir"
    "$target_qmake" "$project_root/iScanMR10.pro" -spec "$target_spec"
    make -j"$jobs"
)
