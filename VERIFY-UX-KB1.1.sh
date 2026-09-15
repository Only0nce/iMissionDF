#!/bin/bash
set -euo pipefail
f="${1:-Setting.qml}"

grep -q 'readonly property bool lanKeyboardMode: selectedTab === "lan" && Qt.inputMethod.visible' "$f"
grep -q 'width: networkManager.lanKeyboardMode ? 330 : 420' "$f"
grep -q 'visible: true' "$f"
grep -q 'x: networkManager.lanKeyboardMode ? 382 : 484' "$f"
if grep -q 'Qt.inputMethod.keyboardRectangle.height > 0' "$f"; then
  echo '[FAIL] stale keyboardRectangle visibility latch still present'
  exit 1
fi
if grep -A8 'id: lanListPanel' "$f" | grep -q 'visible: !networkManager.lanKeyboardMode'; then
  echo '[FAIL] LAN list is still hidden in keyboard mode'
  exit 1
fi

echo '[PASS] keyboard mode follows actual input-method visibility'
echo '[PASS] LAN1-LAN4 list remains visible in keyboard mode'
echo '[PASS] keyboard-mode compact list and shifted detail layout present'
echo '[PASS] UX-KB1.1 structural verification complete'
