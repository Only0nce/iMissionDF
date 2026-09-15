# UX-KB1.2 — Persistent Compact LAN Layout

## Goal
Keep the keyboard-safe LAN layout as the normal LAN layout at all times. Opening or closing the Qt Virtual Keyboard must not move or resize the main LAN panels.

## Behavior
- LAN1-LAN4 list remains visible at all times.
- The LAN list always uses the compact dimensions previously used only while the keyboard was visible.
- The selected-interface detail panel always starts at the compact position.
- IPv4 Configuration always remains at the upper-left of the detail panel.
- The editing summary and Apply/Refresh/Status actions always remain at the upper-right.
- Summary metric cards and the old Device Information card remain hidden in this layout to avoid a second layout mode.
- `Qt.inputMethod.visible` is used only to show the `Done` button while a keyboard is actually open.
- Opening/closing the keyboard does not trigger any LAN layout transition.

## Scope
QML-only change in `Setting.qml`.

No changes to:
- NetworkController / NetworkManager behavior
- `/etc/network_config.json`
- Network2 database
- LAN3=end0 / LAN4=end1 mapping
- RFSoC TCP transport or `setIpConfig`
- WiFi / 5G backend
