#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
pass=0
fail=0
check(){
  local name="$1"; shift
  if "$@" >/dev/null 2>&1; then
    echo "PASS: $name"; pass=$((pass+1));
  else
    echo "FAIL: $name"; fail=$((fail+1));
  fi
}
check "revision marker" grep -q "DOA-VIEWER1.8-STREAMING-GPU-TEXTURE" FftWaterfallTextureItem.cpp
check "upload env" grep -q "ISCAN_DOA_WATERFALL_UPLOAD" FftWaterfallTextureItem.cpp
check "streaming texture class" grep -q "DoaWaterfallStreamingTexture" FftWaterfallTextureItem.cpp
check "dynamic texture" grep -q "QSGDynamicTexture" FftWaterfallTextureItem.cpp
check "gl sub image path" grep -q "glTexSubImage2D" FftWaterfallTextureItem.cpp
check "safe fallback path" grep -q "createTextureFromImage" FftWaterfallTextureItem.cpp
check "stream telemetry" grep -q "streamFrames" FftWaterfallTextureItem.cpp
check "recreate telemetry" grep -q "recreateFrames" FftWaterfallTextureItem.cpp
check "upload stage telemetry" grep -q "cudaInterop=upload-stage" FftWaterfallTextureItem.cpp
check "header stream counters" grep -q "m_streamTextureFrames" FftWaterfallTextureItem.h
check "note exists" test -f DOA-VIEWER1.8-STREAMING-GPU-TEXTURE-UPLOAD.md
# Simple brace sanity for touched C++ files.
python3 - <<'PY'
from pathlib import Path
for f in [Path('FftWaterfallTextureItem.cpp'), Path('FftWaterfallTextureItem.h')]:
    s=f.read_text()
    if s.count('{') != s.count('}'):
        raise SystemExit(f'brace mismatch: {f}')
print('BRACE_SANITY=PASS')
PY
pass=$((pass+1))
if (( fail > 0 )); then
  echo "VERIFY: $pass PASS / $fail FAIL"
  exit 1
fi
echo "VERIFY: $pass PASS / $fail FAIL"
