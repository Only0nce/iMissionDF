# NET-ENDPOINTS1.4 — Existing Backend Reuse Only

Base: R20.4-STAB2 + NET-AUTH3 + NET-ENDPOINTS1.3 + RADIO UX1.4.

## Goal
Wire the new Service Endpoints tab to the already-existing backend/database paths only. Do not add a new DB query, signal, slot, state owner, or endpoint API.

## Existing paths reused

### DF Server initial state
The page calls the existing `iScreenDF::getNetworkfromDb(1)` slot. That path already refreshes Network2 when necessary and emits:

- `networkRowUpdated(row)` with the existing `krakenserver` DB payload.
- `updateGlobalOffsets(...)` from the already-loaded RF parameter state.

The page also keeps listening to the existing `updateServeripDfserver(ip)` signal, which is the established Parameter.ipdfserver/runtime notification used by the legacy UI.

### DF Server Apply
Exactly the proven legacy calls are retained:

- `mainWindows.setNetworkFormDisplay(ip)` compatibility hook.
- `Krakenmapval.connectToDFserver(ip)` real mutation/connection owner.

No replacement backend was created.

### DF Server Reconnect
No dedicated reconnect API is added. Reconnect uses the existing `connectToDFserver(savedIp)` path with the last loaded/applied value, never an unsaved TextField draft.

### Compass Offset
The page uses the existing:

- `setCompassOffset(value)`
- `updateGlobalOffsets(offsetValue, compassOffset)`

No new compass persistence path was introduced.

## Removed additions from NET-ENDPOINTS1
The following previously-added APIs were removed again:

- `iScreenDF::reconnectDFserver()`
- `iScreenDF::requestServiceEndpointsState()`

The extra `updateServeripDfserver()` emission that had been inserted into `connectToDFserver()` was also removed so the backend returns to its existing pre-ENDPOINTS behavior.

## UI behavior
- Endpoints remains editable in Viewer and Admin.
- Tab order remains `LAN | Endpoints | WiFi | 5G | VPN`.
- The temporary legacy Top Network Drawer remains available.
- Apply and Reconnect use the same existing DF backend.
- Reconnect uses the saved/applied field, not an unsaved draft.

## Scope
No changes were made to LAN1-LAN4, WiFi, 5G, VPN, recorder, audio, RFSoC IP configuration packets, or runtime hardening from R20.4-STAB2.
