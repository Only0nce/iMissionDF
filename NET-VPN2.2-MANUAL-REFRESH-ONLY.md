# NET-VPN2.2 — Manual Refresh Only

Base: NET-STAB1 / NET-VPN2.1

## Problem
`VpnPage.qml` periodically called `NetworkController.requestVpnStatus()` every 3 seconds while the page was visible. It also automatically queried VPN status and Public IP when the page was created. This made the VPN page appear to refresh continuously and generated unnecessary background work.

## Change
VPN page refresh is now operator-driven.

- Removed the 3-second reconciliation `Timer`.
- Removed automatic VPN/Public-IP refresh from `Component.onCompleted`.
- Public IP is queried only by the explicit **Refresh** button.
- Initial page message is now `Press Refresh to load VPN status`.
- Removed the deferred automatic Public-IP refresh after VPN enable/connect/disconnect operations.

## Important lifecycle behavior retained
Connect, Disconnect, Enable and Disable still receive one authoritative VPN-status readback from the C++ backend after the requested operation. This is transaction verification, not background polling, and prevents the UI from trusting only an `nmcli` exit code.

External VPN changes performed outside iScanMR10 will no longer appear automatically. Press **Refresh** to synchronize the page with NetworkManager.

## Product-code scope
Only `VpnPage.qml` changed. NetworkController, LAN, WiFi, 5G, RFSoC, database, popup and shutdown hardening behavior remain unchanged.
