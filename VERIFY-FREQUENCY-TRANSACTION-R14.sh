#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
pass=0
fail=0
check() {
  local desc="$1"; shift
  if "$@"; then echo "PASS: $desc"; pass=$((pass+1)); else echo "FAIL: $desc"; fail=$((fail+1)); fi
}
contains() { grep -q -- "$2" "$1"; }
not_contains() { ! grep -q -- "$2" "$1"; }

check "WebSocketClient exposes atomic frequencyStateChanged" contains websocketclient.h 'void frequencyStateChanged'
check "WebSocketClient emits atomic snapshot" contains websocketclient.cpp 'emit frequencyStateChanged'
check "Mainwindows consumes atomic WebSocket snapshot" contains Mainwindows.cpp 'WebSocketClient::frequencyStateChanged'
check "Mainwindows exposes atomic QML snapshot" contains Mainwindows.h 'void frequencyStateChanged(double centerHz'
check "Spectrum connects atomic snapshot" contains SpectrumGLPlot.qml 'mainWindows.frequencyStateChanged.connect(applyBackendFrequencyState)'
check "Spectrum no longer connects legacy center signal" not_contains SpectrumGLPlot.qml 'updateCenterFreq.connect'
check "Spectrum no longer connects legacy receiver signal" not_contains SpectrumGLPlot.qml 'updateReceiverFreq.connect'
check "No implicit setCenterFreq property writer" not_contains SpectrumGLPlot.qml 'property real setCenterFreq'
check "No onSetCenterFreqChanged network side effect" not_contains SpectrumGLPlot.qml 'onSetCenterFreqChanged'
check "Out-of-span transaction has generation guard" contains SpectrumGLPlot.qml 'frequencyRequestGeneration'
check "Pending offsets have generation guard" contains SpectrumGLPlot.qml 'pendingOffsetGeneration'
check "Stale center snapshots are discarded" contains SpectrumGLPlot.qml 'QML-FREQ-STALE-SNAPSHOT-DISCARD'
check "Confirmed snapshot is committed atomically" contains SpectrumGLPlot.qml 'frequencyStateApplying = true'
check "Confirmed snapshot has one commit marker" contains SpectrumGLPlot.qml 'QML-FREQ-SNAPSHOT-COMMIT'
check "Preset tuning uses central API" contains SpectrumGLPlot.qml 'function requestPresetFrequency'
check "Peak scan waits for confirmed center" contains SpectrumGLPlot.qml 'function onFrequencySnapshotCommitted'
check "Mode change command no longer writes offset" not_contains ReceiveModeScanner.qml '"offset_freq":radioScanner.spectrumGLPlot.offsetFrequency'
check "MemoryAddEdit uses central frequency API" contains MemoryAddEdit.qml 'requestPresetFrequency'
check "NewMemoryAddEdit uses central frequency API" contains NewMemoryAddEdit.qml 'requestPresetFrequency'
check "LogDeviceScanner uses central frequency API" contains LogDeviceScanner.qml 'requestPresetFrequency'
check "No external direct centerFreq assignment" bash -c '! grep -RInE "spectrumGLPlot\\.centerFreq[[:space:]]*=" --include="*.qml" . >/dev/null'
check "No external direct offsetFrequency assignment" bash -c '! grep -RInE "spectrumGLPlot\\.offsetFrequency[[:space:]]*=" --include="*.qml" . >/dev/null'
check "No direct QML setfrequency sender outside Spectrum" bash -c '! grep -RIn "sendmessage.*setfrequency" --include="*.qml" . | grep -v "SpectrumGLPlot.qml" >/dev/null'

python3 - <<'PY'
from pathlib import Path
root=Path('.')
pairs={'}':'{',')':'(',']':'['}
failed=[]
for path in root.glob('*.qml'):
    s=path.read_text(errors='ignore')
    stack=[]; i=0; line=1; quote=None; esc=False; lc=False; bc=False
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if c=='\n': line+=1; lc=False
        if lc: i+=1; continue
        if bc:
            if c=='*' and n=='/': bc=False; i+=2; continue
            i+=1; continue
        if quote:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': lc=True; i+=2; continue
        if c=='/' and n=='*': bc=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '({[': stack.append((c,line))
        elif c in ')}]':
            if not stack or stack[-1][0] != pairs[c]:
                failed.append(f'{path}:{line}: unexpected {c}')
                break
            stack.pop()
        i+=1
    if stack:
        failed.append(f'{path}: unclosed {stack[-3:]}')
if failed:
    print('FAIL: QML structural balance')
    print('\n'.join(failed[:20]))
    raise SystemExit(1)
print(f'PASS: QML structural balance ({len(list(root.glob("*.qml")))} files)')
PY
pass=$((pass+1))

echo "R14 verification: PASS=$pass FAIL=$fail"
if (( fail != 0 )); then exit 1; fi
