#!/usr/bin/env bash
set -euo pipefail
fail=0
pass(){ echo "PASS: $1"; }
check(){ local desc="$1"; shift; if "$@"; then pass "$desc"; else echo "FAIL: $desc"; fail=$((fail+1)); fi }

check "in-flight output width member exists" grep -q "m_inFlightOutputWidth" FftWaterfallTextureItem.h
check "CUDA dispatch records intended output width" grep -q "m_inFlightOutputWidth = outputWidth" FftWaterfallTextureItem.cpp
check "row completion computes desired output width" grep -q "desiredOutputWidth" FftWaterfallTextureItem.cpp
check "ARGB append uses desired output width, not argbRow.size" grep -q "appendArgbRowLocked(argbRow, desiredOutputWidth)" FftWaterfallTextureItem.cpp
check "dB fallback uses desired output width" grep -q "appendFrameLocked(dbRow, desiredOutputWidth)" FftWaterfallTextureItem.cpp
check "pending dispatch preserves next output width" grep -q "m_inFlightOutputWidth = nextWidth" FftWaterfallTextureItem.cpp
check "width normalization telemetry exists" grep -q "widthNormalizedRows" FftWaterfallTextureItem.cpp
check "ARGB input width telemetry exists" grep -q "argbInW" FftWaterfallTextureItem.cpp
check "desired output width telemetry exists" grep -q "desiredW" FftWaterfallTextureItem.cpp
check "1.7-r4 runtime banner exists" grep -q "DOA-VIEWER1.7-r4-WATERFALL-WIDTH-STABILITY" FftWaterfallTextureItem.cpp
check "in-flight width reset on clear/disable/lifecycle" bash -c 'test $(grep -c "m_inFlightOutputWidth = 0" FftWaterfallTextureItem.cpp) -ge 5'
check "brace sanity cpp" bash -c 'python3 - <<PY
from pathlib import Path
s=Path("FftWaterfallTextureItem.cpp").read_text()
assert s.count("{")==s.count("}"), (s.count("{"), s.count("}"))
PY'
check "brace sanity header" bash -c 'python3 - <<PY
from pathlib import Path
s=Path("FftWaterfallTextureItem.h").read_text()
assert s.count("{")==s.count("}"), (s.count("{"), s.count("}"))
PY'

if [ "$fail" -ne 0 ]; then
  echo "VERIFY: $fail FAIL"
  exit 1
fi
echo "VERIFY: 13 PASS / 0 FAIL"
