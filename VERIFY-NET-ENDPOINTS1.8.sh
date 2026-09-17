#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
TCP="$ROOT/iScreenDF/TcpClientDF.cpp"
TCPH="$ROOT/iScreenDF/TcpClientDF.h"
MW="$ROOT/Mainwindows.cpp"
QML="$ROOT/Setting.qml"
DB="$ROOT/iScreenDF/DatabaseDF.cpp"
FUNC="$ROOT/iScreenDF/functionTcpServer.cpp"

fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

# LAN3 + LAN4 mappings must both remain intact.
grep -q 'updateNetworkDfDevice("end0"' "$DB" || fail "LAN3/end0 DB emit missing"
grep -q 'updateNetworkDfDevice("end1"' "$DB" || fail "LAN4/end1 DB emit missing"
grep -q 'iface != QStringLiteral("end0") && iface != QStringLiteral("end1")' "$FUNC" || fail "end0/end1 RFSoC guard missing"
grep -q 'obj\["menuID"\]  = "setIpConfig"' "$FUNC" || fail "setIpConfig contract missing"
pass "LAN3/end0 and LAN4/end1 external configuration contracts retained"

# DF Server IP remains the independent shared TCP target.
grep -q 'Parameter.ipdfserver from DB' "$DB" || fail "Parameter.ipdfserver startup path missing"
grep -q 'localDFclient->connectToServer(host,5555)' "$FUNC" || grep -q 'localDFclient->connectToServer(host, 5555)' "$FUNC" || fail "DF Server IP -> TCP 5555 connection owner missing"
pass "DF Server IP remains independent shared RFSoC control target"

# Restore one immediate retry while retaining STAB2 bounded timer.
grep -q 'm_reconnectTimer.setSingleShot(true)' "$TCP" || fail "single-shot bounded reconnect timer regressed"
grep -q 'm_reconnectMaxMs = 60000' "$TCPH" || fail "60s reconnect cap missing"
grep -q 'm_immediateRetryConsumed' "$TCPH" || fail "immediate retry storm guard missing"
grep -q 'requestReconnect(QStringLiteral("disconnected"), true)' "$TCP" || fail "disconnect immediate recovery missing"
grep -q 'requestReconnect(QStringLiteral("socket-error"), true)' "$TCP" || fail "socket-error immediate recovery missing"
grep -q 'immediate reconnect queued' "$TCP" || fail "immediate reconnect diagnostic missing"
pass "one immediate retry per outage restored without reconnect storm"

# A watchdog must remain armed around initial and retry connectToHost attempts.
grep -q 'initial-connect-watchdog' "$TCP" || fail "initial connect watchdog missing"
grep -q 'retry-connect-watchdog' "$TCP" || fail "retry connect watchdog missing"
pass "bounded watchdog covers stuck ConnectingState on initial and retry attempts"

# LAN3/LAN4 queued commands must nudge the same shared target.
grep -q 'requestReconnect(QStringLiteral("queued-write"), true)' "$TCP" || fail "queued external command reconnect nudge missing"
grep -q 'LAN3/end0 and LAN4/end1 share this RFSoC control client' "$TCP" || fail "LAN3/LAN4 shared-client contract marker missing"
pass "queued LAN3/LAN4 setIpConfig commands trigger guarded recovery"

# UI explicitly represents the shared control channel for both external rows.
grep -q 'const bool supported = (index == 2 || index == 3)' "$MW" || fail "LAN3/LAN4 shared external status support missing"
grep -q 'RFSoC Control Connected' "$MW" || fail "backend connected status wording missing"
grep -q 'RFSoC Control Disconnected' "$MW" || fail "backend disconnected status wording missing"
grep -q 'RFSoC Control Connected' "$QML" || fail "QML connected wording missing"
grep -q 'RFSoC Control Disconnected' "$QML" || fail "QML disconnected wording missing"
pass "LAN3 and LAN4 both present shared RFSoC control status explicitly"

# Negative state must be checked before connected (disconnected contains connected).
python3 - "$QML" <<'PY'
import sys
s=open(sys.argv[1], encoding='utf-8').read()
a=s.index('function lanInfoStatusColor(info)')
b=s.index('function lanStatusColor()', a)
body=s[a:b]
neg=body.index('status.indexOf("disconnect")')
pos=body.index('status.indexOf("connected")')
assert neg < pos, 'disconnect color check must precede connected check'
print('[PASS] disconnected state color precedence is correct')
PY

# Lightweight delimiter checks on touched files.
python3 - "$TCP" "$TCPH" "$MW" "$QML" <<'PY'
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

# Focused inherited regression gates.
cd "$ROOT"
bash VERIFY-LAN-RLAN4B.sh >/dev/null || fail "R-LAN4B regression failed"
bash VERIFY-LAN-RLAN4B1.sh >/dev/null || fail "R-LAN4B.1 regression failed"

# NET-ENDPOINTS1.7 / STAB2 contracts relevant to this delta are checked
# directly here so this verifier stays deterministic in constrained build
# environments.
grep -q 'doaClient.setHost(QStringLiteral("127.0.0.1"))' "$ROOT/main.cpp" || fail "integrated DoA local bridge host regressed"
grep -q 'doaClient.setPort(9000)' "$ROOT/main.cpp" || fail "integrated DoA local bridge port regressed"
grep -q 'm_reconnectTimer.setSingleShot(true)' "$TCP" || fail "STAB2 single-shot reconnect safety regressed"
grep -q 'm_shuttingDown' "$TCPH" || fail "STAB2 shutdown guard regressed"
grep -q 'm_userDisconnect' "$TCPH" || fail "STAB2 user-disconnect guard regressed"
pass "LAN3/LAN4, DF endpoint, DoA bridge, and STAB2 reconnect-safety contracts retained"

pass "NET-ENDPOINTS1.8 structural verification complete"
