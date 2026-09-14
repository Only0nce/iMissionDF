#!/bin/sh
set -eu
QML="${1:-Setting.qml}"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

grep -q 'function externalLanConfiguredIpText()' "$QML" || fail "configured IP helper missing"
grep -q 'function externalLanTargetText()' "$QML" || fail "external target helper missing"
grep -q 'Configured IPv4' "$QML" || fail "Configured IPv4 presentation missing"
grep -q 'externalLanTargetText()' "$QML" || fail "RFSoC card does not present target interface"
! grep -q '"TCP · " + externalLanEndpointText()' "$QML" || fail "management endpoint still rendered as LAN IP"
pass "LAN3/LAN4 present configured interface IP rather than management endpoint"
