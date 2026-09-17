#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/rec_state1_work}"
SIDE="$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml"
MAIN="$ROOT/MainPage.qml"
SETTING="$ROOT/Setting.qml"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

[[ -f "$SIDE" && -f "$MAIN" && -f "$SETTING" ]] || fail "required QML files missing"

# Pre-entry chooser/auth path must be gone from the side drawer.
for token in 'NetworkAccessModePopup' 'networkEntryPasswordPopup' 'pendingProtectedNavigation' 'completeProtectedNetworkNavigation' 'requestSelection()'; do
  if grep -Fq "$token" "$SIDE"; then fail "old Network entry popup path remains: $token"; fi
done
pass "Network Settings entry popup/pending-navigation path removed"

# Network entry must navigate directly; MainPage must inject Viewer.
grep -Fq '[NET-AUTH3] direct Network Settings entry -> Viewer' "$SIDE" || fail "direct Viewer entry diagnostic missing"
grep -Fq '"networkAccessRole": "viewer"' "$MAIN" || fail "MainPage does not force Viewer on fresh Network entry"
grep -Fq 'networkAccessRole = "viewer"' "$SETTING" || fail "Setting.qml defensive Viewer reset missing"
pass "fresh Network Settings entry is Viewer-only"

# In-page role elevation must still exist.
grep -Fq 'function requestNetworkAccessToggle()' "$SETTING" || fail "in-page role switch missing"
grep -Fq 'NetworkPasswordPopup {' "$SETTING" || fail "in-page Admin password popup missing"
grep -Fq 'onAuthorized: networkManager.setNetworkAccessRole("admin")' "$SETTING" || fail "Admin authorization path missing"
pass "Viewer/Admin switching remains inside Network Settings"

# Selection and hover must remain separate.
grep -Fq 'toolbar.currentIndex = index' "$SIDE" || fail "toolbar selection assignment missing"
grep -Fq 'toolbar.hoveredIndex = -1' "$SIDE" || fail "toolbar hover clear missing"
python3 - "$SIDE" <<'PY2'
import sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
if 'toolbar.currentIndex = index\n        toolbar.hoveredIndex = index' in s:
    raise SystemExit('navigation still latches hover as selected state')
PY2
grep -Fq '} else if (toolbar.hoveredIndex === index) {' "$SIDE" || fail "hover exit does not clear transient state"
pass "toolbar selected/hover states are decoupled"

# Core Network page contracts remain present.
grep -Fq '{ key: "endpoints", label: "Endpoints", enabled: true }' "$SETTING" || fail "Endpoints tab missing"
grep -Fq 'readonly property bool networkAdminMode' "$SETTING" || fail "Viewer/Admin permission state missing"
grep -Fq 'Network Settings entered in Viewer mode' "$SETTING" || fail "Viewer entry runtime proof missing"
pass "Network tabs and role policy retained"

# Regression guard: unrelated recorder/audio product files must remain byte-identical.
for f in HomeDisplay.qml MyDrawer.qml RadioScanner.qml Mainwindows.h Mainwindows.cpp websocketclient.cpp NetworkController.cpp NetworkController.h ServiceEndpointsPage.qml; do
  [[ -f "$BASE/$f" && -f "$ROOT/$f" ]] || continue
  cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
done
pass "recorder/audio/network backend files unchanged"

# Legacy top drawer from NET-ENDPOINTS1.3 remains restored.
grep -Fq 'TopNetworkDrawer {' "$MAIN" || fail "legacy TopNetworkDrawer runtime instance missing"
grep -Fq 'id: topDrawer' "$MAIN" || fail "legacy topDrawer id missing"
pass "temporary legacy Top Network Drawer remains available"

# Lightweight QML delimiter validation.
python3 - "$SIDE" "$MAIN" "$SETTING" <<'PY'
import sys
from pathlib import Path
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=Path(fn).read_text(errors='replace')
    stack=[]; i=0; quote=None; line=False; block=False
    while i < len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if c=='\\': i+=2; continue
            if c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack[-1] != pairs[c]:
                raise SystemExit(f'{fn}: delimiter mismatch near offset {i}')
            stack.pop()
        i+=1
    if stack: raise SystemExit(f'{fn}: unclosed delimiters {stack[-10:]}')
print('[PASS] modified QML delimiter validation')
PY

pass "NET-AUTH3 structural verification complete"
