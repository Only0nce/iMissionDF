# NET-VPN2.3 — First-Entry Auto Refresh

Base: NET-VPN2.2 manual-refresh-only behavior.

## Requirement
Refresh VPN status/Public IP automatically only the first time the operator enters the VPN tab during a Network Settings page session. Subsequent LAN/WiFi/5G -> VPN returns must not refresh automatically; the operator must press Refresh.

## Root cause / design note
`VpnPage.qml` is instantiated through a Loader whose `active` property follows the selected tab. Leaving the VPN tab destroys the page object and re-entering creates a new one. Therefore a "first entry" guard stored inside `VpnPage.qml` would reset and incorrectly auto-refresh every time.

## Implementation
`Setting.qml` owns:

```qml
property bool vpnInitialRefreshDone: false
```

When `vpnLoader` loads:

```qml
if (!networkManager.vpnInitialRefreshDone) {
    networkManager.vpnInitialRefreshDone = true
    item.refreshAll()
}
```

The flag lives for the lifetime of the Network Settings page. Leaving Network Settings and entering it again creates a new page session, so the first VPN entry of the new session refreshes once again.

## Preserved behavior
- No 3-second VPN polling timer.
- No `Component.onCompleted` auto-refresh in `VpnPage.qml`.
- Manual Refresh remains available.
- Connect/Disconnect/Enable/Disable retain one authoritative backend status readback after the operator action.
- VPN/LAN/network backend behavior is unchanged.
