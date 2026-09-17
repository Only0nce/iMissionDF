#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

NC_H=NetworkController.h
NC_CPP=NetworkController.cpp
MW=Mainwindows.cpp
MAIN=main.cpp
ENDPOINTS=ServiceEndpointsPage.qml
SIDE=iScreenDFqml/pages/SideSettingsDrawer.qml
TOP=DoaViewer/TopBar.qml
DFSRV=iScreenDF/functionTcpServer.cpp
DFCLIENT=iScreenDF/functionTcpClient.cpp
DB=iScreenDF/DatabaseDF.cpp
DOACPP=DoaViewer/DoaClient.cpp

# LAN3 remains the existing CONFIG path.
grep -q 'id == 3' "$DB" || fail "Network2 row 3 mapping missing"
grep -q 'updateNetworkDfDevice("end0"' "$DB" || fail "LAN3 -> end0 DB emit missing"
grep -q 'obj\["menuID"\].*= "setIpConfig"' "$DFSRV" || fail "setIpConfig menuID missing"
grep -q 'obj\["ifname"\].*= iface' "$DFSRV" || fail "setIpConfig ifname missing"
grep -q 'iface == "end0".*"rfsoc1"' "$NC_CPP" || fail "end0 -> rfsoc1 persistence mapping missing"
pass "LAN3 remains Network2 row3 -> end0 -> setIpConfig + lan.rfsoc1"

# DF endpoint stays independent and is the remote CONNECT owner.
grep -q 'queueUpdateParameterField(db, "ipdfserver"' "$DFSRV" || fail "Parameter.ipdfserver persistence missing"
grep -q 'localDFclient->connectToServer(p->m_ipdfServer, 5555)' "$DFSRV" || fail "DF control connect owner missing"
python - <<'PY'
from pathlib import Path
s=Path('iScreenDF/functionTcpServer.cpp').read_text()
a=s.index('void iScreenDF::connectToDFserver')
b=s.index('void iScreenDF::applyRfsocParameterToServer', a)
body=s[a:b]
assert 'setIpConfig' not in body, 'DF Server IP must not configure RFSoC end0/end1'
assert 'updateNetworkfromDisplay' not in body, 'DF Server IP must not update LAN3 Network2'
PY
pass "DF Server IP remains independent Parameter.ipdfserver -> RFSoC :5555 connect target"

# DF endpoint mirror uses the protected file writer and does not touch lan.rfsoc1.
grep -q 'persistDfServerEndpoint' "$NC_H" || fail "DF endpoint persistence helper declaration missing"
grep -q 'persistDfServerEndpoint' "$NC_CPP" || fail "DF endpoint persistence helper implementation missing"
grep -q 'updateNetworkConfigRoot' "$NC_CPP" || fail "shared atomic JSON updater missing"
grep -q 'QStringLiteral("endpoints")' "$NC_CPP" || fail "endpoints JSON section missing"
grep -q 'QStringLiteral("dfServerIp")' "$NC_CPP" || fail "dfServerIp JSON field missing"
grep -q 'persistDfServerEndpoint(ip, &msg)' "$MW" || fail "existing Mainwindows endpoint hook does not mirror JSON"
python - <<'PY'
from pathlib import Path
s=Path('NetworkController.cpp').read_text()
a=s.index('bool NetworkController::persistDfServerEndpoint')
b=s.index('// ============================================================\n// LAN apply', a)
body=s[a:b]
assert 'rfsoc1' not in body and 'end0' not in body, 'DF endpoint mirror must not mutate LAN3'
PY
pass "DF Server IP is mirrored atomically to endpoints.dfServerIp without LAN3 coupling"

# Integrated DoA Viewer consumes local bridge; remote link remains owned by iScreenDF.
grep -q 'doaClient.setHost(QStringLiteral("127.0.0.1"))' "$MAIN" || fail "integrated DoA host is not localhost"
grep -q 'doaClient.setPort(9000)' "$MAIN" || fail "integrated DoA port is not 9000"
grep -q 'doaClient.host = "127.0.0.1"' "$SIDE" || fail "DoA navigation does not enforce local bridge host"
grep -q 'doaClient.port = 9000' "$SIDE" || fail "DoA navigation does not enforce local bridge port"
grep -q 'integratedBridgeMode' "$TOP" || fail "TopBar integrated bridge mode missing"
grep -q 'readOnly: root.integratedBridgeMode' "$TOP" || fail "integrated bridge endpoint fields are not protected"
pass "integrated DoA Viewer uses local iScreenDF bridge 127.0.0.1:9000"

# Prove live receive flow RFSoC -> bridge -> DoaClient parser.
grep -q 'tcpServerDF->broadcastLine(jsonLine)' "$DFCLIENT" || fail "RFSoC receive path does not broadcast to local DoA clients"
grep -q 'if (menu == "DoAResult")' "$DOACPP" || fail "DoaClient DoAResult parser missing"
grep -q 'm_fftFreqHz = fList' "$DOACPP" || fail "DoaClient live FFT update missing"
grep -q 'm_doaDeg = newDoa' "$DOACPP" || fail "DoaClient live DoA update missing"
pass "received RFSoC DoAResult/FFT data reaches DoaClient live properties"

# Page Apply still uses established owners.
grep -q 'mainWindows.setNetworkFormDisplay(ip)' "$ENDPOINTS" || fail "endpoint file mirror hook missing from Apply"
grep -q 'km.connectToDFserver(ip)' "$ENDPOINTS" || fail "existing DF connect backend missing from Apply"
pass "Endpoints Apply uses file mirror + existing DF DB/connect owner"

# Basic delimiter checks for changed source files.
python3 - <<'PY2'
from pathlib import Path
pairs={')':'(',']':'[','}':'{'}
for fn in ['NetworkController.h','NetworkController.cpp','Mainwindows.cpp','main.cpp',
           'ServiceEndpointsPage.qml','iScreenDFqml/pages/SideSettingsDrawer.qml','DoaViewer/TopBar.qml']:
    s=Path(fn).read_text(errors='replace')
    stack=[]; i=0; quote=None; line=False; block=False
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1 < len(s) else ''
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
    if stack:
        raise SystemExit(f'{fn}: unclosed delimiters {stack[-8:]}')
print('[PASS] modified C++/QML delimiter validation')
PY2

# Regression gates.
bash VERIFY-LAN-RLAN4B.sh >/dev/null || fail "R-LAN4B regression failed"
bash VERIFY-LAN-RLAN4B1.sh >/dev/null || fail "R-LAN4B.1 regression failed"
bash VERIFY-NET-ENDPOINTS1.6.sh >/dev/null || fail "NET-ENDPOINTS1.6 regression failed"
bash VERIFY-R20.4-STAB2.sh >/dev/null || fail "R20.4-STAB2 regression failed"
pass "LAN3/DF endpoint/STAB2 regressions retained"
pass "NET-ENDPOINTS1.7 structural verification complete"
