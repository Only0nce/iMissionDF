#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/net_endpoints11_orig}"

fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

SETTING="$ROOT/Setting.qml"
VPN="$ROOT/VpnPage.qml"
ENDPOINTS="$ROOT/ServiceEndpointsPage.qml"

for f in "$SETTING" "$VPN" "$ENDPOINTS"; do
    [[ -f "$f" ]] || fail "missing $(basename "$f")"
done

! grep -qE 'Behavior on scale[[:space:]]*\{[^\n]*\}[[:space:]]*;' "$SETTING" || fail "invalid Behavior semicolon remains in Setting.qml"
! grep -qE 'Behavior on scale[[:space:]]*\{[^\n]*\}[[:space:]]*;' "$VPN" || fail "invalid Behavior semicolon remains in VpnPage.qml"
! grep -qE '\};[[:space:]]*(Button|Text|Item|Rectangle|Column|Row|on[A-Z])' "$VPN" || fail "suspicious child-object separator remains in VpnPage.qml"
pass "QML child-object semicolon parser hazards removed"

grep -q '{ key: "endpoints", label: "Endpoints", enabled: true }' "$SETTING" || fail "Endpoints tab wiring missing"
grep -q 'qrc:/ServiceEndpointsPage.qml' "$SETTING" || fail "ServiceEndpointsPage loader missing"
grep -q 'item.refreshAll()' "$SETTING" || fail "VPN first-entry refresh contract missing"
pass "Endpoints/VPN integration retained"

python3 - "$SETTING" "$VPN" "$ENDPOINTS" <<'PY'
import sys
from pathlib import Path
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=Path(fn).read_text()
    stack=[]; i=0; line=1; quote=None; linecom=False; block=False
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1 < len(s) else ''
        if c=='\n': line += 1
        if linecom:
            if c=='\n': linecom=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if c=='\\': i+=2; continue
            if c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': linecom=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '([{': stack.append((c,line))
        elif c in ')]}':
            if not stack or stack[-1][0] != pairs[c]:
                raise SystemExit(f'{Path(fn).name}: delimiter mismatch {c} line {line}')
            stack.pop()
        i+=1
    if stack:
        raise SystemExit(f'{Path(fn).name}: unclosed delimiter {stack[-1]}')
print('[PASS] Setting/VPN/Endpoints QML delimiter validation')
PY

# Parser fix must not mutate backend implementation.
for f in NetworkController.cpp NetworkController.h iScreenDF/iScreenDF.h iScreenDF/functionTcpServer.cpp qml.qrc; do
    if [[ -f "$BASE/$f" && -f "$ROOT/$f" ]]; then
        cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
    fi
done
pass "backend and qrc remain unchanged from NET-ENDPOINTS1"
pass "NET-ENDPOINTS1.1 structural verification complete"
