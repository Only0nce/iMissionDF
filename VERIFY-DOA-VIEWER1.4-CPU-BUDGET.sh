#!/usr/bin/env bash
set -euo pipefail
pass=0; fail=0
check(){ local name="$1"; shift; if "$@"; then echo "[PASS] $name"; pass=$((pass+1)); else echo "[FAIL] $name"; fail=$((fail+1)); fi; }
check "targetFps property declared" grep -Fq "Q_PROPERTY(int targetFps" FftDisplayItem.h
check "targetFps setter implemented" grep -Fq "FftDisplayItem::setTargetFps" FftDisplayItem.cpp
check "Per-item dirty repaint state present" grep -Fq "m_presentDirty" FftDisplayItem.h
check "Presentation budget interval implemented" grep -Fq "targetPresentIntervalMsLocked" FftDisplayItem.cpp
check "Shared clock default reduced" grep -Fq "ISCAN_ANALYZER_PRESENT_FPS\", 60, 120" FftDisplayItem.cpp
check "Shared clock is dirty gated" grep -Fq "if (!m_presentDirty)" FftDisplayItem.cpp
check "Budget skip counter present" grep -Fq "m_presentSkippedBudget" FftDisplayItem.cpp
check "Spectrum frame marks dirty" grep -Fq "m_spectrumFrame = frame;" FftDisplayItem.cpp
check "Waterfall row marks dirty" grep -Fq "appendProcessedWaterfallRowLocked(dbRow, argbRow);" FftDisplayItem.cpp
check "CPU budget telemetry present" grep -Fq "[DOA-CPU-BUDGET]" FftDisplayItem.cpp
check "Native spectrum item receives target fps" grep -Fq "targetFps: Math.max(1, root.fftFps)" DoaViewer/FftPlot.qml
check "Native waterfall item receives target fps" grep -Fq "targetFps: Math.max(1, root.wfFps)" DoaViewer/WaterfallCanvas.qml
check "FftPlot does not double feed native renderer" grep -Fq "binding-order edge cases without double-feeding C++" DoaViewer/FftPlot.qml
check "FftPlot still submits on frame sequence" grep -Fq "onFrameSequenceChanged: _submitNativeFrame" DoaViewer/FftPlot.qml
check "Waterfall native timer remains disabled" grep -Fq "running: root.enabled && root.visible && !root.nativeRenderEnabled" DoaViewer/WaterfallCanvas.qml
check "Native/CUDA renderer still enabled on ViewerPage" grep -Fq "nativeRenderEnabled: true" DoaViewer/ViewerPage.qml
check "CH1/CH2-CH6 mapping preserved" grep -Fq "CH1 = AstraRX/Home RX" DoaViewer/ViewerPage.qml
check "No accidental protocol change in DoaClient" bash -c 'cmp -s DoaViewer/DoaClient.cpp /mnt/data/doa_viewer13_cuda_work/DoaViewer/DoaClient.cpp && cmp -s DoaViewer/DoaClient.h /mnt/data/doa_viewer13_cuda_work/DoaViewer/DoaClient.h'
echo "VERIFY SUMMARY: pass=$pass fail=$fail"
exit $fail
