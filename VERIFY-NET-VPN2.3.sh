#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
SETTING="$ROOT/Setting.qml"
VPN="$ROOT/VpnPage.qml"

grep -q 'property bool vpnInitialRefreshDone: false' "$SETTING"
grep -q 'if (!networkManager.vpnInitialRefreshDone)' "$SETTING"
grep -q 'networkManager.vpnInitialRefreshDone = true' "$SETTING"
grep -q 'item.refreshAll()' "$SETTING"

if grep -q 'Component.onCompleted.*refreshAll' "$VPN"; then
  echo '[FAIL] VpnPage has Component.onCompleted auto refresh'
  exit 1
fi
if grep -q 'interval: 3000' "$VPN"; then
  echo '[FAIL] VpnPage still has 3-second polling timer'
  exit 1
fi

echo '[PASS] VPN first-entry auto refresh guard is owned by Setting.qml'
echo '[PASS] VpnPage remains free of background polling/startup auto-refresh'
echo '[PASS] NET-VPN2.3 structural verification complete'
