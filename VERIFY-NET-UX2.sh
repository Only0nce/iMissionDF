#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
QML="$ROOT/Setting.qml"
BASE="/mnt/data/net_auth22_work"

pass() { echo "[PASS] $*"; }
fail() { echo "[FAIL] $*" >&2; exit 1; }

[ -f "$QML" ] || fail "Setting.qml missing"
grep -q 'id: networkTabRow' "$QML" || fail "networkTabRow id missing"
grep -A5 'id: networkTabRow' "$QML" | grep -q 'spacing: 16' || fail "tab spacing is not 16 px"
grep -A10 'id: mainPanel' "$QML" | grep -q 'y: 248' || fail "mainPanel top is not y=248"
grep -A12 'id: mainPanel' "$QML" | grep -q 'height: parent.height - y - 60' || fail "mainPanel lower-edge-preserving height formula changed"
pass "24 px navigation-to-panel gutter and 16 px tab spacing present"

# Preserve the latest role/DHCP/apply UX contracts.
grep -q 'return networkAdminMode || index === 0 || index === 1' "$QML" || fail "Viewer LAN1/LAN2 permission rule missing"
grep -q 'lanApplyConfirmPopup.open()' "$QML" || fail "Apply confirmation missing"
grep -q 'property bool lanModeDirty' "$QML" || fail "DHCP draft guard missing"
grep -q 'requestNetworkAccessToggle' "$QML" || fail "in-page Viewer/Admin role switch missing"
pass "existing permission, DHCP and Apply-confirm UX retained"

# This revision must be presentation-only.
for f in NetworkController.cpp Mainwindows.cpp iScreenDF/DatabaseDF.cpp iScreenDF/functionTcpServer.cpp iScreenDF/functionMonitor.cpp; do
    [ -f "$ROOT/$f" ] || fail "missing $f"
    [ -f "$BASE/$f" ] || fail "missing base $f"
    cmp -s "$ROOT/$f" "$BASE/$f" || fail "$f changed unexpectedly"
done
pass "network/database/RFSoC backend files unchanged"

python3 - "$QML" <<'PY'
import sys
fn=sys.argv[1]
s=open(fn, encoding='utf-8').read()
pairs={'{':'}','(':')','[':']'}
stack=[]; quote=None; esc=False; line_comment=False; block_comment=False
i=0
while i < len(s):
    c=s[i]; n=s[i+1] if i+1 < len(s) else ''
    if line_comment:
        if c=='\n': line_comment=False
        i+=1; continue
    if block_comment:
        if c=='*' and n=='/': block_comment=False; i+=2; continue
        i+=1; continue
    if quote:
        if esc: esc=False
        elif c=='\\': esc=True
        elif c==quote: quote=None
        i+=1; continue
    if c=='/' and n=='/': line_comment=True; i+=2; continue
    if c=='/' and n=='*': block_comment=True; i+=2; continue
    if c in ('"', "'"): quote=c; i+=1; continue
    if c in pairs: stack.append((c,i))
    elif c in pairs.values():
        if not stack or pairs[stack[-1][0]] != c:
            raise SystemExit(f"delimiter mismatch in {fn} at {i}")
        stack.pop()
    i+=1
if stack or quote or block_comment:
    raise SystemExit(f"unbalanced structure in {fn}")
print('[PASS] lightweight QML delimiter validation')
PY

pass "NET-UX2 structural verification complete"
