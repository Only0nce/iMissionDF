#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
BASE="${2:-/mnt/data/doa_study_110/net_endpoint_110_work}"
cd "$ROOT"
pass=0; fail=0
check(){ local n="$1"; shift; if "$@"; then echo "PASS  $n"; pass=$((pass+1)); else echo "FAIL  $n"; fail=$((fail+1)); fi; }

check "six logical dropdown choices" grep -Fq 'model: ["CH1","CH2","CH3","CH4","CH5","CH6"]' DoaViewer/FftControlPanel.qml
check "CH1 is RX/Home source" grep -Fq 'logical=CH1 source=ASTRARX_HOME no_setAdcChannel=1' DoaViewer/ViewerPage.qml
check "DF mapping is logical minus two" grep -Fq 'var physicalDfChannel = ch - 2' DoaViewer/ViewerPage.qml
check "RX uses DoA FftPlot renderer" grep -Fq 'freqHz: root.rxDisplaySelected ? root.rxFftFreqHz : doaClient.fftFreqHz' DoaViewer/ViewerPage.qml
check "RX uses DoA Waterfall renderer" grep -Fq 'waterfallRowDb: root.rxDisplaySelected ? root.rxFftMagDb : doaClient.fftMagDb' DoaViewer/ViewerPage.qml
check "RX click-to-DF offset disabled" grep -Fq 'clickOffsetEnabled: !root.rxDisplaySelected' DoaViewer/ViewerPage.qml
check "RX DF target overlay disabled" grep -Fq 'bandBwHz: root.rxDisplaySelected ? 0 : doaClient.doaBwHz' DoaViewer/ViewerPage.qml
check "named FFT consumer API" grep -Fq 'setFftConsumerActive(const QString &consumer, bool active)' websocketclient.h
check "Home retains compatibility owner" grep -Fq 'setFftConsumerActive(QStringLiteral("home-spectrum"), active)' websocketclient.cpp
check "DoA RX owns independent FFT lease" grep -Fq 'setFftConsumerActive(QStringLiteral("doa-viewer-rx"), active)' Mainwindows.cpp
check "RX QML bridge bounded to 1024 bins" grep -Fq 'm_doaRxDisplayBins = 1024' Mainwindows.h
check "RX bridge throttled" grep -Fq 'm_doaRxPublishIntervalMs = 33' Mainwindows.h
check "native Home spectrum signal reused" grep -Fq '&WebSocketClient::spectrumDisplayFrame' Mainwindows.cpp

if [[ -d "$BASE" ]]; then
  check "DoaClient backend unchanged" cmp -s "$BASE/DoaViewer/DoaClient.cpp" DoaViewer/DoaClient.cpp
  check "DoaClient header unchanged" cmp -s "$BASE/DoaViewer/DoaClient.h" DoaViewer/DoaClient.h
  check "DoA Polar unchanged" cmp -s "$BASE/DoaViewer/DoaPolarPlot.qml" DoaViewer/DoaPolarPlot.qml
fi

python3 - "$ROOT" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
files=[root/'DoaViewer/ViewerPage.qml',root/'DoaViewer/TopBar.qml',root/'DoaViewer/FftControlPanel.qml',root/'Mainwindows.cpp',root/'Mainwindows.h',root/'websocketclient.cpp',root/'websocketclient.h']

def stripped(text):
    # Character-state scanner: preserves newlines while hiding comments and
    # quoted strings, so // inside a string cannot corrupt delimiter checks.
    out=[]; i=0; state='code'
    while i < len(text):
        c=text[i]; n=text[i+1] if i+1 < len(text) else ''
        if state == 'code':
            if c == '/' and n == '/': out += [' ',' ']; i += 2; state='line'; continue
            if c == '/' and n == '*': out += [' ',' ']; i += 2; state='block'; continue
            if c == '"': out.append(' '); i += 1; state='dq'; continue
            if c == "'": out.append(' '); i += 1; state='sq'; continue
            out.append(c); i += 1; continue
        if state == 'line':
            out.append('\n' if c == '\n' else ' ')
            if c == '\n': state='code'
            i += 1; continue
        if state == 'block':
            if c == '*' and n == '/': out += [' ',' ']; i += 2; state='code'; continue
            out.append('\n' if c == '\n' else ' '); i += 1; continue
        quote = '"' if state == 'dq' else "'"
        if c == '\\' and i+1 < len(text):
            out += [' ',' ']; i += 2; continue
        out.append('\n' if c == '\n' else ' ')
        if c == quote: state='code'
        i += 1
    return ''.join(out)

pairs={')':'(',']':'[','}':'{'}
opens=set(pairs.values())
bad=[]
for p in files:
    text=stripped(p.read_text(errors='replace'))
    stack=[]
    for idx,c in enumerate(text):
        if c in opens:
            stack.append((c,idx))
        elif c in pairs:
            if not stack or stack[-1][0] != pairs[c]:
                line=text.count('\n',0,idx)+1
                bad.append(f'{p.name}: unexpected {c} at line {line}')
                break
            stack.pop()
    if stack:
        c,idx=stack[-1]
        line=text.count('\n',0,idx)+1
        bad.append(f'{p.name}: unmatched {c} at line {line}')
if bad:
    print('BALANCE FAIL')
    print('\n'.join(bad))
    raise SystemExit(1)
print('PASS  lexical delimiter balance')
PY
pass=$((pass+1))

printf '\nStatic result: %d PASS / %d FAIL\n' "$pass" "$fail"
(( fail == 0 ))
