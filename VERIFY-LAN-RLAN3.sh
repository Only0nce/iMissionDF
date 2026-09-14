#!/bin/bash
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
NC="$ROOT/NetworkController.cpp"
MW="$ROOT/Mainwindows.cpp"
DB="$ROOT/iScreenDF/DatabaseDF.cpp"
TCP="$ROOT/iScreenDF/functionTcpServer.cpp"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

[[ -f "$NC" ]] || fail "NetworkController.cpp missing"
[[ -f "$MW" ]] || fail "Mainwindows.cpp missing"
[[ -f "$DB" ]] || fail "DatabaseDF.cpp missing"
[[ -f "$TCP" ]] || fail "functionTcpServer.cpp missing"

grep -q '#include <QSaveFile>' "$NC" || fail "QSaveFile include missing"
grep -q 'file.setDirectWriteFallback(false)' "$NC" || fail "atomic direct-write fallback guard missing"
grep -q 'static QMutex &networkConfigMutex()' "$NC" || fail "network config mutex missing"
grep -q 'static bool updateNetworkConfigRoot' "$NC" || fail "read-modify-write helper missing"
pass "atomic JSON writer and shared lock present"

python3 - "$NC" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
apply=s[s.index('void NetworkController::applyNetworkConfig'):s.index('void NetworkController::runNmcliCommand')]
assert 'updateNetworkConfigRoot([&](QJsonObject &rootObj)' in apply
assert 'if (!jsonOk)' in apply
assert apply.index('if (!jsonOk)') < apply.index('// 2) Apply nmcli in background.')
assert 'QIODevice::WriteOnly | QIODevice::Truncate' not in s[s.index('static bool writeNetworkConfigRootUnlocked'):s.index('static bool runProcessBlocking')]
print('[PASS] LAN persistence is atomic and failure blocks local system apply')
PY

# Ensure WiFi/5G read-modify-write sites use the same protected helper.
count=$(grep -c 'updateNetworkConfigRoot(\[&\](QJsonObject &root)' "$NC" || true)
[[ "$count" -ge 2 ]] || fail "WiFi/5G protected update helpers not found"
pass "WiFi/5G section updates share protected read-modify-write path"

# Preserve Database / external TCP integration.
grep -q 'm_lanIntegrationBackend->updateNetworkfromDisplayIndex' "$MW" || fail "Mainwindows Network2 path missing"
grep -q 'UPDATE Network2' "$DB" || fail "Network2 UPDATE missing"
grep -q 'emit updateNetworkDfDevice("end0"' "$DB" || fail "LAN3 DB->end0 emission missing"
grep -q 'emit updateNetworkDfDevice("end1"' "$DB" || fail "LAN4 DB->end1 emission missing"
grep -q 'obj\["menuID"\].*=.*"setIpConfig"' "$TCP" || fail "setIpConfig TCP contract missing"
grep -q 'sendRfsocJsonLine(obj, true)' "$TCP" || fail "RFSoC TCP sender missing"
pass "Network2 database and LAN3/LAN4 TCP contract preserved"

echo "[PASS] R-LAN3 structural verification complete"
