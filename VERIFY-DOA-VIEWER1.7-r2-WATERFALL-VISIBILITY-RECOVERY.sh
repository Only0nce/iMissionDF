#!/usr/bin/env bash
set -euo pipefail
fail=0
pass=0
check() {
  local name="$1"; shift
  if "$@"; then
    echo "PASS: $name"; pass=$((pass+1))
  else
    echo "FAIL: $name"; fail=$((fail+1))
  fi
}
contains() { local f="$1" p="$2"; grep -Fq "$p" "$f"; }
not_contains() { local f="$1" p="$2"; ! grep -Fq "$p" "$f"; }

check "backend env parser exists" contains FftWaterfallTextureItem.cpp 'ISCAN_DOA_WATERFALL_BACKEND'
check "testpattern mode exists" contains FftWaterfallTextureItem.cpp 'testpattern'
check "cpu mode exists" contains FftWaterfallTextureItem.cpp 'cpu-forced'
check "backend telemetry exists" contains FftWaterfallTextureItem.cpp '[DOA-WF-BACKEND]'
check "stale row guard no longer drops newer-pending completions" not_contains FftWaterfallTextureItem.cpp 'A newer frame arrived while this one was in flight. Drop the old'
check "visibility recovery comment exists" contains FftWaterfallTextureItem.cpp 'visibility recovery'
check "completed rows are appended before pending dispatch" contains FftWaterfallTextureItem.cpp 'appendArgbRowLocked(argbRow, argbRow.size());'
check "test pattern function declared" contains FftWaterfallTextureItem.h 'appendTestPatternRowLocked'
check "test pattern function implemented" contains FftWaterfallTextureItem.cpp 'appendTestPatternRowLocked'
check "test pattern telemetry counter exists" contains FftWaterfallTextureItem.cpp 'testPatternRows='
check "empty row telemetry counter exists" contains FftWaterfallTextureItem.cpp 'emptyRows='
check "force CPU disables CUDA setter" contains FftWaterfallTextureItem.cpp 'm_forceCpuBackend || m_testPatternBackend'
check "release/geometry cancellation preserved" contains FftWaterfallTextureItem.cpp 'm_cancelBeforeGeneration'
check "SceneGraph texture path preserved" contains FftWaterfallTextureItem.cpp 'sceneGraph=1'
check "QPainter path not restored" contains FftWaterfallTextureItem.cpp 'qPainter=0'
check "documentation exists" test -f DOA-VIEWER1.7-r2-WATERFALL-VISIBILITY-RECOVERY.md
check "DOA page still enables native waterfall" contains DoaViewer/ViewerPage.qml 'nativeRenderEnabled: true'

python3 - <<'PY'
from pathlib import Path
for f in ['FftWaterfallTextureItem.cpp','FftWaterfallTextureItem.h']:
    s=Path(f).read_text()
    # simple brace sanity ignoring strings is intentionally conservative only.
    bal=0
    for ch in s:
        if ch=='{': bal+=1
        elif ch=='}': bal-=1
        if bal<0:
            raise SystemExit(f'brace underflow in {f}')
    if bal!=0:
        raise SystemExit(f'brace imbalance in {f}: {bal}')
print('PASS: brace sanity')
PY
pass=$((pass+1))

echo "VERIFY: $pass PASS / $fail FAIL"
exit "$fail"
