#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/net_endpoints12_orig}"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

PAGE="$ROOT/ServiceEndpointsPage.qml"
MAIN="$ROOT/MainPage.qml"
SETTING="$ROOT/Setting.qml"
QRC="$ROOT/qml.qrc"
PRO="$ROOT/iScanMR10.pro"
MAINCPP="$ROOT/main.cpp"
BACKEND="$ROOT/iScreenDF/functionTcpServer.cpp"

# Proven old Apply bridge reused by new page.
grep -q 'mainWindows.setNetworkFormDisplay(ip)' "$PAGE" || fail "legacy Mainwindows DF Apply bridge missing"
grep -q 'km.connectToDFserver(ip)' "$PAGE" || fail "existing iScreenDF DF Apply backend missing"
pass "new Endpoints page reuses legacy DF Server Apply bridge"

python3 - "$BACKEND" <<'PY'
import re,sys
s=open(sys.argv[1]).read()
m=re.search(r'void\s+iScreenDF::connectToDFserver\s*\([^)]*\)\s*\{',s)
if not m: raise SystemExit('[FAIL] connectToDFserver implementation missing')
i=m.end(); depth=1; j=i
while j<len(s) and depth:
    if s[j]=='{': depth+=1
    elif s[j]=='}': depth-=1
    j+=1
body=s[i:j]
for token in ['queueUpdateParameterField(db, "ipdfserver"',
              'localDFclient->connectToServer',
              'gpsReader->setGpsdEndpoint',
              'emit updateServeripDfserver']:
    if token not in body:
        raise SystemExit('[FAIL] legacy backend missing token: '+token)
print('[PASS] existing DF Server DB/TCP/GPSD backend contract retained')
PY

# Runtime old top drawer must be gone.
! grep -q 'TopNetworkDrawer {' "$MAIN" || fail "TopNetworkDrawer instance still present in MainPage"
! grep -q 'centerGrabHandle' "$MAIN" || fail "old top-center network handle still present"
! grep -q 'topDrawer.close()' "$MAIN" || fail "MainPage still references old top drawer"
! grep -q 'TopNetworkDrawer.qml' "$QRC" || fail "old TopNetworkDrawer still bundled in qrc"
! grep -q 'FEATURE_TOP_NETWORK_DRAWER' "$PRO" || fail "obsolete qmake feature flag remains"
! grep -q 'FEATURE_TOP_NETWORK_DRAWER' "$MAINCPP" || fail "obsolete C++ feature macro remains"
! grep -q 'FeatureTopNetworkDrawer' "$MAINCPP" || fail "obsolete QML context property remains"
! grep -q 'topNetworkDrawerEnabled' "$SETTING" || fail "Setting still exposes old drawer state"
pass "legacy top-bar network configuration UI removed from runtime"

# New tab remains present and ordered before WiFi.
python3 - "$SETTING" <<'PY'
import sys
s=open(sys.argv[1]).read()
keys=['{ key: "lan"','{ key: "endpoints"','{ key: "wifi"','{ key: "cellular"','{ key: "vpn"']
pos=[s.find(k) for k in keys]
if any(x<0 for x in pos) or pos != sorted(pos):
    raise SystemExit('[FAIL] tab order is not LAN / Endpoints / WiFi / 5G / VPN')
print('[PASS] tab order LAN / Endpoints / WiFi / 5G / VPN retained')
PY

# Backend implementation should be byte-identical to base in this revision.
if [[ -f "$BASE/iScreenDF/functionTcpServer.cpp" ]]; then
    cmp -s "$BASE/iScreenDF/functionTcpServer.cpp" "$BACKEND" || fail "DF backend unexpectedly rewritten"
fi
pass "existing DF/Compass backend implementation reused without rewrite"

# Lightweight structure check for changed QML/C++ files.
python3 - "$PAGE" "$MAIN" "$SETTING" "$MAINCPP" <<'PY'
import sys
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=open(fn,errors='replace').read(); st=[]; i=0; state='code'; q=''
    while i<len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if state=='code':
            if c=='/' and n=='/': state='line'; i+=2; continue
            if c=='/' and n=='*': state='block'; i+=2; continue
            if c in ('"',"'"): state='str'; q=c; i+=1; continue
            if c in '([{': st.append(c)
            elif c in ')]}':
                if not st or st.pop()!=pairs[c]: raise SystemExit(f'[FAIL] delimiter mismatch in {fn}')
        elif state=='line':
            if c=='\n': state='code'
        elif state=='block':
            if c=='*' and n=='/': state='code'; i+=2; continue
        elif state=='str':
            if c=='\\': i+=2; continue
            if c==q: state='code'
        i+=1
    if st: raise SystemExit(f'[FAIL] unclosed delimiter in {fn}')
print('[PASS] lightweight QML/C++ delimiter validation')
PY

pass "NET-ENDPOINTS1.2 structural verification complete"
