#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

[[ -f Setting.qml ]] || fail "Setting.qml missing"
[[ -f NetworkAccessModePopup.qml ]] || fail "NetworkAccessModePopup.qml missing"
[[ -f NetworkPasswordPopup.qml ]] || fail "NetworkPasswordPopup.qml missing"

grep -q 'function requestLanApplyConfirmation()' Setting.qml || fail "confirmation request function missing"
grep -q 'id: lanApplyConfirmPopup' Setting.qml || fail "LAN apply confirmation popup missing"
grep -q 'text: "Confirm Apply / Save"' Setting.qml || fail "confirmation title missing"
grep -q 'text: "Confirm Apply"' Setting.qml || fail "confirmation action missing"
grep -q 'onClicked: networkManager.requestLanApplyConfirmation()' Setting.qml || fail "Apply/Save does not open confirmation"
grep -q 'networkManager.applyLanSetting()' Setting.qml || fail "confirmed apply path missing"
pass "two-step LAN Apply / Save confirmation present"

count=$(grep -R 'Behavior on scale' Setting.qml NetworkAccessModePopup.qml NetworkPasswordPopup.qml | wc -l)
[[ "$count" -ge 10 ]] || fail "press-animation coverage too small ($count)"
pass "button press animations present ($count animated controls)"

# Backend hashes from the NET-AUTH2.1 base. These must remain unchanged.
check_hash() {
    local expected="$1" file="$2"
    local actual
    actual=$(sha256sum "$file" | awk '{print $1}')
    [[ "$actual" == "$expected" ]] || fail "backend changed: $file ($actual)"
    pass "backend unchanged: $file"
}
check_hash 31f83b32fd1fc8ab18fa1a9e21b1962a4706a9185f567abc9f79cfecd4003f4f NetworkController.cpp
check_hash 15803b0127bcecb5937a86274e8a1f0c5b218ee0e7d5c8c6b98f5cab30fa1507 Mainwindows.cpp
check_hash 82c5f594c6d7c2b51b87a736a3c4eec8b685fd1d95105ed0f634b3357dc8bb03 iScreenDF/DatabaseDF.cpp
check_hash acea136e8c31c35ad1862c8cb72406d5893e2873cf9c994c45a838e1ebc9d8b0 iScreenDF/functionTcpServer.cpp
check_hash 4779b2d74e69bef235d8f0c4628b5d9115ff0646840c10ed72bee3dfe4bfcf21 iScreenDF/functionMonitor.cpp

python3 - <<'PY'
from pathlib import Path
for fn in ['Setting.qml','NetworkAccessModePopup.qml','NetworkPasswordPopup.qml']:
    s=Path(fn).read_text()
    stack=[]; pairs={'}':'{',']':'[',')':'('}; opens=set(pairs.values())
    state='code'; quote=None; i=0; line=1
    while i<len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if c=='\n': line+=1
        if state=='line':
            if c=='\n': state='code'
            i+=1; continue
        if state=='block':
            if c=='*' and n=='/': state='code'; i+=2; continue
            i+=1; continue
        if state=='string':
            if c=='\\': i+=2; continue
            if c==quote: state='code'; quote=None
            i+=1; continue
        if c=='/' and n=='/': state='line'; i+=2; continue
        if c=='/' and n=='*': state='block'; i+=2; continue
        if c in ('"', "'"): state='string'; quote=c; i+=1; continue
        if c in opens: stack.append((c,line))
        elif c in pairs:
            if not stack or stack[-1][0] != pairs[c]:
                raise SystemExit(f'[FAIL] {fn}: delimiter mismatch line {line}')
            stack.pop()
        i+=1
    if stack:
        raise SystemExit(f'[FAIL] {fn}: unclosed delimiter {stack[-1]}')
    print(f'[PASS] {fn}: balanced delimiters')
PY

pass "NET-UX1 structural verification complete"
