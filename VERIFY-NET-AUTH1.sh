#!/bin/sh
set -eu
ROOT="${1:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}"
SIDE="$ROOT/iScreenDFqml/pages/SideSettingsDrawer.qml"
SETTING="$ROOT/Setting.qml"
WIFI="$ROOT/Wifi5GPage.qml"
SEC_H="$ROOT/NetworkSecurityController.h"
SEC_CPP="$ROOT/NetworkSecurityController.cpp"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

grep -q 'import "../.." as SharedComponents' "$SIDE" || fail "SharedComponents import missing"
grep -q 'id: networkEntryPasswordPopup' "$SIDE" || fail "network entry password popup missing"
grep -q 'source !== settingsPanel.wifi5gSourceUrl' "$SIDE" || fail "network navigation gate missing"
grep -q 'networkEntryPasswordPopup.requestUnlock()' "$SIDE" || fail "entry password request missing"
grep -q 'settingsPanel.performToolbarNavigation(index, title, source)' "$SIDE" || fail "authorized navigation continuation missing"
pass "Network Settings navigation is password-gated before page entry"

if grep -q 'lanApplyPasswordPopup\|requestProtectedLanApply' "$SETTING"; then
    fail "LAN per-Apply password gate still present"
fi
grep -q 'closeVirtualKeyboard(); applyLanSetting()' "$SETTING" || fail "LAN Apply does not directly use existing apply flow"
pass "LAN Apply no longer asks for administrator password"

if grep -q 'wifiApplyPasswordPopup\|requestProtectedWifiAdvancedSave\|pendingWifiAdvancedSettings' "$WIFI"; then
    fail "WiFi per-Apply password gate still present"
fi
grep -q 'root.saveWifiAdvanced(root.copyObject(settings || {}))' "$WIFI" || fail "WiFi advanced Apply direct flow missing"
pass "WiFi advanced Apply no longer asks for administrator password"

grep -q 'Q_INVOKABLE bool verifyPassword' "$SEC_H" || fail "central security verifier missing"
grep -q 'kMaxFailedAttempts = 5' "$SEC_H" || fail "failed-attempt policy missing"
grep -q 'kLockoutDurationMs = 30000' "$SEC_H" || fail "lockout policy missing"
grep -q 'constantTimeEquals' "$SEC_CPP" || fail "constant-time password comparison missing"
pass "Existing centralized password verification and lockout policy retained"

pass "NET-AUTH1 structural verification complete"
