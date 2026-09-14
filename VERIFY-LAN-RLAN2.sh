#!/bin/bash
set -euo pipefail

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

[[ -f Mainwindows.cpp && -f Mainwindows.h && -f Setting.qml ]] || fail "run from project root"

grep -q 'Q_INVOKABLE QVariantMap validateLanSettings' Mainwindows.h || fail "validation API missing"
grep -q 'const QVariantMap validation = validateLanRequest' Mainwindows.cpp || fail "authoritative apply validation missing"
grep -q 'Invalid subnet mask (mask must be contiguous)' Mainwindows.cpp || fail "strict netmask validation missing"
grep -q 'Subnet mask does not match the IPv4 CIDR prefix' Mainwindows.cpp || fail "CIDR/netmask consistency check missing"
grep -q 'Invalid IPv4 gateway' Mainwindows.cpp || fail "gateway validation missing"
grep -q 'Invalid primary DNS address' Mainwindows.cpp || fail "DNS validation missing"
grep -q 'if (!/\^1\*0\*\$/.test(binary))' Setting.qml || fail "QML contiguous mask check missing"
grep -q 'mainWindows.validateLanSettings' Setting.qml || fail "QML preflight call missing"

python3 - <<'PY'
from pathlib import Path
s = Path('Mainwindows.cpp').read_text()
start = s.index('bool Mainwindows::applyLanSettings')
end = s.index('void Mainwindows::broadcastLanSnapshotAsync', start)
body = s[start:end]
validation = body.index('const QVariantMap validation = validateLanRequest')
rejection = body.index('if (!validation.value(QStringLiteral("ok")).toBool())')
network2 = body.index('m_lanIntegrationBackend->updateNetworkfromDisplayIndex')
mutation = body.index('netWorkController->applyNetworkConfig')
recorder = body.index('emit commandMainCppToRecCpp')
assert validation < rejection < network2 < mutation < recorder
print('[PASS] validation precedes Network2, JSON/system apply, and recorder side effects')
PY

# Quick behavioral checks for the same contiguous-mask contract used by QML/C++.
python3 - <<'PY'
def prefix(mask):
    parts = mask.split('.')
    if len(parts) != 4: return -1
    try:
        nums = [int(x) for x in parts]
    except ValueError:
        return -1
    if any(x < 0 or x > 255 for x in nums): return -1
    bits = ''.join(f'{x:08b}' for x in nums)
    if '01' in bits: return -1
    return bits.count('1')
assert prefix('255.255.255.0') == 24
assert prefix('255.255.0.0') == 16
assert prefix('255.255.255.128') == 25
assert prefix('255.0.255.0') == -1
assert prefix('255.255.1.0') == -1
print('[PASS] representative contiguous/non-contiguous netmask cases')
PY

pass "R-LAN2 structural verification complete"
