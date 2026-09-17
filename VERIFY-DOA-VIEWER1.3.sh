#!/usr/bin/env bash
set -euo pipefail
pass=0; fail=0
check(){ local name="$1"; shift; if "$@"; then echo "[PASS] $name"; pass=$((pass+1)); else echo "[FAIL] $name"; fail=$((fail+1)); fi; }
check "FftDisplayItem external frame API declared" grep -Fq "submitExternalFrame" FftDisplayItem.h
check "FftDisplayItem external frame API implemented" grep -Fq "FftDisplayItem::submitExternalFrame" FftDisplayItem.cpp
check "External frame feeds spectrum path" grep -Fq "onSpectrumFrame(frame)" FftDisplayItem.cpp
check "External frame feeds waterfall path" grep -Fq "onWaterfallFrame(frame)" FftDisplayItem.cpp
check "FftPlot imports native display type" grep -Fq "import iScan.Display 1.0" DoaViewer/FftPlot.qml
check "FftPlot native renderer property" grep -Fq "property bool nativeRenderEnabled" DoaViewer/FftPlot.qml
check "FftPlot uses FftDisplayItem" grep -Fq "id: nativeSpectrumItem" DoaViewer/FftPlot.qml
check "FftPlot bypasses QML trace when native" grep -Fq "visible: root.enabled && !root.nativeRenderEnabled" DoaViewer/FftPlot.qml
check "Waterfall imports native display type" grep -Fq "import iScan.Display 1.0" DoaViewer/WaterfallCanvas.qml
check "Waterfall native renderer property" grep -Fq "property bool nativeRenderEnabled" DoaViewer/WaterfallCanvas.qml
check "Waterfall uses FftDisplayItem" grep -Fq "id: nativeWaterfallItem" DoaViewer/WaterfallCanvas.qml
check "Waterfall timer disabled in native path" grep -Fq "running: root.enabled && root.visible && !root.nativeRenderEnabled" DoaViewer/WaterfallCanvas.qml
check "ViewerPage enables native spectrum" grep -Fq "nativeRenderEnabled: true" DoaViewer/ViewerPage.qml
check "ViewerPage preserves display frame sequence" grep -Fq "frameSequence: root.displayFrameSequence" DoaViewer/ViewerPage.qml
check "Engineering note exists" test -s DOA-VIEWER1.3-NATIVE-CUDA-RENDERER.md
# Lightweight syntax guards: verify that the native items and functions are inside the expected files.
check "FftPlot submit function present" grep -Fq "function _submitNativeFrame" DoaViewer/FftPlot.qml
check "Waterfall submit function present" grep -Fq "function _submitNativeRow" DoaViewer/WaterfallCanvas.qml
check "No legacy full-file comment block before active Waterfall imports" bash -c 'head -40 DoaViewer/WaterfallCanvas.qml | grep -Fq "import QtQuick 2.15"'

echo "VERIFY SUMMARY: pass=$pass fail=$fail"
exit $fail
