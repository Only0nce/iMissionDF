#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
pass(){ echo "[PASS] $*"; }
fail(){ echo "[FAIL] $*" >&2; exit 1; }

POP="$ROOT/NetworkAccessModePopup.qml"
LWCPP="$ROOT/logwatcher.cpp"
LWH="$ROOT/logwatcher.h"
WSCPP="$ROOT/websocketclient.cpp"
WSH="$ROOT/websocketclient.h"
MAIN="$ROOT/main.cpp"

[[ $(grep -c 'contentItem: Item' "$POP") -ge 2 ]] || fail "popup content wrappers missing"
[[ $(grep -c 'implicitHeight: 0' "$POP") -ge 2 ]] || fail "popup implicit-height break missing"
! grep -q 'contentItem: ColumnLayout' "$POP" || fail "direct Button contentItem ColumnLayout remains"
pass "Viewer/Admin Button implicitHeight cycle removed"

grep -q '~LogWatcher() override' "$LWH" || fail "LogWatcher destructor missing"
grep -q 'void stopWatching()' "$LWH" || fail "LogWatcher stop API missing"
grep -q 'LogWatcher::~LogWatcher' "$LWCPP" || fail "LogWatcher destructor implementation missing"
grep -q 'tailProcess->terminate()' "$LWCPP" || fail "tail terminate missing"
grep -q 'tailProcess->kill()' "$LWCPP" || fail "tail kill fallback missing"
pass "tail QProcess shutdown lifecycle present"

grep -q 'void shutdown();' "$WSH" || fail "WebSocketClient shutdown API missing"
grep -q 'void WebSocketClient::shutdown()' "$WSCPP" || fail "WebSocketClient shutdown implementation missing"
grep -q 'webSocket.abort()' "$WSCPP" || fail "shutdown WebSocket abort missing"
grep -q 'hdAudioPlayer->stop()' "$WSCPP" || fail "HD audio stop missing"
grep -q 'sdAudioPlayer->stop()' "$WSCPP" || fail "SD audio stop missing"
grep -q 'mainWindows.wsClient.shutdown();' "$MAIN" || fail "pre-teardown ws/audio shutdown missing"
python3 - "$MAIN" <<'PY'
import sys
s=open(sys.argv[1]).read()
a=s.index('mainWindows.wsClient.shutdown();')
b=s.index('engine.reset();')
assert a < b, 'audio shutdown must precede QML engine destruction'
PY
pass "AstraRX/audio workers stop before QML/backend teardown"

python3 - "$POP" "$LWCPP" "$LWH" "$WSCPP" "$WSH" "$MAIN" <<'PY'
import sys
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=open(fn).read(); stack=[]; state='code'; quote=''; i=0; line=1
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1 < len(s) else ''
        if c=='\n': line += 1
        if state=='code':
            if c=='/' and n=='/': state='line'; i+=2; continue
            if c=='/' and n=='*': state='block'; i+=2; continue
            if c in ('"', "'"): state='str'; quote=c; i+=1; continue
            if c in '([{': stack.append((c,line))
            elif c in ')]}':
                if not stack or stack[-1][0] != pairs[c]:
                    raise SystemExit(f'{fn}: delimiter mismatch {c} at line {line}')
                stack.pop()
        elif state=='line':
            if c=='\n': state='code'
        elif state=='block':
            if c=='*' and n=='/': state='code'; i+=2; continue
        elif state=='str':
            if c=='\\': i+=2; continue
            if c==quote: state='code'
        i+=1
    if stack: raise SystemExit(f'{fn}: unclosed delimiter {stack[-1]}')
print('[PASS] modified QML/C++ delimiter validation')
PY

pass "NET-STAB1 structural verification complete"
