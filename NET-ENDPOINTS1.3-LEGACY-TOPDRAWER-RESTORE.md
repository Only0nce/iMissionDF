# NET-ENDPOINTS1.3 — Temporary Legacy Top Network Drawer Restore

Base: NET-ENDPOINTS1.2

## Requirement
Temporarily restore the legacy top-bar Network Settings drawer exactly as the operator used before, until DF Server IP configuration through the new Endpoints workflow is fully validated. Keep the new Endpoints tab and its reused backend in place for continued development/testing.

## What was restored
The runtime wiring removed in NET-ENDPOINTS1.2 was restored from the immediately preceding NET-ENDPOINTS1.1 baseline:

- top-center grab/click handle in `MainPage.qml`
- `TopNetworkDrawer {}` runtime instance
- `TopNetworkDrawer.qml` resource registration in `qml.qrc`
- `FEATURE_TOP_NETWORK_DRAWER` qmake feature flag
- `FeatureTopNetworkDrawer` QML context property
- the matching visual marker/comment path in Network Settings

The feature flag is enabled in `iScanMR10.pro`, so the old top drawer is active again by default.

## New Endpoints page remains
NET-ENDPOINTS1.2's new `ServiceEndpointsPage.qml` is intentionally **not** reverted.

Therefore both paths are available temporarily:

1. Legacy Top Network Drawer (fallback / known operator path)
2. Network Settings -> Endpoints (new path under validation)

The Endpoints page still uses the legacy backend bridge:

- `mainWindows.setNetworkFormDisplay(ip)`
- `Krakenmapval.connectToDFserver(ip)`
- `Krakenmapval.setCompassOffset(value)`

## Important backend behavior
The restored legacy drawer does not call the removed/commented `connectToserverKraken()` method. Its Reconnect path already falls back to `connectToDFserver()` in the current source, so the restored UI uses an active backend path.

## Deliberately unchanged
- LAN / WiFi / 5G / VPN pages
- Viewer/Admin permissions
- VPN first-entry refresh behavior
- QML parser fixes
- shutdown hardening
- NetworkController implementation
- ServiceEndpointsPage implementation
- RFSoC LAN3/LAN4 contract

## Removal later
Once the user confirms the new Endpoints IP workflow is correct on target hardware, the legacy top drawer can be removed again in a later revision. Do not remove it before that explicit confirmation.
