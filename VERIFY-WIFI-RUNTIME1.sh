#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
QML="$ROOT/Wifi5GPage.qml"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

[[ -f "$QML" ]] || fail "Wifi5GPage.qml not found"

# The removed function name may exist in documentation comments, but never as an executable call.
if grep -nE '^[[:space:]]*clearPendingWifiAdvancedSave[[:space:]]*\(' "$QML" >/dev/null; then
    fail "stale clearPendingWifiAdvancedSave() call still executable"
fi
pass "stale WiFi advanced-save cleanup call removed"

grep -q 'Component.onDestruction' "$QML" || fail "destruction handler missing"
grep -q 'pageRuntimeSyncTimer.stop()' "$QML" || fail "page runtime sync timer cleanup missing"
grep -q 'deactivateWifiPageRuntime()' "$QML" || fail "WiFi runtime cleanup missing"
grep -q 'deactivateCellularPageRuntime()' "$QML" || fail "cellular runtime cleanup missing"
pass "WiFi/5G destruction cleanup retained"

# NET-AUTH1 contract: old per-Apply WiFi auth transaction must remain removed.
if grep -qE 'pendingWifiAdvancedSettings|requestProtectedWifiAdvancedSave|commitProtectedWifiAdvancedSave|wifiApplyPasswordPopup' "$QML"; then
    fail "removed WiFi per-Apply auth transaction reintroduced"
fi
pass "NET-AUTH1 direct WiFi advanced-save contract retained"

python3 - "$QML" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
# Lightweight delimiter check with comments/strings ignored well enough for regression use.
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
            if not stack or stack[-1] != pairs[c]:
                raise SystemExit(f"delimiter mismatch at offset {i}: {c}")
            stack.pop()
    elif state=='line':
        if c=='\n': state='code'
    elif state=='block':
        if c=='*' and n=='/': state='code'; i+=2; continue
    elif state=='string':
        if c=='\\': i+=2; continue
        if c==quote: state='code'
    i+=1
if stack:
    raise SystemExit(f"unclosed delimiters: {stack[-10:]}")
print('[PASS] lightweight QML delimiter validation')
PY

pass "WIFI-RUNTIME1 structural verification complete"
