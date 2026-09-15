#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
QML="$ROOT/Setting.qml"

grep -q 'color: useDhcp ? "#303740" : ui.field' "$QML"
grep -q 'border.color: useDhcp ? "#46505c"' "$QML"
grep -q 'color: useDhcp ? ui.disabled : ui.text' "$QML"
# IP/mask/gateway plus primary and secondary DNS should all be disabled in DHCP mode.
COUNT="$(grep -c 'enabled: !useDhcp' "$QML")"
if [ "$COUNT" -lt 3 ]; then
    echo "[FAIL] expected DHCP-disabled text fields" >&2
    exit 1
fi
# DHCP selection must retain draft guard and must not trigger immediate refresh.
grep -q 'lanModeDirty = true' "$QML"
if sed -n '/text: "Using DHCP"/,/text: "Static Manual"/p' "$QML" | grep -q 'refreshDhcpInfo()'; then
    echo "[FAIL] DHCP button still refreshes immediately" >&2
    exit 1
fi

echo "[PASS] DHCP fields use explicit disabled gray visual state"
echo "[PASS] DHCP mode draft guard remains intact"
echo "[PASS] DHCP-UX1.1 structural verification complete"
