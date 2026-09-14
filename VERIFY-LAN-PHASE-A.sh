#!/usr/bin/env bash
set -euo pipefail

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

require_grep() {
    local pattern="$1" file="$2" label="$3"
    grep -qE "$pattern" "$file" || fail "$label"
    pass "$label"
}

require_grep 'Q_INVOKABLE bool applyLanSettings' Mainwindows.h \
    'Mainwindows exposes the new LAN compatibility coordinator'
require_grep 'Mainwindows mainWindows\(netCtrl, kraken, nullptr\)' main.cpp \
    'Mainwindows receives the proven iScreenDF/Network2 integration backend'
require_grep 'updateNetworkfromDisplayIndex\(index' Mainwindows.cpp \
    'New LAN UI restores Network2 and LAN3/LAN4 external-device path'
require_grep 'commandMainCppToRecCpp' Mainwindows.cpp \
    'LAN2 recorder side effect remains present'
require_grep 'modeLower == "on"' NetworkController.cpp \
    'Legacy DHCP on/off vocabulary is accepted by NetworkController'
require_grep 'mainWindows\.applyLanSettings' Setting.qml \
    'Setting.qml routes production LAN mutations through one coordinator'
require_grep 'broadcastLanSnapshotAsync' Mainwindows.cpp \
    'LAN snapshot broadcast is asynchronous after apply completion'

# Guard against accidentally reintroducing the old synchronous readback inside
# setNetworkFormDisplay(). Restrict the search to that method body.
python3 - <<'PY'
from pathlib import Path
s = Path('Mainwindows.cpp').read_text()
a = s.index('void Mainwindows::setNetworkFormDisplay(const int index,')
b = s.index('bool Mainwindows::applyLanSettings', a)
body = s[a:b]
if 'loadAllLanConfig()' in body:
    raise SystemExit('[FAIL] setNetworkFormDisplay still performs synchronous loadAllLanConfig()')
print('[PASS] setNetworkFormDisplay has no synchronous loadAllLanConfig()')
PY

echo '[PASS] LAN Phase A structural verification complete'
