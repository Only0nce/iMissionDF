#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

DB="$ROOT/iScreenDF/DatabaseDF.cpp"
TCP="$ROOT/iScreenDF/functionTcpServer.cpp"
TCPC="$ROOT/iScreenDF/TcpClientDF.cpp"
TCPH="$ROOT/iScreenDF/TcpClientDF.h"
ISCH="$ROOT/iScreenDF/iScreenDF.h"
MWC="$ROOT/Mainwindows.cpp"
MWH="$ROOT/Mainwindows.h"
QML="$ROOT/Setting.qml"

grep -q 'SELECT.*ipdfserver\| ipdfserver ' "$DB" || fail "Parameter.ipdfserver DB source missing"
grep -q 'connectToServer(p->m_ipdfServer, 5555)' "$TCP" || fail "legacy RFSoC control endpoint :5555 missing"
pass "legacy RFSoC management TCP source/port preserved"

grep -q 'emit updateNetworkDfDevice("end0"' "$DB" || fail "LAN3 -> end0 DB emit missing"
grep -q 'emit updateNetworkDfDevice("end1"' "$DB" || fail "LAN4 -> end1 DB emit missing"
pass "Network2 row 3/4 remote-interface emits preserved"

for field in menuID ifname ip netmask gateway dns1 dns2; do
    grep -q "obj\[\"$field\"\]" "$TCP" || fail "setIpConfig field missing: $field"
done
grep -q 'obj\["menuID"\].*= "setIpConfig"' "$TCP" || fail "setIpConfig menuID missing"
pass "legacy setIpConfig JSON contract preserved"

grep -q 'iface != QStringLiteral("end0").*iface != QStringLiteral("end1")' "$TCP" || fail "end0/end1 guard missing"
grep -q 'rfsocIpConfigDispatchResult' "$TCP" || fail "dispatch result telemetry missing"
grep -q 'pendingWriteCount' "$TCPH" || fail "pending queue visibility missing"
grep -q 'hasTarget' "$TCPH" || fail "control target visibility missing"
pass "RFSoC dispatch hardening present"

grep -q 'm_pendingWrites.push_back(payload)' "$TCPC" || fail "legacy disconnected write queue missing"
grep -q 'flushPendingWrites' "$TCPC" || fail "legacy reconnect flush missing"
pass "legacy reconnect queue preserved"

grep -q 'remoteLanIpConfigDispatch' "$MWH" || fail "Mainwindows dispatch signal missing"
grep -q 'rfsocIpConfigDispatchResult' "$MWC" || fail "iScreenDF -> Mainwindows dispatch relay missing"
grep -q 'onRemoteLanIpConfigDispatch' "$QML" || fail "QML dispatch feedback missing"
pass "dispatch state reaches Network Settings UI"

grep -q 'updateNetworkfromDisplayIndex(index' "$MWC" || fail "Network2 integration missing from Mainwindows"
grep -q 'netWorkController->applyNetworkConfig(iface' "$MWC" || fail "JSON/system apply path missing"
pass "database + persistence/local apply integration retained"

echo "[PASS] R-LAN4B legacy RFSoC IP apply verification complete"
