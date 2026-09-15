#!/bin/sh
set -eu

FILE="${1:-Setting.qml}"

fail() { echo "[FAIL] $*"; exit 1; }
pass() { echo "[PASS] $*"; }

[ -f "$FILE" ] || fail "$FILE not found"

grep -q 'readonly property bool lanKeyboardMode' "$FILE" || fail "keyboard-aware LAN mode missing"
grep -q 'Qt.inputMethod.visible' "$FILE" || fail "Qt input-method visibility detection missing"
grep -q 'Qt.inputMethod.keyboardRectangle.height > 0' "$FILE" || fail "keyboard rectangle fallback missing"
grep -q 'id: lanListPanel' "$FILE" || fail "LAN list panel id missing"
grep -q 'visible: !networkManager.lanKeyboardMode' "$FILE" || fail "keyboard-mode panel visibility rules missing"
grep -q 'id: lanDetailPanel' "$FILE" || fail "adaptive LAN detail panel missing"
grep -q 'id: ipv4ConfigCard' "$FILE" || fail "IPv4 card id missing"
grep -q 'id: keyboardEditSummary' "$FILE" || fail "keyboard editing summary missing"
grep -q 'id: lanActionRow' "$FILE" || fail "adaptive action row missing"
grep -q 'function closeVirtualKeyboard()' "$FILE" || fail "keyboard close helper missing"
grep -q 'text: "Done"' "$FILE" || fail "Done button missing"
grep -q 'closeVirtualKeyboard(); requestProtectedLanApply()' "$FILE" || fail "Apply does not commit/close keyboard first"

pass "keyboard-safe LAN editing mode present"
pass "normal backend apply entry point preserved"
pass "UX-KB1 structural verification complete"
