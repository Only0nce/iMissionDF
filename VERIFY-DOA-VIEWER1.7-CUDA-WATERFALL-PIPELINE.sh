#!/usr/bin/env bash
set -euo pipefail
pass=0
fail=0
check() {
  local name="$1"; shift
  if "$@"; then
    echo "PASS - $name"; pass=$((pass+1))
  else
    echo "FAIL - $name"; fail=$((fail+1))
  fi
}
contains() { grep -Fq -- "$2" "$1"; }
not_contains() { ! grep -Fq -- "$2" "$1"; }

check "waterfall item includes SpectrumCudaWorker" contains FftWaterfallTextureItem.cpp '#include "SpectrumCudaWorker.h"'
check "CUDA processing property exposed" contains FftWaterfallTextureItem.h 'Q_PROPERTY(bool cudaProcessingEnabled'
check "compute backend property exposed" contains FftWaterfallTextureItem.h 'Q_PROPERTY(QString computeBackend'
check "cuda active property exposed" contains FftWaterfallTextureItem.h 'Q_PROPERTY(bool cudaAccelerationActive'
check "CUDA worker thread object name" contains FftWaterfallTextureItem.cpp 'DoaCudaWaterfall'
check "processWaterfallRequested signal exists" contains FftWaterfallTextureItem.h 'processWaterfallRequested'
check "worker waterfall slot connected" contains FftWaterfallTextureItem.cpp 'SpectrumCudaWorker::processWaterfallRow'
check "backend ready connected" contains FftWaterfallTextureItem.cpp 'onComputeBackendReady'
check "waterfall row ready connected" contains FftWaterfallTextureItem.cpp 'onWaterfallRowReady'
check "latest-frame-only pending queue" contains FftWaterfallTextureItem.cpp 'Latest-frame-only queue'
check "stale backlog drop implemented" contains FftWaterfallTextureItem.cpp 'Drop the old'
check "CUDA telemetry rows" contains FftWaterfallTextureItem.cpp 'cudaRows='
check "CPU fallback telemetry rows" contains FftWaterfallTextureItem.cpp 'cpuRows='
check "coalesced telemetry" contains FftWaterfallTextureItem.cpp 'coalesced='
check "plugin-stage telemetry" contains FftWaterfallTextureItem.cpp 'cudaInterop=plugin-stage'
check "startup 1.7 log" contains FftWaterfallTextureItem.cpp '[DOA-VIEWER1.7-CUDA-WATERFALL]'
check "startup 1.6 compatibility log retained" contains FftWaterfallTextureItem.cpp '[DOA-VIEWER1.6-GPU-WATERFALL]'
check "QML exposes cudaWaterfallEnabled" contains DoaViewer/WaterfallCanvas.qml 'property bool cudaWaterfallEnabled: true'
check "QML binds cudaProcessingEnabled" contains DoaViewer/WaterfallCanvas.qml 'cudaProcessingEnabled: root.cudaWaterfallEnabled'
check "QPainter not used by new waterfall item" not_contains FftWaterfallTextureItem.cpp 'QPainter'
check "1.6-r2 releaseResources still present" contains FftWaterfallTextureItem.cpp 'releaseResources()'
check "conservative texture node replacement retained" contains FftWaterfallTextureItem.cpp 'delete oldNode;'
python3 - <<'PY'
from pathlib import Path
files=['FftWaterfallTextureItem.h','FftWaterfallTextureItem.cpp','DoaViewer/WaterfallCanvas.qml']
for f in files:
    text=Path(f).read_text()
    bal=0
    for ch in text:
        if ch=='{': bal+=1
        elif ch=='}': bal-=1
        if bal < 0:
            raise SystemExit(f'{f}: negative brace balance')
    if bal != 0:
        raise SystemExit(f'{f}: brace balance {bal}')
print('brace sanity ok')
PY
if [[ $fail -ne 0 ]]; then
  echo "VERIFY: $pass PASS / $fail FAIL"
  exit 1
fi
echo "VERIFY: $pass PASS / $fail FAIL"
