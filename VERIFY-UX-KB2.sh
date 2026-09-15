#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
QML="$ROOT/Wifi5GView.qml"

pass() { printf '[PASS] %s\n' "$1"; }
fail() { printf '[FAIL] %s\n' "$1" >&2; exit 1; }

[ -f "$QML" ] || fail "Wifi5GView.qml not found"

grep -q 'id: wifiAdvancedOverlay' "$QML" || fail "WiFi IPv4 panel missing"
grep -q 'y: 24' "$QML" || fail "WiFi IPv4 panel is not pinned near the top"
grep -q 'height: 282' "$QML" || fail "compact WiFi IPv4 panel height missing"
grep -q 'columns: 3' "$QML" || fail "three-column IPv4 grid missing"
grep -q 'id: wifiIpv4ActionRow' "$QML" || fail "top action row missing"
grep -q 'visible: Qt.inputMethod.visible' "$QML" || fail "keyboard Done control missing"
grep -q 'Qt.inputMethod.hide()' "$QML" || fail "keyboard hide action missing"
pass "WiFi IPv4 dialog uses persistent keyboard-safe geometry"

for token in 'wifiAdvancedSaveRequested' 'wifiConnectRequested' 'wifiDisconnectRequested' 'wifiForgetRequested'; do
    grep -q "$token" "$QML" || fail "existing WiFi contract missing: $token"
done
pass "existing WiFi signal contracts retained"

python3 - "$QML" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
for a,b in [('(',')'),('{','}'),('[',']')]:
    if s.count(a) != s.count(b):
        raise SystemExit(f'unbalanced {a}{b}: {s.count(a)} != {s.count(b)}')
print('[PASS] QML delimiter counts balanced')
PY

pass "UX-KB2 structural verification complete"
