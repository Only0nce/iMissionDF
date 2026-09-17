#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PAGE="$ROOT/ServiceEndpointsPage.qml"
HDR="$ROOT/iScreenDF/iScreenDF.h"
CPP="$ROOT/iScreenDF/functionTcpServer.cpp"
DB="$ROOT/iScreenDF/DatabaseDF.cpp"

pass(){ echo "[PASS] $*"; }
fail(){ echo "[FAIL] $*" >&2; exit 1; }

# Existing authoritative startup DB path must remain intact.
grep -q 'GetIPDFServerFromDB();' "$DB" || fail 'startup Parameter.ipdfserver read missing'
grep -q 'void DatabaseDF::GetIPDFServerFromDB()' "$DB" || fail 'GetIPDFServerFromDB implementation missing'
grep -q 'qry.value("ipdfserver")' "$DB" || fail 'Parameter.ipdfserver extraction missing'
grep -q 'void iScreenDF::GetIPDFServer(const QString &ip)' "$CPP" || fail 'existing DF startup receiver missing'
grep -q 'm_parameter.first()->m_ipdfServer = host' "$CPP" || fail 'runtime DF endpoint cache assignment missing'
pass 'existing Parameter.ipdfserver startup path retained'

# Existing replay method must be callable from QML, not duplicated.
grep -q 'void updateIPServerDF();' "$HDR" || fail 'existing updateIPServerDF not exposed through meta-object slot'
grep -q 'void iScreenDF::updateIPServerDF()' "$CPP" || fail 'existing updateIPServerDF implementation missing'
grep -q 'emit updateServeripDfserver(p->m_ipdfServer);' "$CPP" || fail 'existing updateServeripDfserver replay missing'
pass 'existing DF endpoint replay function reused'

# Endpoints must replay Parameter IP before Network2 fallback.
python3 - "$PAGE" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
a=s.index('function requestLegacyState()')
b=s.index('function applyDfServer()', a)
body=s[a:b]
replay=body.find('km.updateIPServerDF()')
legacy=body.find('km.getNetworkfromDb(1)')
if replay < 0:
    raise SystemExit('[FAIL] Endpoints does not request existing Parameter.ipdfserver replay')
if legacy < 0:
    raise SystemExit('[FAIL] existing Network2 fallback removed unexpectedly')
if replay > legacy:
    raise SystemExit('[FAIL] Network2 fallback runs before authoritative Parameter.ipdfserver replay')
print('[PASS] Parameter.ipdfserver replay precedes Network2 fallback')
PY

grep -q 'function onUpdateServeripDfserver(ip)' "$PAGE" || fail 'existing DF signal handler missing'
grep -q 'root.appliedDfServerIp = normalized' "$PAGE" || fail 'DF signal does not establish applied endpoint'
grep -q 'root.appliedDfServerIp.length === 0' "$PAGE" || fail 'Network2 fallback overwrite guard missing'
pass 'late Network2 refresh cannot overwrite authoritative DF endpoint'

# Mutation owners remain unchanged; no ENDPOINTS-specific DB/query API is added.
grep -q 'km.connectToDFserver(ip)' "$PAGE" || fail 'existing Apply owner missing'
grep -q 'km.connectToDFserver(savedIp)' "$PAGE" || fail 'existing reconnect reuse missing'
grep -q 'km.setCompassOffset(value)' "$PAGE" || fail 'existing compass owner missing'
if grep -Rq 'requestServiceEndpointsState\|reconnectDFserver' "$PAGE" "$HDR" "$CPP"; then
    fail 'removed ENDPOINTS-specific backend API returned'
fi
pass 'existing mutation owners retained with no duplicate endpoint backend'

# Structural delimiter check.
python3 - "$PAGE" "$HDR" "$CPP" <<'PY'
from pathlib import Path
import sys

def strip(src):
    out=[]; i=0; state='code'; quote=''
    while i < len(src):
        c=src[i]; d=src[i+1] if i+1<len(src) else ''
        if state=='code':
            if c=='/' and d=='/': state='line'; out += [' ',' ']; i+=2; continue
            if c=='/' and d=='*': state='block'; out += [' ',' ']; i+=2; continue
            if c in ('"', "'"): state='str'; quote=c; out.append(' '); i+=1; continue
            out.append(c); i+=1; continue
        if state=='line':
            if c=='\n': state='code'; out.append('\n')
            else: out.append(' ')
            i+=1; continue
        if state=='block':
            if c=='*' and d=='/': state='code'; out += [' ',' ']; i+=2
            else: out.append('\n' if c=='\n' else ' '); i+=1
            continue
        if state=='str':
            if c=='\\': out += [' ',' ']; i+=2; continue
            if c==quote: state='code'
            out.append(' '); i+=1
    return ''.join(out)

pairs={'}':'{',')':'(',']':'['}
for fn in sys.argv[1:]:
    stack=[]
    for lineno,line in enumerate(strip(Path(fn).read_text()).splitlines(),1):
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

# Prior NET-ENDPOINTS1.4 regression should remain valid.
"$ROOT/VERIFY-NET-ENDPOINTS1.4.sh" >/tmp/net_endpoints14_regression.log
cat /tmp/net_endpoints14_regression.log

echo '[PASS] NET-ENDPOINTS1.5 structural verification complete'
