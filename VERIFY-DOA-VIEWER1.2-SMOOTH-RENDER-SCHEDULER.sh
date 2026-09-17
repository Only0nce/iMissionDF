#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
cd "$ROOT"
pass=0; fail=0
check(){ local name="$1"; shift; if "$@"; then echo "PASS  $name"; pass=$((pass+1)); else echo "FAIL  $name"; fail=$((fail+1)); fi; }
check "Viewer display cache properties" grep -q 'displayFftMagDb' DoaViewer/ViewerPage.qml
check "Viewer render scheduler" grep -q 'id: renderScheduler' DoaViewer/ViewerPage.qml
check "Viewer perf telemetry" grep -q '\[DOA-VIEWER-PERF\]' DoaViewer/ViewerPage.qml
check "FftPlot uses scheduled display mag" grep -q 'magDb: root.displayFftMagDb' DoaViewer/ViewerPage.qml
check "Waterfall uses scheduled display mag" grep -q 'waterfallRowDb: root.displayFftMagDb' DoaViewer/ViewerPage.qml
check "Waterfall frame sequence wired" grep -q 'frameSequence: root.displayFrameSequence' DoaViewer/ViewerPage.qml
check "Waterfall source key wired" grep -q 'sourceKey: root.logicalSourceName()' DoaViewer/ViewerPage.qml
check "Waterfall sequence gate" grep -q 'root.frameSequence === root._lastFrameSequence' DoaViewer/WaterfallCanvas.qml
check "Waterfall duplicate fallback does not fake-scroll" grep -q 'if (key === tick._lastKey) return' DoaViewer/WaterfallCanvas.qml
check "Waterfall run-length drawing" grep -q 'runLen' DoaViewer/WaterfallCanvas.qml
check "Waterfall debug off default" grep -q 'property bool showDebug: false' DoaViewer/WaterfallCanvas.qml
check "Polar paint scheduler" grep -q 'property int paintFps' DoaViewer/DoaPolarPlot.qml
check "Polar duplicate gate removed" bash -c "[[ \$(grep -c 'NO SIGNAL (DOA gated)' DoaViewer/DoaPolarPlot.qml) -eq 1 ]]"
check "FftPlot no direct fftChanged repaint" bash -c "! grep -q 'function onFftChanged' DoaViewer/FftPlot.qml"
check "RX display publish interval bounded" grep -q 'm_doaRxPublishIntervalMs = 40' Mainwindows.h
check "RX display bins bounded" grep -q 'm_doaRxDisplayBins = 768' Mainwindows.h
# simple active QML balance checks, ignoring the large commented legacy block in Waterfall
python3 - <<'PY'
from pathlib import Path
files = ['DoaViewer/ViewerPage.qml','DoaViewer/FftPlot.qml','DoaViewer/DoaPolarPlot.qml']
# for Waterfall, only the active file starts at the second import
wf = Path('DoaViewer/WaterfallCanvas.qml').read_text()
idx = wf.find('import QtQuick 2.15', wf.find('import QtQuick 2.15') + 1)
active = wf[idx:] if idx >= 0 else wf
texts = [(f, Path(f).read_text()) for f in files] + [('DoaViewer/WaterfallCanvas.qml(active)', active)]
def clean(s):
    out=[]; i=0; st='code'
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1 < len(s) else ''
        if st=='code':
            if c=='/' and n=='/': st='line'; i+=2; continue
            if c=='"': st='dq'; i+=1; continue
            if c=="'": st='sq'; i+=1; continue
            out.append(c); i+=1
        elif st=='line':
            if c=='\n': st='code'; out.append('\n')
            i+=1
        elif st=='dq':
            if c=='\\': i+=2; continue
            if c=='"': st='code'
            i+=1
        elif st=='sq':
            if c=='\\': i+=2; continue
            if c=="'": st='code'
            i+=1
    return ''.join(out)
for name, text in texts:
    c = clean(text)
    assert c.count('{') == c.count('}'), (name, c.count('{'), c.count('}'))
    assert c.count('(') == c.count(')'), (name, c.count('('), c.count(')'))
print('PASS  QML structural balance')
PY
pass=$((pass+1))
printf '\nStatic result: %d PASS / %d FAIL\n' "$pass" "$fail"
(( fail == 0 ))
