#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/vpn22_orig}"
QML="$ROOT/VpnPage.qml"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

[[ -f "$QML" ]] || fail "VpnPage.qml missing"

grep -q 'onClicked: refreshAll()' "$QML" || fail "Refresh button is not wired to refreshAll()"
grep -q 'NetworkController.requestVpnStatus()' "$QML" || fail "manual VPN status request missing"
grep -q 'NetworkController.requestVpnPublicIp()' "$QML" || fail "manual Public IP request missing"
! grep -q 'id: reconcileTimer' "$QML" || fail "background reconcile Timer still present"
! grep -q 'interval: 3000' "$QML" || fail "3-second VPN polling still present"
! grep -q 'Component.onCompleted' "$QML" || fail "VPN page still auto-refreshes on construction"
! grep -q 'refreshPublicAfterReadback' "$QML" || fail "automatic Public-IP post-operation refresh still present"
grep -q 'Press Refresh to load VPN status' "$QML" || fail "manual-refresh initial guidance missing"
pass "VPN page refresh is manual-only"

# Transaction verification must remain in backend after operator lifecycle actions.
grep -q 'self->requestVpnStatus();' "$ROOT/NetworkController.cpp" || fail "authoritative operation readback missing"
pass "one-shot post-operation VPN status verification retained"

# Only VpnPage.qml should differ in production code from the base.
for f in NetworkController.cpp NetworkController.h Setting.qml NetworkAccessModePopup.qml main.cpp; do
    if [[ -f "$BASE/$f" && -f "$ROOT/$f" ]]; then
        cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
    fi
done
pass "VPN/backend/LAN/shutdown integration files unchanged from NET-STAB1 base"

python3 - "$QML" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1])
s=p.read_text()
# Lightweight delimiter scan with strings/comments stripped enough for QML structure.
stack=[]
pairs={'}':'{', ')':'(', ']':'['}
opens=set(pairs.values())
i=0
quote=None
line_comment=False
block_comment=False
while i < len(s):
    c=s[i]
    n=s[i+1] if i+1 < len(s) else ''
    if line_comment:
        if c=='\n': line_comment=False
        i+=1; continue
    if block_comment:
        if c=='*' and n=='/': block_comment=False; i+=2; continue
        i+=1; continue
    if quote:
        if c=='\\': i+=2; continue
        if c==quote: quote=None
        i+=1; continue
    if c=='/' and n=='/': line_comment=True; i+=2; continue
    if c=='/' and n=='*': block_comment=True; i+=2; continue
    if c in ('"', "'"): quote=c; i+=1; continue
    if c in opens: stack.append(c)
    elif c in pairs:
        if not stack or stack[-1] != pairs[c]:
            raise SystemExit(f"delimiter mismatch near offset {i}: {c}")
        stack.pop()
    i+=1
if stack:
    raise SystemExit(f"unclosed delimiters: {stack[-10:]}")
PY
pass "lightweight QML delimiter validation"
pass "NET-VPN2.2 structural verification complete"
