#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PAGE="$ROOT/ServiceEndpointsPage.qml"
HDR="$ROOT/iScreenDF/iScreenDF.h"
CPP="$ROOT/iScreenDF/functionTcpServer.cpp"
TOP="$ROOT/iScreenDFqml/pages/TopNetworkDrawer.qml"

pass(){ echo "[PASS] $*"; }
fail(){ echo "[FAIL] $*" >&2; exit 1; }

# New page must consume existing APIs/signals.
grep -q 'km.getNetworkfromDb(1)' "$PAGE" || fail 'Endpoints does not use existing DB/network refresh path'
grep -q 'function onNetworkRowUpdated(row)' "$PAGE" || fail 'Endpoints does not consume existing Network2 DB signal'
grep -q 'function onUpdateServeripDfserver(ip)' "$PAGE" || fail 'existing DF server signal missing'
grep -q 'function onUpdateGlobalOffsets(offsetValue, compassOffset)' "$PAGE" || fail 'existing global offsets signal missing'
grep -q 'km.connectToDFserver(ip)' "$PAGE" || fail 'existing Apply backend missing'
grep -q 'km.connectToDFserver(savedIp)' "$PAGE" || fail 'Reconnect does not reuse existing DF backend'
grep -q 'km.setCompassOffset(value)' "$PAGE" || fail 'existing Compass backend missing'
pass 'Endpoints uses existing DB/signals/DF/Compass backend only'

# NET-ENDPOINTS1-added backend APIs must be gone.
if grep -Rq 'requestServiceEndpointsState' "$PAGE" "$HDR" "$CPP"; then
    fail 'requestServiceEndpointsState addition still present'
fi
if grep -Rq 'reconnectDFserver' "$PAGE" "$HDR" "$CPP" "$TOP"; then
    fail 'reconnectDFserver addition still present'
fi
pass 'no Service Endpoints-specific backend API remains'

# Original mutation owners remain.
grep -q 'void connectToDFserver(const QString &ip);' "$HDR" || fail 'connectToDFserver declaration missing'
grep -q 'void setCompassOffset(double offset);' "$HDR" || fail 'setCompassOffset declaration missing'
grep -q 'void iScreenDF::connectToDFserver(const QString &ip)' "$CPP" || fail 'connectToDFserver implementation missing'
grep -q 'void iScreenDF::setCompassOffset(double offset)' "$CPP" || fail 'setCompassOffset implementation missing'
pass 'original DF/Compass mutation owners retained'

# connectToDFserver should be back to pre-ENDPOINTS notification semantics.
python3 - "$CPP" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
a=s.index('void iScreenDF::connectToDFserver(const QString &ip)')
b=s.index('void iScreenDF::applyRfsocParameterToServer(bool needAck)', a)
body=s[a:b]
if 'emit updateServeripDfserver' in body:
    raise SystemExit('[FAIL] added updateServeripDfserver emission still present in connectToDFserver')
print('[PASS] connectToDFserver restored to existing backend behavior')
PY

# Top drawer remains available and also uses existing DF backend.
grep -q 'krakenmapval.connectToDFserver(serverField.text)' "$TOP" || fail 'legacy Top Network Drawer DF backend missing'
pass 'temporary legacy Top Network Drawer remains on existing backend'

# Lightweight delimiter scanner for modified QML/C++.
python3 - "$PAGE" "$TOP" "$HDR" "$CPP" <<'PY'
from pathlib import Path
import sys

def strip(src):
    out=[]; i=0; n=len(src); state='code'; quote=''
    while i<n:
        c=src[i]; d=src[i+1] if i+1<n else ''
        if state=='code':
            if c=='/' and d=='/': state='line'; out.extend('  '); i+=2; continue
            if c=='/' and d=='*': state='block'; out.extend('  '); i+=2; continue
            if c in ('"', "'"): state='str'; quote=c; out.append(' '); i+=1; continue
            out.append(c); i+=1; continue
        if state=='line':
            if c=='\n': state='code'; out.append('\n')
            else: out.append(' ')
            i+=1; continue
        if state=='block':
            if c=='*' and d=='/': state='code'; out.extend('  '); i+=2
            else: out.append('\n' if c=='\n' else ' '); i+=1
            continue
        if state=='str':
            if c=='\\': out.extend('  '); i+=2; continue
            if c==quote: state='code'
            out.append(' '); i+=1
    return ''.join(out)

pairs={'}':'{',')':'(',']':'['}
for fn in sys.argv[1:]:
    src=strip(Path(fn).read_text())
    stack=[]
    for lineno,line in enumerate(src.splitlines(),1):
        for ch in line:
            if ch in '{([': stack.append((ch,lineno))
            elif ch in '})]':
                if not stack or stack[-1][0] != pairs[ch]:
                    raise SystemExit(f'[FAIL] delimiter mismatch {fn}:{lineno}')
                stack.pop()
    if stack:
        raise SystemExit(f'[FAIL] unclosed delimiter {fn}:{stack[-1][1]}')
print('[PASS] modified QML/C++ delimiter validation')
PY

# Preserve key latest product integrations.
grep -q '{ key: "endpoints", label: "Endpoints"' "$ROOT/Setting.qml" || fail 'Endpoints tab missing'
grep -q 'networkAccessRole.*viewer' "$ROOT/Setting.qml" || fail 'Viewer default contract missing'
grep -q 'TopNetworkDrawer' "$ROOT/MainPage.qml" || fail 'temporary Top Network Drawer restore missing'
pass 'latest Network Settings/Viewer/legacy drawer integration retained'

echo '[PASS] NET-ENDPOINTS1.4 structural verification complete'
