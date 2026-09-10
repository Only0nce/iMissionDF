#!/bin/bash
set -euo pipefail

# Build the optional iScanMR10 Spectrum/Waterfall CUDA plugin natively on
# Jetson Orin. This intentionally stays outside the Qt cross-build so nvcc's
# host object always matches AArch64.

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
CUDA_ROOT="${CUDA_HOME:-/usr/local/cuda}"
NVCC="${CUDA_ROOT}/bin/nvcc"
CUDA_ARCH="${ISCAN_CUDA_ARCH:-sm_87}"
OUT="${ISCAN_CUDA_OUTPUT:-${SRC_DIR}/libiscan_spectrum_cuda.so}"
INSTALL_DIR="${ISCAN_CUDA_INSTALL_DIR:-/opt/iScanMR10/lib}"

if [[ ! -x "$NVCC" ]]; then
    echo "ERROR: nvcc not found at $NVCC" >&2
    exit 2
fi

HOST_ARCH="$(uname -m)"
if [[ "$HOST_ARCH" != "aarch64" && "$HOST_ARCH" != "arm64" ]]; then
    echo "ERROR: build this plugin natively on Jetson Orin (current host: $HOST_ARCH)." >&2
    echo "The Qt application itself may remain cross-compiled in Qt Creator." >&2
    exit 3
fi

echo "[CUDA-PLUGIN] nvcc      : $NVCC"
echo "[CUDA-PLUGIN] arch      : $CUDA_ARCH"
echo "[CUDA-PLUGIN] source    : ${SRC_DIR}/SpectrumCudaKernels.cu"
echo "[CUDA-PLUGIN] output    : $OUT"

"$NVCC" \
    -shared \
    -O3 \
    -std=c++14 \
    -arch="$CUDA_ARCH" \
    -Xcompiler -fPIC \
    "${SRC_DIR}/SpectrumCudaKernels.cu" \
    -o "$OUT" \
    -lcudart

file "$OUT" || true
sha256sum "$OUT" || true

if [[ "${1:-}" == "--install" ]]; then
    install -d -m 0755 "$INSTALL_DIR"
    install -m 0755 "$OUT" "$INSTALL_DIR/libiscan_spectrum_cuda.so"
    echo "[CUDA-PLUGIN] installed: $INSTALL_DIR/libiscan_spectrum_cuda.so"
fi
