# NET-VPN1 — NetworkManager VPN Page

## Base
Built on `src-WIFI-RUNTIME1-STALE-DESTRUCTION-CALL-FIX-20260915.tar.xz`.

## Purpose
Add a real VPN subsystem to Network Settings instead of a cosmetic tab.

## UX
Top navigation is now `LAN | WiFi | 5G | VPN` with the existing 24 px tab-to-panel gutter.
The VPN page contains an overview card, active tunnel state, NetworkManager VPN/WireGuard profile list, Refresh, and per-profile Connect/Disconnect controls.

## Access policy
- Viewer: may inspect VPN status and profiles only.
- Admin: may connect/disconnect existing NetworkManager VPN/WireGuard profiles.
- No VPN secrets, passwords, private keys, or tokens are exposed to QML/log output.

## Backend
`NetworkController` adds:
- `vpnStatus()`
- `requestVpnStatus()`
- `setVpnConnectionActive(uuid, active)`
- `vpnStatusReady(...)`
- `vpnOperationFinished(...)`

The implementation uses `nmcli` argument lists, not shell command concatenation. Profile UUID is used for connect/disconnect so duplicate profile names do not select the wrong profile.

## Scope intentionally deferred
This phase does not import/create/edit VPN profiles. Profiles must already exist in NetworkManager. That keeps the first production integration small and avoids storing or exposing VPN credentials in the UI.

## Files changed
- `Setting.qml`
- `NetworkAccessModePopup.qml`
- `VpnPage.qml` (new)
- `NetworkController.h`
- `NetworkController.cpp`
- `qml.qrc`

## Runtime verification
1. Open Network Settings -> VPN.
2. Confirm existing NetworkManager VPN/WireGuard profiles appear.
3. Viewer: Connect/Disconnect must show `Admin Required` and remain disabled.
4. Elevate to Admin; buttons become active without page reload.
5. Connect a profile and verify status becomes Connected.
6. Disconnect it and verify status becomes Disconnected.
7. Confirm LAN/WiFi/5G behavior is unchanged.
