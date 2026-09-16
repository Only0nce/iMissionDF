#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

MAIN="$ROOT/MainPage.qml"
SETTING="$ROOT/Setting.qml"
QRC="$ROOT/qml.qrc"
PRO="$ROOT/iScanMR10.pro"
MAINCPP="$ROOT/main.cpp"
TOP="$ROOT/iScreenDFqml/pages/TopNetworkDrawer.qml"
ENDPOINTS="$ROOT/ServiceEndpointsPage.qml"

[[ -f "$TOP" ]] || fail "TopNetworkDrawer.qml missing"
grep -q 'TopNetworkDrawer {' "$MAIN" || fail "TopNetworkDrawer runtime instance missing"
grep -q 'id: topDrawer' "$MAIN" || fail "topDrawer id missing"
grep -q 'centerGrabHandle' "$MAIN" || fail "top-center drawer handle missing"
grep -q 'FeatureTopNetworkDrawer' "$MAIN" || fail "MainPage feature binding missing"
grep -q '<file>iScreenDFqml/pages/TopNetworkDrawer.qml</file>' "$QRC" || fail "TopNetworkDrawer missing from qrc"
grep -q 'CONFIG += FEATURE_TOP_NETWORK_DRAWER' "$PRO" || fail "TopNetworkDrawer qmake feature is not enabled"
grep -q 'setContextProperty("FeatureTopNetworkDrawer"' "$MAINCPP" || fail "FeatureTopNetworkDrawer QML context missing"
pass "legacy top-bar Network Settings drawer restored to runtime"

grep -q '{ key: "endpoints", label: "Endpoints", enabled: true }' "$SETTING" || fail "new Endpoints tab missing"
grep -q 'mainWindows.setNetworkFormDisplay(ip)' "$ENDPOINTS" || fail "legacy Mainwindows DF bridge missing from Endpoints"
grep -q 'km.connectToDFserver(ip)' "$ENDPOINTS" || fail "DF server backend missing from Endpoints"
grep -q 'km.setCompassOffset(value)' "$ENDPOINTS" || fail "Compass backend missing from Endpoints"
pass "new Endpoints page retained with legacy backend reuse"

# Ensure restored legacy drawer is not calling the dead/commented legacy method.
python3 - "$TOP" <<'PY2'
import sys
for raw in open(sys.argv[1], encoding="utf-8"):
    code = raw.split("//", 1)[0]
    if "connectToserverKraken(" in code:
        raise SystemExit("active connectToserverKraken() call found in restored drawer")
PY2
pass "restored drawer avoids dead connectToserverKraken backend call"

python3 - "$MAIN" "$SETTING" "$TOP" "$ENDPOINTS" <<'PY'
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
    if stack: raise SystemExit(f'{fn}: unclosed delimiters {stack[-10:]}')
print('[PASS] QML delimiter validation')
PY

pass "NET-ENDPOINTS1.3 structural verification complete"
