#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

SETTING="$ROOT/Setting.qml"
PAGE="$ROOT/ServiceEndpointsPage.qml"
QRC="$ROOT/qml.qrc"
HDR="$ROOT/iScreenDF/iScreenDF.h"
CPP="$ROOT/iScreenDF/functionTcpServer.cpp"
MAIN="$ROOT/main.cpp"

for f in "$SETTING" "$PAGE" "$QRC" "$HDR" "$CPP" "$MAIN"; do
  [[ -f "$f" ]] || fail "missing $f"
done

python3 - "$SETTING" <<'PY'
import re,sys
s=open(sys.argv[1],encoding='utf-8').read()
m=re.search(r'model:\s*\[(.*?)\]\s*\n\s*Button\s*\{', s, re.S)
if not m:
    raise SystemExit('[FAIL] Network tab model not found')
block=m.group(1)
keys=re.findall(r'key:\s*"([^"]+)"', block)
want=['lan','endpoints','wifi','cellular','vpn']
if keys[:5] != want:
    raise SystemExit(f'[FAIL] tab order {keys[:5]} != {want}')
print('[PASS] Network tab order LAN / Endpoints / WiFi / 5G / VPN')
PY

grep -q 'source: active ? "qrc:/ServiceEndpointsPage.qml" : ""' "$SETTING" || fail "Endpoints loader missing"
grep -q '<file>ServiceEndpointsPage.qml</file>' "$QRC" || fail "ServiceEndpointsPage missing from qrc"
grep -q 'Viewer access enabled: LAN1-LAN2, Endpoints, WiFi and 5G editable' "$SETTING" || fail "Viewer Endpoints policy text missing"
! grep -q 'property bool adminMode' "$PAGE" || fail "Endpoints page unexpectedly gated by adminMode"
pass "Endpoints tab is available and editable in Viewer/Admin"

grep -q 'text: "DF Server Endpoint"' "$PAGE" || fail "DF Server card missing"
grep -q 'text: "Apply"' "$PAGE" || fail "DF Apply button missing"
grep -q 'text: "Reconnect"' "$PAGE" || fail "DF Reconnect button missing"
grep -q 'text: "Global Offsets"' "$PAGE" || fail "Global Offsets card missing"
grep -q 'text: "Compass Offset"' "$PAGE" || fail "Compass Offset field missing"
grep -q 'text: "Set Compass Offset"' "$PAGE" || fail "Set Compass Offset button missing"
pass "requested Service Endpoints controls present"

grep -q 'setContextProperty("Krakenmapval", kraken)' "$MAIN" || fail "Krakenmapval QML context missing"
grep -q 'typeof Krakenmapval' "$PAGE" || fail "Endpoints page does not bind current Krakenmapval context"
grep -q 'void reconnectDFserver();' "$HDR" || fail "reconnectDFserver public slot missing"
grep -q 'void requestServiceEndpointsState();' "$HDR" || fail "service state request slot missing"
grep -q 'void iScreenDF::reconnectDFserver()' "$CPP" || fail "reconnectDFserver implementation missing"
grep -q 'void iScreenDF::requestServiceEndpointsState()' "$CPP" || fail "service state request implementation missing"
pass "QML-to-iScreenDF endpoint bridge present"

# Apply must preserve the existing DB + runtime mutation path.
python3 - "$CPP" <<'PY'
import re,sys
s=open(sys.argv[1],encoding='utf-8').read()
def body(name):
    m=re.search(r'void\s+iScreenDF::'+re.escape(name)+r'\s*\([^)]*\)\s*\{',s)
    if not m: raise SystemExit(f'[FAIL] {name} missing')
    i=m.end(); depth=1; j=i
    while j<len(s) and depth:
        if s[j]=='{': depth+=1
        elif s[j]=='}': depth-=1
        j+=1
    return s[i:j-1]
apply=body('connectToDFserver')
for token in ['queueUpdateParameterField(db, "ipdfserver"', 'localDFclient->connectToServer', 'gpsReader->setGpsdEndpoint']:
    if token not in apply: raise SystemExit(f'[FAIL] DF Apply missing {token}')
reconn=body('reconnectDFserver')
for token in ['localDFclient->connectToServer', 'gpsReader->setGpsdEndpoint']:
    if token not in reconn: raise SystemExit(f'[FAIL] reconnect missing {token}')
if 'queueUpdateParameterField' in reconn:
    raise SystemExit('[FAIL] Reconnect unexpectedly rewrites DB')
compass=body('setCompassOffset')
if 'queueUpdateParameterField(db, "compass_offset"' not in compass:
    raise SystemExit('[FAIL] Compass Offset persistence missing')
print('[PASS] Apply/Reconnect/Compass backend semantics preserved')
PY

# Structural balance for the modified/new QML and C++ bridge files.
python3 - "$PAGE" "$SETTING" "$HDR" "$CPP" <<'PY'
import sys
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=open(fn,encoding='utf-8',errors='replace').read(); st=[]; i=0; state='code'; q=''
    while i<len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if state=='code':
            if c=='/' and n=='/': state='line'; i+=2; continue
            if c=='/' and n=='*': state='block'; i+=2; continue
            if c in ('"',"'"): state='str'; q=c; i+=1; continue
            if c in '([{': st.append(c)
            elif c in ')]}':
                if not st or st[-1]!=pairs[c]: raise SystemExit(f'[FAIL] delimiter mismatch in {fn}')
                st.pop()
        elif state=='line':
            if c=='\n': state='code'
        elif state=='block':
            if c=='*' and n=='/': state='code'; i+=2; continue
        elif state=='str':
            if c=='\\': i+=2; continue
            if c==q: state='code'
        i+=1
    if st: raise SystemExit(f'[FAIL] unclosed delimiter in {fn}: {st[-5:]}')
print('[PASS] lightweight QML/C++ delimiter validation')
PY

pass "NET-ENDPOINTS1 structural verification complete"
