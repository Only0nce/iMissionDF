#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

grep -q '<file>NetworkAccessModePopup.qml</file>' "$ROOT/qml.qrc" || fail "role popup missing from qrc"
grep -q 'signal viewerSelected()' "$ROOT/NetworkAccessModePopup.qml" || fail "viewer signal missing"
grep -q 'signal adminSelected()' "$ROOT/NetworkAccessModePopup.qml" || fail "admin signal missing"
grep -q 'networkAccessModePopup.requestSelection()' "$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml" || fail "network entry does not open role selector"
grep -q 'onViewerSelected: settingsPanel.completeProtectedNetworkNavigation("viewer")' "$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml" || fail "viewer navigation missing"
grep -q 'onAdminSelected: networkEntryPasswordPopup.requestUnlock()' "$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml" || fail "admin password gate missing"
grep -q '"networkAccessRole": settingsDrawer.networkAccessRole' "$ROOT/MainPage.qml" || fail "role not passed to Setting.qml"
grep -q 'property string networkAccessRole: "viewer"' "$ROOT/Setting.qml" || fail "least-privilege default missing"
grep -q 'return networkAdminMode || index === 0' "$ROOT/Setting.qml" || fail "Viewer LAN permission model missing"
grep -q 'Viewer access: .* is read-only' "$ROOT/Setting.qml" || fail "apply permission guard missing"
grep -q 'text: "VIEW ONLY"' "$ROOT/Setting.qml" || fail "LAN list view-only indicator missing"
grep -q 'text: "READ ONLY"' "$ROOT/Setting.qml" || fail "form read-only indicator missing"
pass "Viewer/Admin entry flow and LAN permission model present"

# Important backend implementation files must remain byte-identical to NET-AUTH1 base if supplied.
if [ -n "${BASE_DIR:-}" ] && [ -d "$BASE_DIR" ]; then
  for f in NetworkController.cpp NetworkController.h Mainwindows.cpp Mainwindows.h iScreenDF/DatabaseDF.cpp iScreenDF/functionTcpServer.cpp iScreenDF/functionMonitor.cpp; do
    [ -f "$BASE_DIR/$f" ] || continue
    cmp -s "$ROOT/$f" "$BASE_DIR/$f" || fail "backend file changed unexpectedly: $f"
  done
  pass "network/database/RFSoC backend files unchanged from base"
fi

python3 - "$ROOT" <<'PY'
import pathlib, sys
root=pathlib.Path(sys.argv[1])
for name in ['Setting.qml','MainPage.qml','NetworkAccessModePopup.qml','iScreenDFqml/pages/SideSettingsDrawer.qml']:
    s=(root/name).read_text()
    # Lightweight balance check ignoring quoted strings and // comments.
    stack=[]; quote=None; esc=False; i=0
    while i < len(s):
        c=s[i]
        if quote:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c==quote: quote=None
            i+=1; continue
        if c in ('"', "'"):
            quote=c; i+=1; continue
        if c=='/' and i+1<len(s) and s[i+1]=='/':
            j=s.find('\n',i+2); i=len(s) if j<0 else j+1; continue
        if c in '{[(':
            stack.append(c)
        elif c in '}])':
            pairs={'}':'{',']':'[',')':'('}
            if not stack or stack.pop()!=pairs[c]:
                raise SystemExit(f'unbalanced delimiter in {name}')
        i+=1
    if stack or quote:
        raise SystemExit(f'unbalanced delimiter/string in {name}')
print('[PASS] lightweight QML delimiter validation')
PY

pass "NET-AUTH2 structural verification complete"
