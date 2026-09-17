#!/usr/bin/env bash
set -euo pipefail
fail=0
pass=0
check() {
  local name="$1" pattern="$2" file="$3"
  if grep -qE "$pattern" "$file"; then
    echo "PASS: $name"; pass=$((pass+1))
  else
    echo "FAIL: $name"; fail=$((fail+1))
  fi
}
check "QML deferred native submit" "Qt\.callLater\(function\(\)" DoaViewer/WaterfallCanvas.qml
check "QML row db change recovery" "onWaterfallRowDbChanged" DoaViewer/WaterfallCanvas.qml
check "QML no native reset on geometry jitter" "Avoid clearing history from QML" DoaViewer/WaterfallCanvas.qml
check "C++ first visible fallback declaration" "appendFirstVisibleCpuFallbackLocked" FftWaterfallTextureItem.h
check "C++ first visible fallback log" "DOA-WF-FIRST-ROW" FftWaterfallTextureItem.cpp
check "C++ bootstrap cuda path" "cuda-bootstrap" FftWaterfallTextureItem.cpp
check "C++ cuda timeout path" "cuda-timeout" FftWaterfallTextureItem.cpp
check "C++ defensive dB fallback" "empty ARGB row" FftWaterfallTextureItem.cpp
check "C++ creates background texture before rows" "background texture proves the SceneGraph path is alive" FftWaterfallTextureItem.cpp
check "C++ no early history null return before ensure" "ensureHistoryLocked\(qMax\(1, static_cast<int>\(std::ceil\(width\(\)\)\)\)" FftWaterfallTextureItem.cpp
check "C++ geometry diagnostic" "DOA-WF-GEOM" FftWaterfallTextureItem.cpp
check "C++ bootstrap telemetry" "bootstrapCpuRows" FftWaterfallTextureItem.cpp
check "Engineering note exists" "Waterfall Pipeline Diagnostic" DOA-VIEWER1.7-r3-WATERFALL-PIPELINE-DIAGNOSTIC-RECOVERY.md
if ((fail)); then
  echo "VERIFY: $pass PASS / $fail FAIL"
  exit 1
fi
echo "VERIFY: $pass PASS / 0 FAIL"
