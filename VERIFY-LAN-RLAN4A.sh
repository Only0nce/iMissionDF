#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

NC="$ROOT/NetworkController.cpp"
QML="$ROOT/Setting.qml"
MWC="$ROOT/Mainwindows.cpp"
MWH="$ROOT/Mainwindows.h"
ISH="$ROOT/iScreenDF/iScreenDF.h"
ISC="$ROOT/iScreenDF/iScreenDF.cpp"
TCH="$ROOT/iScreenDF/TcpClientDF.h"

for f in "$NC" "$QML" "$MWC" "$MWH" "$ISH" "$ISC" "$TCH"; do
  [[ -f "$f" ]] || fail "missing $f"
done

grep -q 'static bool isExternalRfsocLan' "$NC" || fail "missing external RFSoC classifier"
grep -q 'external-rfsoc' "$NC" || fail "missing external port type"
grep -q 'controlTransport' "$NC" || fail "missing control transport metadata"
grep -q 'QStringLiteral("tcp")' "$NC" || fail "missing TCP transport value"
grep -q 'if (externalRfsoc)' "$NC" || fail "external path does not short-circuit local live probing"
pass "NetworkController has an explicit remote RFSoC LAN model"

grep -q 'if (isExternalRfsocLan(iface))' "$NC" || fail "external DHCP query guard missing"
pass "end0/end1 do not use local DHCP/nmcli status probing"

grep -q 'bool isConnected() const' "$TCH" || fail "TcpClientDF connection query missing"
grep -q 'rfsocControlConnectionChanged' "$ISH" || fail "iScreenDF connection signal missing"
grep -q 'emit rfsocControlConnectionChanged(true)' "$ISC" || fail "connected relay missing"
grep -q 'emit rfsocControlConnectionChanged(false)' "$ISC" || fail "disconnected relay missing"
grep -q 'Q_INVOKABLE QVariantMap externalLanStatus' "$MWH" || fail "Mainwindows status API missing"
grep -q 'externalLanControlStatusChanged' "$MWC" || fail "Mainwindows live relay missing"
pass "RFSoC TCP control state reaches Mainwindows/QML"

grep -q 'RFSoC TCP' "$QML" || fail "QML RFSoC TCP presentation missing"
grep -q 'External RFSoC LAN' "$QML" || fail "QML external port type missing"
grep -q 'RFSoC Status' "$QML" || fail "external status refresh action missing"
grep -q 'onExternalLanControlStatusChanged' "$QML" || fail "QML live connection update missing"
pass "Setting.qml presents LAN3/LAN4 as external TCP-controlled ports"

# Preserve proven write path contracts.
grep -q 'updateNetworkfromDisplayIndex(index' "$MWC" || fail "Network2 mutation path missing"
grep -q 'index == 2 || index == 3' "$MWC" || fail "LAN3/LAN4 external guard missing"
grep -q 'setIpConfig' "$MWC" || fail "setIpConfig contract comment/path evidence missing"
grep -q 'obj\["menuID"\].*= "setIpConfig"' "$ROOT/iScreenDF/functionTcpServer.cpp" || fail "setIpConfig TCP packet missing"
pass "Network2 + existing setIpConfig write path preserved"

echo "[PASS] R-LAN4A structural verification complete"
