#!/usr/bin/env bash
set -euo pipefail
fail=0
check() {
  local name="$1"; shift
  if "$@"; then
    echo "[PASS] $name"
  else
    echo "[FAIL] $name"
    fail=$((fail+1))
  fi
}

check "FftLineGraphItem header exists" test -f FftLineGraphItem.h
check "FftLineGraphItem source exists" test -f FftLineGraphItem.cpp
check "SceneGraph item derives QQuickItem" grep -Fq "class FftLineGraphItem : public QQuickItem" FftLineGraphItem.h
check "SceneGraph updatePaintNode override exists" grep -Fq "updatePaintNode" FftLineGraphItem.h
check "SceneGraph geometry node used" grep -Fq "QSGGeometryNode" FftLineGraphItem.cpp
check "Flat color material used" grep -Fq "QSGFlatColorMaterial" FftLineGraphItem.cpp
check "Triangle strip fill used" grep -Fq "DrawTriangleStrip" FftLineGraphItem.cpp
check "Line strip trace used" grep -Fq "DrawLineStrip" FftLineGraphItem.cpp
check "Peak pooling per pixel implemented" grep -Fq "peakPoolPerPixel=1" FftLineGraphItem.cpp
check "Coalesced update implemented" grep -Fq "m_updatePending" FftLineGraphItem.cpp
check "Budget timer implemented" grep -Fq "m_budgetTimer" FftLineGraphItem.cpp
check "GPU telemetry log present" grep -Fq "[DOA-GPU-SPECTRUM]" FftLineGraphItem.cpp
check "FftLineGraphItem registered to QML" grep -Fq 'qmlRegisterType<FftLineGraphItem>("iScan.Display", 1, 0, "FftLineGraphItem")' main.cpp
check "FftLineGraphItem source added to qmake" grep -Fq "FftLineGraphItem.cpp" iScanMR10.pro
check "FftLineGraphItem header added to qmake" grep -Fq "FftLineGraphItem.h" iScanMR10.pro
check "DoA FftPlot uses FftLineGraphItem" grep -Fq "FftLineGraphItem {" DoaViewer/FftPlot.qml
check "DoA FftPlot still submits external frame" grep -Fq "nativeSpectrumItem.submitExternalFrame(root.magDb)" DoaViewer/FftPlot.qml
check "Waterfall remains existing CUDA FftDisplayItem" grep -Fq "id: nativeWaterfallItem" DoaViewer/WaterfallCanvas.qml
check "Waterfall remains FftDisplayItem.Waterfall" grep -Fq "mode: FftDisplayItem.Waterfall" DoaViewer/WaterfallCanvas.qml

if [[ $fail -ne 0 ]]; then
  echo "VERIFY: $fail FAIL"
  exit 1
fi

echo "VERIFY: 19 PASS / 0 FAIL"
