#!/usr/bin/env bash
set -euo pipefail
fail=0
pass=0
check(){
  local name="$1"; shift
  if "$@"; then echo "PASS: $name"; pass=$((pass+1)); else echo "FAIL: $name"; fail=$((fail+1)); fi
}
contains(){ grep -q -- "$2" "$1"; }
not_contains(){ ! grep -q -- "$2" "$1"; }
check "new FftWaterfallTextureItem header exists" test -f FftWaterfallTextureItem.h
check "new FftWaterfallTextureItem source exists" test -f FftWaterfallTextureItem.cpp
check "registered QML type" contains main.cpp 'qmlRegisterType<FftWaterfallTextureItem>'
check "main includes waterfall texture item" contains main.cpp 'FftWaterfallTextureItem.h'
check "pro includes source for X86/JETSON" bash -c '[[ $(grep -c "FftWaterfallTextureItem.cpp" iScanMR10.pro) -ge 2 ]]'
check "pro includes header for X86/JETSON" bash -c '[[ $(grep -c "FftWaterfallTextureItem.h" iScanMR10.pro) -ge 2 ]]'
check "WaterfallCanvas uses new GPU texture item" contains DoaViewer/WaterfallCanvas.qml 'FftWaterfallTextureItem'
check "WaterfallCanvas no longer instantiates FftDisplayItem for native waterfall" not_contains DoaViewer/WaterfallCanvas.qml 'mode: FftDisplayItem.Waterfall'
check "QPainter not used by new waterfall item" not_contains FftWaterfallTextureItem.cpp 'QPainter'
check "SceneGraph texture node is used" contains FftWaterfallTextureItem.cpp 'QSGSimpleTextureNode'
check "texture created from image" contains FftWaterfallTextureItem.cpp 'createTextureFromImage'
check "event-driven submitExternalFrame exists" contains FftWaterfallTextureItem.cpp 'submitExternalFrame'
check "budget gate exists" contains FftWaterfallTextureItem.cpp 'skipBudget'
check "pending gate exists" contains FftWaterfallTextureItem.cpp 'skipPending'
check "peak-preserving pooling comment exists" contains FftWaterfallTextureItem.cpp 'Peak-preserving bucket pooling'
check "clearHistory invokable exists" contains FftWaterfallTextureItem.h 'Q_INVOKABLE void clearHistory'
check "diagnostic log exists" contains FftWaterfallTextureItem.cpp '\[DOA-GPU-WATERFALL\]'
check "1.6 startup log exists" contains FftWaterfallTextureItem.cpp '\[DOA-VIEWER1.6-GPU-WATERFALL\]'
python3 - <<'PY'
from pathlib import Path
pairs={'(':')','{':'}','[':']'}
for f in ['FftWaterfallTextureItem.h','FftWaterfallTextureItem.cpp','DoaViewer/WaterfallCanvas.qml']:
    text=Path(f).read_text()
    st=[]
    in_s=in_d=False; esc=False; line=False; block=False
    for ch,nx in zip(text, text[1:]+''):
        if line:
            if ch=='\n': line=False
            continue
        if block:
            if ch=='*' and nx=='/': block=False; esc=True
            continue
        if esc:
            esc=False; continue
        if in_s:
            if ch=='\\': esc=True
            elif ch=="'": in_s=False
            continue
        if in_d:
            if ch=='\\': esc=True
            elif ch=='"': in_d=False
            continue
        if ch=='/' and nx=='/': line=True; esc=True; continue
        if ch=='/' and nx=='*': block=True; esc=True; continue
        if ch=="'": in_s=True; continue
        if ch=='"': in_d=True; continue
        if ch in pairs: st.append(pairs[ch])
        elif ch in pairs.values():
            if not st or st.pop()!=ch:
                raise SystemExit(f'unbalanced {f}')
    if st: raise SystemExit(f'unclosed {f}: {st[-10:]}')
print('PASS: structural delimiter balance')
PY
pass=$((pass+1))
echo "VERIFY: $pass PASS / $fail FAIL"
exit "$fail"
