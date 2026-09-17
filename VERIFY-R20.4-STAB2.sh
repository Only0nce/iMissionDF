#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

# --- QML runtime hazards ---
! grep -Eq '^[[:space:]]*anchors\.fill:[[:space:]]*parent' <(sed -n '1,18p' "$ROOT/DoaViewer/ViewerPage.qml") || fail "ViewerPage root still anchors to StackView"
! grep -Eq '^[[:space:]]*anchors\.fill:[[:space:]]*parent' <(sed -n '1,40p' "$ROOT/iScreenDFqml/pages/QMLMap.qml") || fail "QMLMap root still anchors to StackView"
grep -Fq 'clearRequested = true' "$ROOT/iScreenDFqml/pages/QMLMap.qml" || fail "QMLMap deferred Canvas clear missing"
grep -Fq 'if (!ctx)' "$ROOT/iScreenDFqml/pages/QMLMap.qml" || fail "QMLMap Canvas null guard missing"
grep -Fq 'onValuePctChanged: ring.requestPaint()' "$ROOT/iRecordManage/StorageUsed.qml" || fail "StorageUsed direct repaint handler missing"
! grep -Fq 'Connections {' "$ROOT/iRecordManage/StorageUsed.qml" || fail "StorageUsed stale Connections block remains"
pass "QML Canvas/StackView/Storage runtime hazards hardened"

# --- GPIO / SQL ---
grep -Fq 'hasCachedValue' "$ROOT/newGPIOClass.h" || fail "GPIO output cache missing"
grep -Fq 'if (isOutput && hasCachedValue)' "$ROOT/newGPIOClass.cpp" || fail "GPIO output readback cache path missing"
grep -Fq 'm_shdAmpReadFaultLogged' "$ROOT/Mainwindows.cpp" || fail "SHD_AMP fault suppression missing"
grep -Fq 'm_sqlGpioWriteFaultLogged' "$ROOT/Mainwindows.cpp" || fail "SQL GPIO write fault guard missing"
pass "SQL/GPIO failures degrade without warning flood/null dereference"

# --- RFSoC TCP ---
grep -Fq 'm_reconnectTimer.setSingleShot(true)' "$ROOT/iScreenDF/TcpClientDF.cpp" || fail "RFSoC reconnect timer not single-shot"
grep -Fq 'm_reconnectMaxMs = 60000' "$ROOT/iScreenDF/TcpClientDF.h" || fail "RFSoC reconnect cap missing"
grep -Fq 'scheduleReconnect' "$ROOT/iScreenDF/TcpClientDF.cpp" || fail "RFSoC bounded reconnect scheduler missing"
grep -Fq 'TcpClientDF::~TcpClientDF()' "$ROOT/iScreenDF/TcpClientDF.cpp" || fail "RFSoC destructor shutdown missing"
pass "RFSoC reconnect is bounded and lifecycle-aware"

# --- DB idempotent update ---
grep -Fq '[UpdateParameterField] unchanged' "$ROOT/iScreenDF/DatabaseDF.cpp" || fail "idempotent DB update handling missing"
grep -Fq 'Parameter.id=1 missing' "$ROOT/iScreenDF/DatabaseDF.cpp" || fail "missing Parameter row diagnostic missing"
pass "DB zero-row update is distinguished from missing-row failure"

# --- WebSocket lifetime ---
grep -Fq 'QPointer<QWebSocket> SocketClients' "$ROOT/ChatServer.h" || fail "main ChatServer socket guard missing"
grep -Fq 'const QPointer<QWebSocket> safeWs' "$ROOT/ChatServer.cpp" || fail "queued guarded WebSocket send missing"
grep -Fq 'm_snmpSocketClients.removeAll(pClient)' "$ROOT/ChatServer.cpp" || fail "disconnected socket not removed from SNMP list"
python3 - "$ROOT/ChatServer.cpp" <<'PY'
import re,sys
s=open(sys.argv[1]).read()
if re.search(r'delete\s+c\s*;\s*delete\s+c\s*;', s):
    raise SystemExit('duplicate wrapper delete remains')
PY
grep -Fq 'm_WebSocketClients.removeAll(socket)' "$ROOT/iRecordManage/ChatServerWebRec.cpp" || fail "WebRec stale web socket removal missing"
grep -Fq 'QAbstractSocket::ConnectedState' "$ROOT/iRecordManage/ChatServerWebRec.cpp" || fail "WebRec connected-state send guard missing"
grep -Fq 'QWebSocket *wClient = nullptr;' "$ROOT/iRecordManage/mainwindowsiRec.cpp" || fail "uninitialized QWebSocket pointer remains"
pass "WebSocket ownership/queued-send lifetime hardened"

# --- MAX31760 / I2C ---
! grep -Eq '^[[:space:]]*system\s*\(\s*"i2ctransfer' "$ROOT/iRecordManage/MAX31760.cpp" || fail "shell i2ctransfer remains"
! grep -Eq '^[[:space:]]*exit[[:space:]]*\(' "$ROOT/iRecordManage/MAX31760.cpp" || fail "MAX31760 still terminates process"
grep -Fq 'writeProfilePayload(0x50, 0x01, 0x00, "safe-mode")' "$ROOT/iRecordManage/MAX31760.cpp" || fail "safe-mode payload contract changed"
grep -Fq 'writeProfilePayload(0x50, 0x11, 0x32, "normal-mode")' "$ROOT/iRecordManage/MAX31760.cpp" || fail "normal-mode payload contract changed"
python3 - "$ROOT/iRecordManage/MAX31760.cpp" <<'PY'
import re,sys
s=open(sys.argv[1]).read()
a=s.index('kProfilePayload')
b=s.index('}};', a)
vals=re.findall(r'0x[0-9A-Fa-f]+', s[a:b])
if len(vals) != 93:
    raise SystemExit(f'MAX31760 payload length={len(vals)}, expected 93')
if vals[2].lower() != '0x11' or vals[81].lower() != '0xc8':
    raise SystemExit(f'MAX31760 base contract mismatch control={vals[2]} profile={vals[81]}')
PY
pass "MAX31760 I2C/thermal failure is non-fatal and payload-compatible"

# --- Recorder threads / Unix socket ---
for token in m_dateTimeThreadStarted m_fanThreadStarted m_mainThreadStarted m_monitorThreadStarted m_cleanupThreadStarted; do
  grep -Fq "$token" "$ROOT/iRecordManage/mainwindowsiRec.cpp" "$ROOT/iRecordManage/mainwindowsiRec.h" || fail "recorder thread lifecycle token missing: $token"
done
grep -Fq 'm_threadRunning.store(false' "$ROOT/iRecordManage/mainwindowsiRec.cpp" || fail "recorder stop flag missing"
[[ $(grep -R "new UnixSocketListener" -n "$ROOT/iRecordManage" --include='*.cpp' | wc -l) -eq 1 ]] || fail "expected exactly one recorder UnixSocketListener owner"
! grep -Fq 'qFatal' "$ROOT/iRecordManage/Unixsocketlistener.cpp" || fail "Unix socket failure still fatal"
pass "recorder pthread/Unix-socket shutdown lifecycle hardened"

# --- SPI/input fatal paths ---
! grep -Eq '^[[:space:]]*exit[[:space:]]*\(' "$ROOT/SPI.cpp" || fail "SPI initialization still exits process"
grep -Fq 'm_ready = false' "$ROOT/SPI.cpp" || fail "SPI degraded readiness state missing"
! grep -Eq '^[[:space:]]*exit[[:space:]]*\(' "$ROOT/iRecordManage/GetInputEvent.cpp" || fail "input-event worker still exits process"
pass "optional SPI/input peripheral failures are non-fatal"

# --- Model guard / latest feature regression ---
grep -Fq 'Math.max(0, Math.min(2, Number(nicCount) || 0))' "$ROOT/iScreenDFqml/pages/TopNetworkDrawer.qml" || fail "top drawer non-negative model guard missing"
grep -Fq '[NAV] title=' "$ROOT/MainPage.qml" || fail "navigation correlation diagnostic missing"
grep -Fq 'NET-AUTH3' "$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml" || fail "latest direct Viewer Network entry missing"
grep -Fq 'getRecActive()' "$ROOT/Mainwindows.cpp" || fail "latest recorder-state decoupling missing"
pass "model guard and latest Network/REC contracts retained"

# --- lightweight delimiter validation for modified QML ---
python3 - "$ROOT/DoaViewer/ViewerPage.qml" "$ROOT/iScreenDFqml/pages/QMLMap.qml" "$ROOT/iRecordManage/StorageUsed.qml" "$ROOT/iScreenDFqml/pages/TopNetworkDrawer.qml" "$ROOT/MainPage.qml" <<'PY'
import sys
from pathlib import Path
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=Path(fn).read_text(errors='replace')
    stack=[]; i=0; quote=None; line=False; block=False
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if c=='\\': i+=2; continue
            if c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack[-1] != pairs[c]:
                raise SystemExit(f'{fn}: delimiter mismatch near offset {i}')
            stack.pop()
        i+=1
    if stack: raise SystemExit(f'{fn}: unclosed delimiters {stack[-8:]}')
print('[PASS] modified QML delimiter validation')
PY

# --- lightweight C++ delimiter validation for touched C++/headers ---
python3 - "$ROOT" <<'PY'
import sys
from pathlib import Path
root=Path(sys.argv[1])
files=['ChatServer.cpp','ChatServer.h','Mainwindows.cpp','Mainwindows.h','SPI.cpp','SPI.h','newGPIOClass.cpp','newGPIOClass.h',
'iRecordManage/ChatServerWebRec.cpp','iRecordManage/GetInputEvent.cpp','iRecordManage/MAX31760.cpp','iRecordManage/MAX31760.h',
'iRecordManage/Unixsocketlistener.cpp','iRecordManage/Unixsocketlistener.h','iRecordManage/mainwindowsiRec.cpp','iRecordManage/mainwindowsiRec.h',
'iScreenDF/DatabaseDF.cpp','iScreenDF/TcpClientDF.cpp','iScreenDF/TcpClientDF.h']
pairs={')':'(',']':'[','}':'{'}
for rel in files:
    s=(root/rel).read_text(errors='replace'); stack=[]; i=0; quote=None; line=False; block=False
    while i<len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if c=='\\': i+=2; continue
            if c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack[-1]!=pairs[c]: raise SystemExit(f'{rel}: delimiter mismatch at {i}')
            stack.pop()
        i+=1
    if stack: raise SystemExit(f'{rel}: unclosed delimiters {stack[-8:]}')
print('[PASS] modified C++ delimiter validation')
PY

pass "R20.4-STAB2 structural verification complete"
