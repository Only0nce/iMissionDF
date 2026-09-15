# NET-AUTH2.1 — In-Page Viewer/Admin Role Switch

## Base

This revision is developed directly on top of:

`src-DHCP-UX1.1-DHCP-DISABLED-FIELD-VISUAL-20260915.tar.xz`

It preserves NET-AUTH2 access rules and DHCP-UX1/1.1 behavior.

## Problem

The Network Settings header showed `VIEWER ACCESS` / `ADMIN ACCESS` only as a status badge. To change role, the operator had to leave Network Settings and enter again.

## New behavior

The access badge is now an interactive role switcher.

### Admin -> Viewer

- Click the `ADMIN ACCESS` button.
- The page immediately changes to Viewer mode.
- No password is required to reduce privileges.
- LAN2/LAN3/LAN4 become read-only immediately.
- If LAN2/LAN3/LAN4 had unsaved Admin edits, those drafts are discarded and the persisted/runtime LAN state is reloaded.

### Viewer -> Admin

- Click the `VIEWER ACCESS` button.
- The existing `NetworkPasswordPopup` opens.
- Password verification still uses the existing `NetworkSecurityController` including failed-attempt and lockout policy.
- Successful authentication changes the current page to Admin mode without page reload.
- Cancel or failed authentication leaves the page in Viewer mode.

## Permissions remain unchanged

| Area | Viewer | Admin |
|---|---|---|
| LAN1 | Edit/apply | Edit/apply |
| LAN2 | Read only | Edit/apply |
| LAN3 | Read only | Edit/apply |
| LAN4 | Read only | Edit/apply |
| WiFi | Edit/apply | Edit/apply |
| 5G | Edit/apply | Edit/apply |

The existing `applyLanSetting()` permission guard remains in place, so the role restriction is not visual-only.

## Scope

Production logic change is limited to `Setting.qml`.

No changes are made to:

- `NetworkController.cpp`
- `Mainwindows.cpp`
- `iScreenDF/DatabaseDF.cpp`
- `iScreenDF/functionTcpServer.cpp`
- `iScreenDF/functionMonitor.cpp`
- Network2 database contract
- `/etc/network_config.json` persistence
- LAN3/end0 and LAN4/end1 RFSoC mapping
- RFSoC `setIpConfig` protocol
- DHCP-UX1 draft-state guard
- DHCP-UX1.1 disabled-field visual state

## Bench acceptance

1. Enter Network Settings as Admin.
2. Select LAN2 and verify editing is enabled.
3. Click the `ADMIN ACCESS` switch.
4. Verify role changes to Viewer without password and LAN2 becomes read-only immediately.
5. Click `VIEWER ACCESS`.
6. Cancel password popup: role must remain Viewer.
7. Click again and enter a wrong password: role must remain Viewer and existing attempt/lockout policy must work.
8. Enter the correct password: role changes to Admin without leaving/reloading the Network Settings page.
9. Verify LAN2-LAN4 editing is re-enabled.
10. Verify LAN1/WiFi/5G remain editable in both roles.
