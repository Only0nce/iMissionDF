#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
QML="$ROOT/Setting.qml"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

[ -f "$QML" ] || fail "Setting.qml missing"

grep -q 'id: networkAccessSwitchButton' "$QML" || fail "access switch button missing"
grep -q 'onClicked: networkManager.requestNetworkAccessToggle()' "$QML" || fail "access switch is not clickable"
grep -q 'function requestNetworkAccessToggle()' "$QML" || fail "role toggle function missing"
grep -q 'setNetworkAccessRole("viewer")' "$QML" || fail "Admin -> Viewer downgrade missing"
grep -q 'networkModePasswordPopup.requestUnlock()' "$QML" || fail "Viewer -> Admin password gate missing"
grep -q 'id: networkModePasswordPopup' "$QML" || fail "in-page admin password popup missing"
grep -q 'onAuthorized: networkManager.setNetworkAccessRole("admin")' "$QML" || fail "authorized elevation missing"
grep -q 'if (!networkAdminMode && !canEditCurrentLan())' "$QML" || fail "protected draft reload on downgrade missing"
grep -q 'if (!canEditLanByIndex(index))' "$QML" || fail "backend-facing LAN permission guard missing"
grep -q 'property bool lanModeDirty: false' "$QML" || fail "DHCP draft guard missing"
grep -q 'color: useDhcp ? "#303740" : ui.field' "$QML" || fail "DHCP disabled visual state missing"
pass "in-page Viewer/Admin switch and existing permission/DHCP guards present"

python3 - "$QML" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text()
# Lightweight structural balance check. Strip quoted strings and comments enough
# to catch accidental block damage without pretending to be a QML parser.
out=[]
i=0
state='code'
quote=''
while i < len(s):
    c=s[i]
    n=s[i+1] if i+1 < len(s) else ''
    if state=='code':
        if c=='/' and n=='/': state='line'; i+=2; continue
        if c=='/' and n=='*': state='block'; i+=2; continue
        if c in ('"', "'"): state='str'; quote=c; i+=1; continue
        out.append(c); i+=1; continue
    if state=='line':
        if c=='\n': state='code'; out.append('\n')
        i+=1; continue
    if state=='block':
        if c=='*' and n=='/': state='code'; i+=2; continue
        i+=1; continue
    if state=='str':
        if c=='\\': i+=2; continue
        if c==quote: state='code'
        i+=1; continue
code=''.join(out)
pairs={'}':'{',')':'(',']':'['}
stack=[]
for c in code:
    if c in '{([': stack.append(c)
    elif c in '})]':
        if not stack or stack[-1]!=pairs[c]:
            raise SystemExit(f'[FAIL] QML delimiter mismatch at {c}')
        stack.pop()
if stack:
    raise SystemExit(f'[FAIL] QML unclosed delimiters: {stack[-10:]}')
print('[PASS] lightweight QML delimiter validation')
PY

# Production backend must stay byte-identical to the supplied parent revision when
# an adjacent reference directory is available during development verification.
REF="/mnt/data/net_role_switch_orig"
if [ -d "$REF" ]; then
    for f in NetworkController.cpp Mainwindows.cpp iScreenDF/DatabaseDF.cpp iScreenDF/functionTcpServer.cpp iScreenDF/functionMonitor.cpp; do
        cmp -s "$ROOT/$f" "$REF/$f" || fail "backend changed unexpectedly: $f"
    done
    pass "network/database/RFSoC backend files unchanged"
fi

echo "[PASS] NET-AUTH2.1 structural verification complete"
