#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
pass(){ printf '[PASS] %s\n' "$*"; }
fail(){ printf '[FAIL] %s\n' "$*" >&2; exit 1; }
need(){ local pat="$1" file="$2" msg="$3"; grep -Eq "$pat" "$ROOT/$file" || fail "$msg"; }
reject(){ local pat="$1" file="$2" msg="$3"; if grep -Eq "$pat" "$ROOT/$file"; then fail "$msg"; fi; }

TCPH=iScreenDF/TcpClientDF.h
TCPC=iScreenDF/TcpClientDF.cpp
DBH=iScreenDF/DatabaseDF.h
DBC=iScreenDF/DatabaseDF.cpp
ISH=iScreenDF/iScreenDF.h
ISC=iScreenDF/iScreenDF.cpp
FTC=iScreenDF/functionTcpServer.cpp
QML=iScreenDFqml/pages/TopNetworkDrawer.qml

need 'm_reconnectTimer\.setSingleShot\(true\)' "$TCPC" 'single-shot reconnect watchdog missing'
need 'm_immediateRetryConsumed' "$TCPH" 'one-immediate-retry guard missing'
need 'reason=.*initial-connect-watchdog|initial-connect-watchdog' "$TCPC" 'initial connect watchdog missing'
need 'queued-write' "$TCPC" 'queued write reconnect nudge missing'
pass 'bounded TcpClientDF reconnect guards present'

need 'persistDfServerEndpoint' "$DBH" 'verified endpoint DB API missing'
need 'UPDATE Parameter SET ipdfserver' "$DBC" 'endpoint DB update missing'
need 'SELECT ipdfserver FROM Parameter WHERE id = 1' "$DBC" 'endpoint DB read-back missing'
need 'result=COMMIT_OK' "$DBC" 'endpoint DB commit telemetry missing'
pass 'endpoint persistence uses update + read-back verification'

need 'void iScreenDF::connectToDFserver' "$FTC" 'Apply entry missing'
need 'candidate not committed' "$FTC" 'candidate state missing'
need 'TCP_OK_DB_COMMIT' "$FTC" 'TCP-before-DB commit boundary missing'
need 'void iScreenDF::rollbackDfEndpoint' "$FTC" 'rollback path missing'
need 'last-known-good connection restored' "$FTC" 'last-known-good restore missing'
need 'void iScreenDF::reconnectToDFserver\(\)' "$FTC" 'committed reconnect API missing'
pass 'Apply candidate / commit / rollback state machine present'

# connectToDFserver must not use the old generic eager DB writer.
python3 - "$ROOT/$FTC" <<'PY'
import re,sys
s=open(sys.argv[1],encoding='utf-8').read()
m=re.search(r'void iScreenDF::connectToDFserver\(const QString &ip\)\s*\{(.*?)\n\}',s,re.S)
if not m:
    raise SystemExit(2)
body=m.group(1)
if 'queueUpdateParameterField(db, "ipdfserver"' in body:
    raise SystemExit('eager ipdfserver DB write still exists in connectToDFserver')
PY
pass 'draft candidate is not eagerly persisted'

need 'reconnectToDFserver\(\)' "$QML" 'QML Reconnect does not use committed reconnect API'
reject 'connectToserverKraken\(serverField\.text\)' "$QML" 'legacy draft-based reconnect still used in endpoint page'
need 'onDfServerEndpointTransactionChanged' "$QML" 'QML transaction feedback missing'
pass 'QML Apply/Reconnect semantics use backend transaction owner'

reject 'qDeleteAll\(m_parameter\)' "$FTC" 'partial RFSoC refresh still destroys Parameter'
need 'ipdfserver_after' "$FTC" 'Parameter endpoint-preservation telemetry missing'
pass 'partial RFSoC refresh preserves endpoint state'

need 'obj\["menuID"\][[:space:]]*=[[:space:]]*"setIpConfig"' "$FTC" 'legacy setIpConfig menuID changed/missing'
need 'iface != QStringLiteral\("end0"\).*iface != QStringLiteral\("end1"\)' "$FTC" 'end0/end1 guard missing'
need 'id == 3' "$DBC" 'LAN3 Network2 mapping missing'
need 'updateNetworkDfDevice\("end0"' "$DBC" 'LAN3 end0 dispatch missing'
need 'id == 4' "$DBC" 'LAN4 Network2 mapping missing'
need 'updateNetworkDfDevice\("end1"' "$DBC" 'LAN4 end1 dispatch missing'
pass 'LAN3/end0 and LAN4/end1 legacy contracts retained'

python3 - "$ROOT" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
files=[
 root/'iScreenDF/TcpClientDF.h', root/'iScreenDF/TcpClientDF.cpp',
 root/'iScreenDF/DatabaseDF.h', root/'iScreenDF/DatabaseDF.cpp',
 root/'iScreenDF/iScreenDF.h', root/'iScreenDF/iScreenDF.cpp',
 root/'iScreenDF/functionTcpServer.cpp', root/'iScreenDFqml/pages/TopNetworkDrawer.qml']
# Lightweight delimiter scan after removing strings/comments. This is not a compiler,
# but catches accidental truncation in generated patches.
import re
for f in files:
    s=f.read_text(encoding='utf-8',errors='replace')
    s=re.sub(r'/\*.*?\*/','',s,flags=re.S)
    s=re.sub(r'//.*','',s)
    s=re.sub(r'"(?:\\.|[^"\\])*"','""',s)
    s=re.sub(r"'(?:\\.|[^'\\])*'","''",s)
    pairs={'{':'}','(':')','[':']'}
    inv={v:k for k,v in pairs.items()}
    st=[]
    for i,ch in enumerate(s):
        if ch in pairs: st.append((ch,i))
        elif ch in inv:
            if not st or st[-1][0]!=inv[ch]:
                raise SystemExit(f'{f}: delimiter mismatch at {i}: {ch}')
            st.pop()
    if st:
        raise SystemExit(f'{f}: unclosed delimiter {st[-1]}')
print('[PASS] touched C++/QML delimiter scan')
PY

pass 'NET-ENDPOINTS1.9 structural verification complete'
