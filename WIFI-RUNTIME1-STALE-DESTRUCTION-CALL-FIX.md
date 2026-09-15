# WIFI-RUNTIME1 — Stale destruction-call cleanup

## Problem
Runtime log repeatedly reported:

`qrc:/Wifi5GPage.qml:1539: ReferenceError: clearPendingWifiAdvancedSave is not defined`

The symbol was removed in NET-AUTH1 when the WiFi advanced IPv4 password gate moved away from per-Apply authentication, but one call remained in `Component.onDestruction`.

## Root cause
JavaScript/QML stops the current handler when an undefined function is called. Therefore the stale call prevented the following cleanup from executing:

- `deactivateWifiPageRuntime()`
- `deactivateCellularPageRuntime()`

The second path can send `lte_realtime_stop`, so leaving the error in place can allow background page runtime work to outlive the page transition.

## Fix
Remove only the dead `clearPendingWifiAdvancedSave()` call from `Component.onDestruction` and retain the live runtime cleanup sequence:

1. stop `pageRuntimeSyncTimer`
2. deactivate WiFi page runtime
3. deactivate cellular page runtime

No WiFi connect/scan/config semantics are changed.

## Scope
Production change: `Wifi5GPage.qml` only.

Unchanged: NetworkController, Mainwindows, database, RFSoC TCP, LAN permissions, DHCP draft guard, Apply confirmation, network UI spacing.

## Validation target
After repeatedly switching LAN -> WiFi -> 5G -> LAN, there must be no `clearPendingWifiAdvancedSave` ReferenceError. Expected lifecycle logs may include WiFi page STOP and 5G realtime stop activity.
