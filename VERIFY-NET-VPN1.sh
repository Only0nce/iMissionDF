#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
pass(){ echo "[PASS] $1"; }
grep -q '{ key: "vpn", label: "VPN", enabled: true }' Setting.qml
grep -q 'qrc:/VpnPage.qml' Setting.qml
grep -q '<file>VpnPage.qml</file>' qml.qrc
pass "VPN tab/page is wired into Network Settings"
grep -q 'Q_INVOKABLE QVariantMap vpnStatus' NetworkController.h
grep -q 'Q_INVOKABLE void requestVpnStatus' NetworkController.h
grep -q 'Q_INVOKABLE void setVpnConnectionActive' NetworkController.h
grep -q 'QStringLiteral("wireguard")' NetworkController.cpp
grep -q 'QStringLiteral("vpn")' NetworkController.cpp
pass "NetworkManager VPN/WireGuard backend contract present"
grep -q 'Admin Required' VpnPage.qml
grep -q 'adminMode' VpnPage.qml
pass "Viewer/Admin VPN permission UX present"
if grep -Rni --include='VpnPage.qml' -E 'password|private.key|secret|token' . | grep -v 'No VPN secrets' >/dev/null 2>&1; then
  echo "[WARN] review possible secret wording in VpnPage.qml"
fi
pass "NET-VPN1 structural verification complete"
