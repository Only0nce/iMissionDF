# NET-ENDPOINTS1.2 — Legacy Backend Reuse + Top Network Drawer Removal

Base: NET-ENDPOINTS1.1 QML parser fix.

## Goal
Make the new `Network Settings -> Endpoints` page the only runtime UI owner for DF Server / network service endpoint configuration, while preserving the proven legacy backend behavior that existed in `TopNetworkDrawer.qml`.

## DF Server Apply: legacy backend preserved
The old TopNetworkDrawer Apply path performed two calls:

1. `mainWindows.setNetworkFormDisplay(ip)` — compatibility/display hook (currently logging-only in `Mainwindows.cpp`).
2. `Krakenmapval.connectToDFserver(ip)` — the real existing iScreenDF mutation owner.

`ServiceEndpointsPage.qml` now preserves the same sequence. It does **not** introduce a new DF Server persistence owner.

The existing `iScreenDF::connectToDFserver()` remains unchanged and still:

- updates in-memory `Parameter.m_ipdfServer`,
- persists `Parameter.ipdfserver`,
- reconnects `TcpClientDF` to `<ip>:5555`,
- retargets GPSD to `<ip>:2947`,
- publishes `updateServeripDfserver(ip)` back to QML.

Compass Offset continues to use the existing `iScreenDF::setCompassOffset(double)` backend and existing `compass_offset` DB path.

## Old top-bar network UI removed
The legacy TopNetworkDrawer frontend is removed from runtime ownership:

- top-center click/drag network handle removed from `MainPage.qml`,
- `TopNetworkDrawer` instance removed from `MainPage.qml`,
- Menu no longer attempts `topDrawer.close()`,
- `TopNetworkDrawer.qml` removed from `qml.qrc`,
- obsolete `FEATURE_TOP_NETWORK_DRAWER` qmake/macro/context-property plumbing removed,
- feature-only visual marker removed from `Setting.qml`.

The historical `iScreenDFqml/pages/TopNetworkDrawer.qml` source file is retained in the source tree only as legacy reference; it is no longer in the QML resource bundle and cannot be opened by the application runtime.

## Runtime configuration ownership after this revision

`Side Settings -> Network Settings` is now the sole network/service configuration UI:

`LAN | Endpoints | WiFi | 5G | VPN`

Endpoints remains editable in both Viewer and Admin mode.

## Deliberately unchanged
- `iScreenDF::connectToDFserver()` backend implementation
- `Parameter.ipdfserver` DB contract
- RFSoC TCP port 5555
- GPSD port 2947
- `setCompassOffset()` and `compass_offset` persistence
- LAN1/LAN2/LAN3/LAN4 behavior
- WiFi / 5G / VPN behavior
- Viewer/Admin policy outside Endpoints
