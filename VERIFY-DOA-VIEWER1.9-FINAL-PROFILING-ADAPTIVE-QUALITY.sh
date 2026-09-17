#!/usr/bin/env bash
set -euo pipefail

pass=0
fail=0
check() {
    local name="$1"
    shift
    if "$@"; then
        echo "PASS: $name"
        pass=$((pass+1))
    else
        echo "FAIL: $name"
        fail=$((fail+1))
    fi
}
contains() {
    local file="$1"
    local needle="$2"
    grep -Fq "$needle" "$file"
}
not_contains() {
    local file="$1"
    local needle="$2"
    ! grep -Fq "$needle" "$file"
}

check "ViewerPage has adaptive governor property" contains DoaViewer/ViewerPage.qml "adaptiveQualityEnabled"
check "ViewerPage has quality level" contains DoaViewer/ViewerPage.qml "renderQualityLevel"
check "ViewerPage has adaptive log" contains DoaViewer/ViewerPage.qml "[DOA-ADAPTIVE]"
check "Perf log includes target FPS" contains DoaViewer/ViewerPage.qml "targetFps="
check "Perf log includes quality" contains DoaViewer/ViewerPage.qml "quality="
check "Render scheduler uses adaptive interval" contains DoaViewer/ViewerPage.qml "interval: root._schedulerIntervalMs()"
check "Spectrum FPS uses adaptive function" contains DoaViewer/ViewerPage.qml "fftFps: root._spectrumFps()"
check "Waterfall FPS uses adaptive function" contains DoaViewer/ViewerPage.qml "wfFps: root._waterfallFps()"
check "Polar FPS uses adaptive function" contains DoaViewer/ViewerPage.qml "paintFps: root._polarFps()"
check "Settings persist adaptive enable" contains DoaViewer/ViewerPage.qml "property bool adaptiveQualityEnabled: true"
check "Settings persist quality level" contains DoaViewer/ViewerPage.qml "property int  renderQualityLevel: 1"
check "CH1 RX FFT toggle remains present" contains DoaViewer/ViewerPage.qml "setRxFftEnabled"
check "CH1 RX consumer gate preserved" contains DoaViewer/ViewerPage.qml "setDoaRxSpectrumActive(root.visible && root.rxDisplaySelected && root.rxFftEnabled)"
check "Squelch log throttle member exists" contains websocketclient.h "m_squelchLogSuppressed"
check "Squelch log still emits state" contains websocketclient.cpp "[QT5-SQUELCH-RX]"
check "Squelch signal preserved" contains websocketclient.cpp "emit onSQLChanged(sqlOn);"
check "Squelch log suppression counter is reported" contains websocketclient.cpp "suppressed="
check "No accidental CUDA interop claim" not_contains FftWaterfallTextureItem.cpp "cudaInterop=zero-copy"

# Lightweight syntax balance checks for the touched QML file.
python3 - <<'PY'
from pathlib import Path
p = Path('DoaViewer/ViewerPage.qml')
s = p.read_text()
for ch in '{}()[]':
    pass
pairs = {'{':'}','(':')','[':']'}
stack=[]
for i,c in enumerate(s):
    if c in pairs:
        stack.append((c,i))
    elif c in pairs.values():
        if not stack or pairs[stack[-1][0]] != c:
            raise SystemExit(f'Unbalanced {c} at {i}')
        stack.pop()
if stack:
    raise SystemExit(f'Unclosed {stack[-1]}')
print('PASS: ViewerPage bracket balance')
PY
pass=$((pass+1))

echo "VERIFY SUMMARY: ${pass} PASS / ${fail} FAIL"
exit "$fail"
