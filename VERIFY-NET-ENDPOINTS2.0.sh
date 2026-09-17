#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
TCP="$ROOT/iScreenDF/TcpClientDF.cpp"
TCPH="$ROOT/iScreenDF/TcpClientDF.h"
FUNC="$ROOT/iScreenDF/functionTcpServer.cpp"
ISCREEN="$ROOT/iScreenDF/iScreenDF.cpp"
MW="$ROOT/Mainwindows.cpp"
QML="$ROOT/Setting.qml"
DB="$ROOT/iScreenDF/DatabaseDF.cpp"
NC="$ROOT/NetworkController.cpp"
MON="$ROOT/iScreenDF/functionMonitor.cpp"

fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

# 1) LAN3/LAN4 exact protocol mapping remains unchanged.
grep -q 'updateNetworkDfDevice("end0"' "$DB" || fail "LAN3/end0 mapping missing"
grep -q 'updateNetworkDfDevice("end1"' "$DB" || fail "LAN4/end1 mapping missing"
grep -q 'obj\["menuID"\]  = "setIpConfig"' "$FUNC" || fail "setIpConfig menuID missing"
grep -q 'obj\["ifname"\]  = iface' "$FUNC" || fail "setIpConfig ifname missing"
grep -q 'obj\["netmask"\] = mask' "$FUNC" || fail "setIpConfig netmask missing"
grep -q 'obj\["gateway"\] = gw' "$FUNC" || fail "setIpConfig gateway missing"
grep -q 'obj\["dns1"\]    = dns1' "$FUNC" || fail "setIpConfig dns1 missing"
grep -q 'obj\["dns2"\]    = dns2' "$FUNC" || fail "setIpConfig dns2 missing"
pass "LAN3/end0 and LAN4/end1 exact setIpConfig protocol retained"

# 2) Status is the real socket state only.
grep -q 'bool isConnected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }' "$TCPH" || fail "TcpClientDF socket-state truth missing"
grep -q 'const bool connected = backendReady && m_lanIntegrationBackend->isRfsocControlConnected()' "$MW" || fail "Mainwindows status is not using backend TCP truth"
grep -q 'emit rfsocControlConnectionChanged(true)' "$ISCREEN" || fail "connected signal bridge missing"
grep -q 'emit rfsocControlConnectionChanged(false)' "$ISCREEN" || fail "disconnected signal bridge missing"
grep -q 'emit rfsocControlConnectionChanged(isRfsocControlConnected())' "$ISCREEN" || fail "socket-error state republish missing"
pass "LAN3/LAN4 status is authoritative to the live QTcpSocket"

# 3) Synthetic ping heartbeat must be disabled by default.
grep -q 'bool m_heartbeatEnabled = false' "$TCPH" || fail "unsolicited RFSoC heartbeat still enabled by default"
pass "unsolicited RFSoC heartbeat JSON disabled by default"

# 4) setIpConfig must never enter the offline queue.
grep -q 'sendLineIfConnected' "$TCPH" || fail "connected-only send API missing"
grep -q 'sendLineIfConnected(payload, true)' "$FUNC" || fail "setIpConfig not using connected-only sender"
grep -q 'CONTROL_DISCONNECTED' "$FUNC" || fail "disconnected command rejection state missing"
python3 - "$FUNC" <<'PY'
import sys
s=open(sys.argv[1], encoding='utf-8').read()
a=s.index('void iScreenDF::onUpdateNetworkDfDevice(')
b=s.index('void iScreenDF::GetrfsocParameter', a)
body=s[a:b]
for forbidden in ('QUEUED_NO_TARGET', 'QStringLiteral("QUEUED")', 'pendingWriteCount()'):
    assert forbidden not in body, f'setIpConfig path still contains deferred queue logic: {forbidden}'
assert 'sendLineIfConnected(payload, true)' in body
print('[PASS] setIpConfig cannot be queued for delayed delivery')
PY

# 5) Apply must reject before mutation when control TCP is down.
python3 - "$MW" <<'PY'
import sys
s=open(sys.argv[1], encoding='utf-8').read()
a=s.index('bool Mainwindows::applyLanSettings(')
b=s.index('QVariantMap Mainwindows::externalLanStatus', a)
body=s[a:b]
check=body.index('if (remoteLan && !m_lanIntegrationBackend->isRfsocControlConnected())')
mut=body.index('m_lanIntegrationBackend->updateNetworkfromDisplayIndex')
assert check < mut, 'remote TCP check must precede DB mutation'
print('[PASS] disconnected remote Apply is rejected before DB/config mutation')
PY

python3 - "$MON" <<'PY'
import sys
s=open(sys.argv[1], encoding='utf-8').read()
a=s.index('void iScreenDF::updateNetworkfromDisplayIndex(')
b=s.index('void iScreenDF::restartNetworkIndex', a)
body=s[a:b]
check=body.index('if (index == 2 || index == 3)')
mut=body.index('db->updateNetworkfromDisplay')
assert check < mut, 'legacy remote path must check TCP before DB mutation'
assert 'CONTROL_DISCONNECTED' in body
print('[PASS] legacy/alternate LAN3-LAN4 callers are also TCP-gated before DB mutation')
PY

# 6) end0/end1 must not run local NetworkManager mutation.
grep -q 'if (iface.contains("end"))' "$NC" || fail "end* local nmcli skip missing"
grep -q 'nmcli skipped for end\* iface' "$NC" || fail "end* nmcli skip diagnostic missing"
pass "LAN3/LAN4 do not configure local Linux interfaces"

# 7) QML must use the backend TCP preflight and report no-send behavior.
grep -q 'var tcpState = mainWindows.externalLanStatus(index)' "$QML" || fail "QML TCP preflight missing"
grep -q 'RFSoC control server disconnected - command not sent' "$QML" || fail "QML disconnected no-send message missing"
grep -q 'RFSoC control backend is unavailable' "$QML" || fail "remote local-fallback guard missing"
pass "LAN3/LAN4 UI follows actual TCP status and never fakes remote Apply"

# 8) Existing bounded reconnect/shutdown safety remains.
grep -q 'm_reconnectTimer.setSingleShot(true)' "$TCP" || fail "bounded reconnect timer regressed"
grep -q 'm_reconnectMaxMs = 60000' "$TCPH" || fail "reconnect cap regressed"
grep -q 'm_shuttingDown' "$TCPH" || fail "shutdown guard regressed"
grep -q 'm_userDisconnect' "$TCPH" || fail "user disconnect guard regressed"
pass "STAB2 reconnect/shutdown safety retained"

# 9) Delimiter sanity on touched source files.
python3 - "$TCP" "$TCPH" "$FUNC" "$ISCREEN" "$MW" "$MON" "$QML" <<'PY'
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
                raise SystemExit(f'{fn}: delimiter mismatch at {i}')
            stack.pop()
        i+=1
    if stack:
        raise SystemExit(f'{fn}: unclosed delimiters {stack[-8:]}')
print('[PASS] touched C++/QML delimiter validation')
PY

# 10) Existing DoA local bridge and status badge UX remain present.
grep -q 'doaClient.setHost(QStringLiteral("127.0.0.1"))' "$ROOT/main.cpp" || fail "DoA bridge host regressed"
grep -q 'doaClient.setPort(9000)' "$ROOT/main.cpp" || fail "DoA bridge port regressed"
grep -q 'id: lanStatusBadge' "$QML" || fail "1.9 responsive status badge regressed"
grep -q 'lanInfoCompactStatusText' "$QML" || fail "1.9 compact LAN status text regressed"
pass "DoA bridge and NET-ENDPOINTS1.9 status badge UX retained"

pass "NET-ENDPOINTS2.0 structural verification complete"
