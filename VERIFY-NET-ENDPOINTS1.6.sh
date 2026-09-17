#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CPP="$ROOT/iScreenDF/functionTcpServer.cpp"
DBCPP="$ROOT/iScreenDF/DatabaseDF.cpp"
HDR="$ROOT/iScreenDF/iScreenDF.h"
QML="$ROOT/ServiceEndpointsPage.qml"

fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

python3 - "$CPP" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
start=s.find('void iScreenDF::GetrfsocParameter(')
end=s.find('\nvoid iScreenDF::setIPLocalForRemoteGroup', start)
if start < 0 or end < 0:
    raise SystemExit('[FAIL] GetrfsocParameter body not found')
body=s[start:end]
for bad in ('qDeleteAll(m_parameter)', 'm_parameter.clear()'):
    if bad in body:
        raise SystemExit(f'[FAIL] destructive partial refresh remains: {bad}')
if 'p = m_parameter.first();' not in body:
    raise SystemExit('[FAIL] existing Parameter object is not reused')
if 'm_parameter.append(p);' not in body:
    raise SystemExit('[FAIL] first-time Parameter creation path missing')
if 'p->m_ipdfServer =' in body or 'm_parameter.first()->m_ipdfServer =' in body:
    raise SystemExit('[FAIL] GetrfsocParameter still owns/mutates DF Server IP')
for required in (
    'p->m_setDoaEnable          = setDoaEnable;',
    'p->m_spectrumEnabled       = spectrumEnabled;',
    'p->m_setAdcChannel         = setAdcChannel;',
    'p->m_Frequency             = Frequency;',
    'p->m_compass_offset        = compassoffset;',
    'p->m_ipLocalForRemoteGroup = ipLocalForRemoteGroup;',
    '[PARAM][RFSOC-REFRESH]',
    'ipdfserver_before=',
    'ipdfserver_after='
):
    if required not in body:
        raise SystemExit(f'[FAIL] expected partial-refresh contract missing: {required}')
print('[PASS] GetrfsocParameter updates in place and preserves non-owned fields')
PY

python3 - "$DBCPP" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
a=s.find('void DatabaseDF::GetrfsocParameter()')
b=s.find('void DatabaseDF::GetIPDFServerFromDB()', a)
if a < 0 or b < 0:
    raise SystemExit('[FAIL] DB parameter functions not found')
rfsoc=s[a:b]
if 'ipdfserver' in rfsoc:
    raise SystemExit('[FAIL] GetrfsocParameter unexpectedly expanded to own ipdfserver')
ip=s[b:s.find('void DatabaseDF::UpdateParameterField', b)]
for required in ('SELECT ', ' ipdfserver ', 'FROM Parameter ', 'WHERE id = 1', 'emit GetIPDFServer(ip);'):
    if required not in ip:
        raise SystemExit(f'[FAIL] existing ipdfserver DB startup path missing: {required}')
print('[PASS] existing dedicated Parameter.ipdfserver DB owner retained')
PY

grep -q 'm_parameter.first()->m_ipdfServer = host' "$CPP" || fail 'DB-loaded IP is not stored in runtime Parameter'
grep -q 'p->m_ipdfServer = ip' "$CPP" || fail 'existing Apply runtime assignment missing'
grep -q 'queueUpdateParameterField(db, "ipdfserver", p->m_ipdfServer)' "$CPP" || fail 'existing Apply persistence path missing'
grep -q 'void updateIPServerDF();' "$HDR" || fail 'existing Endpoints replay slot missing'
grep -q 'km.updateIPServerDF()' "$QML" || fail 'Endpoints does not replay the existing DB-loaded runtime IP'
pass 'DF Server IP DB-load / Apply / replay owners retained'

python3 - "$CPP" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
# Lightweight delimiter validation with strings/comments skipped enough for this source.
stack=[]
pairs={')':'(',']':'[','}':'{'}
opens=set(pairs.values())
i=0
state='code'
quote=''
while i < len(s):
    c=s[i]
    n=s[i+1] if i+1 < len(s) else ''
    if state=='code':
        if c=='/' and n=='/': state='line'; i+=2; continue
        if c=='/' and n=='*': state='block'; i+=2; continue
        if c in ('"', "'"): state='string'; quote=c; i+=1; continue
        if c in opens: stack.append(c)
        elif c in pairs:
            if not stack or stack.pop()!=pairs[c]:
                raise SystemExit('[FAIL] C++ delimiter mismatch')
    elif state=='line':
        if c=='\n': state='code'
    elif state=='block':
        if c=='*' and n=='/': state='code'; i+=2; continue
    else:
        if c=='\\': i+=2; continue
        if c==quote: state='code'
    i+=1
if stack:
    raise SystemExit('[FAIL] C++ delimiters left open')
print('[PASS] modified C++ delimiter validation')
PY

# Re-run the directly inherited contracts that this fix must not regress.
bash "$ROOT/VERIFY-NET-ENDPOINTS1.5.sh" >/tmp/net_endpoints16_ep15.verify
cat /tmp/net_endpoints16_ep15.verify
bash "$ROOT/VERIFY-R20.4-STAB2.sh" >/tmp/net_endpoints16_stab2.verify
cat /tmp/net_endpoints16_stab2.verify
pass 'NET-ENDPOINTS1.6 structural verification complete'
