#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
pass=0
fail=0
check(){
    local name="$1"; shift
    if "$@"; then echo "PASS $name"; pass=$((pass+1)); else echo "FAIL $name"; fail=$((fail+1)); fi
}
contains(){ grep -R --fixed-strings --quiet -- "$1" "$2"; }

check "FftControlPanel exposes rxFftEnabled property" contains "property bool rxFftEnabled: true" "$ROOT/DoaViewer/FftControlPanel.qml"
check "FftControlPanel exposes CH1 toggle signal" contains "signal rxFftEnabledRequested(bool enabled)" "$ROOT/DoaViewer/FftControlPanel.qml"
check "CH1 switch is enabled when RX source exists" contains "enabled: root.displayChannel === 1 ? root.rxSourceAvailable : doaClient.connected" "$ROOT/DoaViewer/FftControlPanel.qml"
check "CH1 switch emits rxFftEnabledRequested" contains "root.rxFftEnabledRequested(checked)" "$ROOT/DoaViewer/FftControlPanel.qml"
check "CH2-CH6 still use doaClient.spectrumEnabled" contains "doaClient.spectrumEnabled = checked" "$ROOT/DoaViewer/FftControlPanel.qml"

check "TopBar forwards rxFftEnabled property" contains "property bool rxFftEnabled: true" "$ROOT/DoaViewer/TopBar.qml"
check "TopBar forwards rxFftEnabled request" contains "signal rxFftEnabledRequested(bool enabled)" "$ROOT/DoaViewer/TopBar.qml"
check "TopBar passes rxFftEnabled to FftControlPanel" contains "rxFftEnabled: root.rxFftEnabled" "$ROOT/DoaViewer/TopBar.qml"

check "ViewerPage owns CH1 FFT gate" contains "property bool rxFftEnabled: true" "$ROOT/DoaViewer/ViewerPage.qml"
check "RX consumer is gated by CH1 FFT switch" contains "m.setDoaRxSpectrumActive(root.visible && root.rxDisplaySelected && root.rxFftEnabled)" "$ROOT/DoaViewer/ViewerPage.qml"
check "RX frame publishing is gated by CH1 FFT switch" contains "if (!root.rxSourceAvailable || !root.rxFftEnabled) return" "$ROOT/DoaViewer/ViewerPage.qml"
check "ViewerPage provides setRxFftEnabled" contains "function setRxFftEnabled(enabled)" "$ROOT/DoaViewer/ViewerPage.qml"
check "ViewerPage logs CH1 FFT toggles" contains "[DOA-RX-FFT]" "$ROOT/DoaViewer/ViewerPage.qml"
check "FftPlot CH1 enabled includes rxFftEnabled" contains "enabled: root.rxDisplaySelected ? (root.rxSourceAvailable && root.rxFftEnabled) : doaClient.spectrumEnabled" "$ROOT/DoaViewer/ViewerPage.qml"
check "CH1 FFT state persisted in Settings" contains "property bool rxFftEnabled: true" "$ROOT/DoaViewer/ViewerPage.qml"
check "CH1 FFT request wired from TopBar" contains "onRxFftEnabledRequested: root.setRxFftEnabled(enabled)" "$ROOT/DoaViewer/ViewerPage.qml"

ROOT_PATH="$ROOT" python3 - <<'PY'
import os
from pathlib import Path
files = [
    Path('DoaViewer/FftControlPanel.qml'),
    Path('DoaViewer/TopBar.qml'),
    Path('DoaViewer/ViewerPage.qml'),
]
root = Path(os.environ['ROOT_PATH'])
for rel in files:
    text = (root / rel).read_text()
    # Lightweight sanity only; QML JavaScript may contain braces in comments/strings.
    if text.count('{') != text.count('}'):
        raise SystemExit(f'brace imbalance: {rel}')
print('PASS qml brace sanity')
PY
pass=$((pass+1))

echo "VERIFY: ${pass} PASS / ${fail} FAIL"
exit "$fail"
