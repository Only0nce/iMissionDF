# DHCP-UX1 — DHCP/Static Mode Draft Guard

## Problem
In `Setting.qml`, pressing **Using DHCP** immediately called `refreshDhcpInfo()`. For remote LAN3/LAN4 this reloads the persisted configuration (because `end0/end1` are not local NetworkManager interfaces), so a persisted `static` mode overwrote the just-selected DHCP state. More generally, any delayed async LAN readback could overwrite a user mode choice before Apply.

## Root cause
The UI used one variable (`useDhcp`) for both:
- persisted/runtime readback state; and
- the user's not-yet-applied edit choice.

Because asynchronous callbacks call `applyLanInfo()`, stale readback could reset `useDhcp` after the button click.

## Fix
- Added `lanModeDirty` as a QML edit-draft guard.
- Selecting **Using DHCP** or **Static Manual** marks the mode draft dirty.
- Async status/readback continues updating IP/link/status fields but does not overwrite mode while the draft is dirty.
- Selecting another LAN clears the draft and loads that interface's persisted mode.
- Explicit Refresh clears the draft and reloads state.
- Successful JSON persistence clears the draft, then normal readback owns the mode again.
- Removed the immediate `refreshDhcpInfo()` call from the **Using DHCP** button.
- Normalized NetworkManager `manual` as Static Manual, alongside JSON `static` and legacy off/false aliases.

## Scope
Only `Setting.qml` is modified. No changes to:
- `NetworkController.cpp`
- `Mainwindows.cpp`
- Network2 database integration
- RFSoC TCP `setIpConfig`
- LAN mappings
- WiFi/5G backend

## Validation target
1. Open LAN1 (or any editable LAN in Admin mode).
2. Press **Using DHCP**.
3. Wait several seconds — selection must remain DHCP.
4. Press **DHCP Info** — selection must remain DHCP while draft is un-applied.
5. Press **Apply / Save**.
6. After persistence/readback completes, DHCP remains selected if the saved mode is DHCP.
7. Select **Static Manual** and repeat.
8. Switch to another LAN while a draft is pending — new interface must load its own persisted mode.
