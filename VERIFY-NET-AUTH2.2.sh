#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
QML="$ROOT/Setting.qml"
POP="$ROOT/NetworkAccessModePopup.qml"
DRAWER="$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml"
BASE="/mnt/data/net_auth22_orig"

pass() { echo "[PASS] $*"; }
fail() { echo "[FAIL] $*" >&2; exit 1; }

grep -q 'return networkAdminMode || index === 0 || index === 1' "$QML" || fail "Viewer LAN1/LAN2 permission rule missing"
grep -q 'LAN1 + LAN2 + WiFi + 5G editable' "$QML" || fail "Viewer access summary not updated"
grep -q 'LAN3-LAN4 are read-only' "$QML" || fail "Viewer protected LAN message missing"
grep -q 'Can modify LAN1, LAN2, WiFi and 5G' "$POP" || fail "role chooser Viewer description not updated"
grep -q 'Viewer may edit LAN1 + LAN2 + WiFi + 5G' "$DRAWER" || fail "drawer permission contract comment not updated"
grep -q 'if (!canEditLanByIndex(index))' "$QML" || fail "backend-facing LAN permission guard missing"
grep -q 'lanApplyConfirmPopup.open()' "$QML" || fail "NET-UX1 Apply confirmation missing"
grep -q 'property bool lanModeDirty' "$QML" || fail "DHCP draft guard missing"
pass "Viewer may edit LAN1/LAN2 while LAN3/LAN4 remain protected"

for f in NetworkController.cpp Mainwindows.cpp iScreenDF/DatabaseDF.cpp iScreenDF/functionTcpServer.cpp iScreenDF/functionMonitor.cpp; do
    [ -f "$ROOT/$f" ] || fail "missing $f"
    [ -f "$BASE/$f" ] || fail "missing base $f"
    cmp -s "$ROOT/$f" "$BASE/$f" || fail "$f changed unexpectedly"
done
pass "network/database/RFSoC backend files unchanged from NET-UX1 base"

python3 - "$QML" "$POP" "$DRAWER" <<'PY'
import sys
for fn in sys.argv[1:]:
    s=open(fn, encoding='utf-8').read()
    # lightweight delimiter balance after stripping simple quoted strings/comments is
    # intentionally conservative; this is structural verification, not a QML compile.
    pairs={'{':'}','(':')','[':']'}
    stack=[]
    quote=None; esc=False; line_comment=False; block_comment=False
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

pass "NET-AUTH2.2 structural verification complete"
