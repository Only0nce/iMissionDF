# NET-ENDPOINTS1 — Service Endpoints Tab

Date: 2026-09-15

## Goal
Add a first-class **Endpoints** tab to Network Settings, ordered:

`LAN | Endpoints | WiFi | 5G | VPN`

Service Endpoints are editable in both Viewer and Admin modes.

## UI structure
The page is intentionally split into two cards instead of reproducing the dense Top Bar layout.

### DF Server Endpoint
- DF Server IP
- Apply
- Reconnect
- Saved endpoint summary including TCP 5555 and GPSD 2947

### Global Offsets
- Compass Offset
- Set Compass Offset

## Reused legacy behavior
The implementation reuses the existing iScreenDF/Top Bar backend contracts instead of creating a second endpoint configuration owner.

### Apply DF Server IP
`connectToDFserver(ip)` remains the mutation owner:

1. update in-memory `Parameter.m_ipdfServer`
2. queue DB update of `Parameter.ipdfserver`
3. reconnect DF TCP client to `<ip>:5555`
4. move GPSD endpoint to `<ip>:2947`
5. publish `updateServeripDfserver(ip)` back to QML

### Reconnect
The old Top Bar attempted to call `connectToserverKraken()`, but that legacy path is no longer an active public slot in the current iScreenDF implementation.

NET-ENDPOINTS1 adds `reconnectDFserver()` which deliberately **does not rewrite the database**. It reopens the already-saved runtime endpoint using the active `TcpClientDF::connectToServer()` path. That method aborts the old socket before `connectToHost()`, so Reconnect is operational rather than cosmetic.

GPSD is also pointed back to the saved endpoint on reconnect.

### Compass Offset
`setCompassOffset(double)` remains the existing mutation owner:

1. update `Parameter.m_compass_offset`
2. queue DB update of `compass_offset`
3. emit `updateGlobalOffsets(...)`

## Readback
`requestServiceEndpointsState()` emits the current in-memory DF Server IP and global offsets to the page. `ServiceEndpointsPage.qml` requests this state on load and again after actions.

## Access model
Both roles can edit the Endpoints page:

- Viewer: LAN1, LAN2, Endpoints, WiFi, 5G editable; LAN3/LAN4 and VPN mutation remain restricted.
- Admin: LAN1-LAN4, Endpoints, WiFi, 5G and VPN control available.

There is no `adminMode` gate in `ServiceEndpointsPage.qml` by design.

## Files involved
- `ServiceEndpointsPage.qml` (new)
- `Setting.qml`
- `NetworkAccessModePopup.qml`
- `iScreenDF/iScreenDF.h`
- `iScreenDF/functionTcpServer.cpp`
- `qml.qrc`

The source package also contains prior accepted Network/VPN/stability work already present in the current development baseline.

## Device validation
1. Enter Network Settings as Viewer. Confirm tab order is LAN / Endpoints / WiFi / 5G / VPN.
2. Open Endpoints. Both cards must be editable.
3. Change DF Server IP to a valid address and press Apply.
4. Confirm DB field `Parameter.ipdfserver` changes and DF TCP reconnects on port 5555.
5. Confirm GPSD endpoint follows the same host on port 2947.
6. Press Reconnect without modifying the field. Confirm the saved endpoint is reused and DB is not rewritten.
7. Change Compass Offset, press Set Compass Offset, and confirm `Parameter.compass_offset` persists.
8. Enter Network Settings as Admin and repeat; behavior must be identical.
9. Verify LAN/WiFi/5G/VPN behavior is unaffected.
