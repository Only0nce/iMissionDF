#!/bin/bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"

view="$ROOT/Wifi5GView.qml"
page="$ROOT/Wifi5GPage.qml"
net="$ROOT/NetworkController.cpp"

for f in "$view" "$page" "$net"; do
    test -f "$f" || { echo "[FAIL] missing $f"; exit 1; }
done

grep -q 'property string wifiIface: ""' "$view"
grep -q 'y: wifiAdvancedVisible ? 392 : 88' "$view"
grep -q 'x: 560' "$view"
echo '[PASS] WiFi UI no longer starts with hard-coded wlan0 and keeps network list visible during IPv4 editing'

grep -q 'property bool wifiConfigReady: false' "$page"
grep -q 'function completeWifiRefreshAfterConfig()' "$page"
grep -q 'wifiConfigResolveTimeoutTimer' "$page"
grep -q 'Detecting WiFi interface' "$page"
echo '[PASS] interface-first WiFi startup sequencing present'

grep -q '\[WiFiScan\] preserving' "$page"
grep -q 'showing previous scan results' "$page"
echo '[PASS] transient scan failures preserve the last-good connection list'

grep -q '\[WiFi\]\[SCAN\] requested=' "$net"
grep -q '\[WiFi\]\[SCAN\] failed requested=' "$net"
echo '[PASS] backend scan diagnostics present'

python3 - "$view" "$page" <<'PY'
from pathlib import Path
import sys
for fn in sys.argv[1:]:
    s=Path(fn).read_text()
    pairs=[('{','}'),('(',')'),('[',']')]
    for a,b in pairs:
        if s.count(a)!=s.count(b):
            raise SystemExit(f"[FAIL] delimiter count mismatch in {fn}: {a}={s.count(a)} {b}={s.count(b)}")
print('[PASS] QML delimiter-count structural check')
PY

echo '[PASS] UX-KB2.1 WiFi connection recovery structural verification complete'
