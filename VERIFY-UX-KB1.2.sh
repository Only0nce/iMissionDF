#!/bin/sh
set -eu
FILE="${1:-Setting.qml}"
fail(){ echo "[FAIL] $1"; exit 1; }
pass(){ echo "[PASS] $1"; }
[ -f "$FILE" ] || fail "$FILE not found"
grep -q 'readonly property bool lanKeyboardMode: selectedTab === "lan" && Qt.inputMethod.visible' "$FILE" || fail "keyboard visibility contract missing"
grep -A10 'id: mainPanel' "$FILE" | grep -q 'y: 220' || fail "main panel is not permanently compact"
grep -A12 'id: lanListPanel' "$FILE" | grep -q 'width: 330' || fail "LAN list is not permanently compact"
grep -A12 'id: lanDetailPanel' "$FILE" | grep -q 'x: 382' || fail "detail panel is not permanently compact"
grep -A8 'id: ipv4ConfigCard' "$FILE" | grep -q 'y: 95' || fail "IPv4 card is not permanently raised"
grep -A12 'id: keyboardEditSummary' "$FILE" | grep -q 'visible: true' || fail "editing summary is not persistent"
grep -A8 'id: lanActionRow' "$FILE" | grep -q 'x: 650' || fail "action row x is not persistent"
grep -A8 'id: lanActionRow' "$FILE" | grep -q 'y: 220' || fail "action row y is not persistent"
# Layout geometry must no longer depend on keyboard mode.
if grep -E '^[[:space:]]*(x|y|width|height):.*lanKeyboardMode' "$FILE" >/dev/null 2>&1; then
  fail "LAN geometry still depends on keyboard mode"
fi
pass "persistent compact LAN layout present"
pass "keyboard visibility affects controls only, not geometry"
