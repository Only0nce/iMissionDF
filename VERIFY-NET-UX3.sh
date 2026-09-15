#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
QML="$ROOT/NetworkAccessModePopup.qml"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

[ -f "$QML" ] || fail "NetworkAccessModePopup.qml missing"
grep -q 'width: 720' "$QML" || fail "popup width fix missing"
grep -q 'height: 460' "$QML" || fail "popup height fix missing"
grep -q 'Layout.minimumHeight: 248' "$QML" || fail "card minimum height missing"
[ "$(grep -c 'contentItem: ColumnLayout' "$QML")" -ge 2 ] || fail "Viewer/Admin cards are not layout-managed"
[ "$(grep -c 'Layout.fillHeight: true' "$QML")" -ge 3 ] || fail "flexible card spacing missing"
grep -q 'LAN3/LAN4 and VPN control remain read-only' "$QML" || fail "Viewer policy text changed unexpectedly"
grep -q 'connect/disconnect VPN profiles' "$QML" || fail "Admin VPN policy text changed unexpectedly"
pass "Viewer/Admin popup uses non-overlapping layout"

python3 - "$QML" <<'PY'
import sys
p=sys.argv[1]
s=open(p, encoding='utf-8').read()
pairs={'{':'}','(':')','[':']'}
stack=[]
in_s=in_d=False
esc=False
line=1
for i,ch in enumerate(s):
    if ch=='\n': line+=1
    if esc:
        esc=False; continue
    if ch=='\\' and (in_s or in_d):
        esc=True; continue
    if not in_d and ch=="'": in_s=not in_s; continue
    if not in_s and ch=='"': in_d=not in_d; continue
    if in_s or in_d: continue
    if ch in pairs: stack.append((ch,line))
    elif ch in pairs.values():
        if not stack or pairs[stack[-1][0]] != ch:
            raise SystemExit(f"unbalanced delimiter at line {line}")
        stack.pop()
if stack: raise SystemExit(f"unclosed delimiter from line {stack[-1][1]}")
print('[PASS] lightweight QML delimiter validation')
PY

pass "NET-UX3 structural verification complete"
