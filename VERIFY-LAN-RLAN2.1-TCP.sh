#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

need() { grep -q "$1" "$2" || { echo "[FAIL] $3"; exit 1; }; echo "[PASS] $3"; }
need 'externalLan = (index == 2 || index == 3)' "$ROOT/Mainwindows.cpp" 'LAN3/LAN4 external-port guard exists'
need 'external TCP backend is unavailable' "$ROOT/Mainwindows.cpp" 'LAN3/LAN4 reject when TCP backend is missing'
need 'emit updateNetworkDfDevice("end0"' "$ROOT/iScreenDF/DatabaseDF.cpp" 'LAN3 maps to end0 TCP path'
need 'emit updateNetworkDfDevice("end1"' "$ROOT/iScreenDF/DatabaseDF.cpp" 'LAN4 maps to end1 TCP path'
need 'obj\["menuID"\].*= "setIpConfig"' "$ROOT/iScreenDF/functionTcpServer.cpp" 'TCP JSON uses setIpConfig'
for k in ifname ip netmask gateway dns1 dns2; do
  need "obj\[\"$k\"\]" "$ROOT/iScreenDF/functionTcpServer.cpp" "TCP JSON contains $k"
done
need 'sendRfsocJsonLine(obj, true)' "$ROOT/iScreenDF/functionTcpServer.cpp" 'TCP JSON uses existing sendRfsocJsonLine path'
echo '[PASS] R-LAN2.1 external TCP contract verification complete'
