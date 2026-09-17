# NET-AUTH3 — Direct Viewer Entry

Base: latest project source containing NET-ENDPOINTS1.3 and R20.4 RADIO UX1.4.

## Goal
Selecting Network Settings from the main side drawer must open the Network Settings page immediately in Viewer mode. The pre-entry Viewer/Admin/Cancel popup is removed. Admin elevation remains available only from inside `Setting.qml` through the existing password flow.

## Root cause of the previous double-highlight bug
The old flow did not navigate immediately when Network Settings was tapped. It first latched a pending navigation and opened `NetworkAccessModePopup`. When Cancel was pressed, pending navigation was cleared but toolbar hover state could remain latched on Network Settings while the real `currentIndex` still pointed to the previous page (for example DOA Viewer). Since the delegate painted both `checked` and `hoveredIndex` with the same selected color, two mode buttons could appear selected.

## New entry contract
- Main side drawer: `Network Settings` navigates immediately.
- `MainPage.qml` always pushes `Setting.qml` with `networkAccessRole: "viewer"`.
- `Setting.qml` also resets its role to Viewer on component creation as a defensive fallback.
- There is no entry access-selection popup and no entry admin-password popup in `SideSettingsDrawer.qml`.
- Re-tapping Network Settings while already on that page does not recreate/downgrade the current page.
- Viewer -> Admin remains available through the existing in-page `NetworkPasswordPopup`.
- Admin -> Viewer remains immediate through the existing in-page role switch.
- Leaving Network Settings destroys/pops that page; entering it again starts Viewer again.

## Toolbar state hardening
`toolbar.currentIndex` is the persistent selected page. `toolbar.hoveredIndex` is only transient pointer state.

- Successful navigation sets `hoveredIndex = -1`.
- Leaving a hovered button clears the hover state unconditionally.
- The old Cancel path no longer exists.

This removes the trigger that produced DOA + Network double highlight and also prevents hover state from being treated as selection state.

## Preserved behavior
- Network tabs remain `LAN | Endpoints | WiFi | 5G | VPN`.
- Endpoints remains editable in both Viewer and Admin.
- Viewer/Admin permission matrix inside `Setting.qml` is unchanged.
- Existing in-page Admin password authentication is unchanged.
- Legacy Top Network Drawer restore from NET-ENDPOINTS1.3 is unchanged.
- VPN, NetworkController, RFSoC TCP, recorder, mute/audio, spectrum and other subsystems are unchanged.

## Target validation
1. From RADIO, DOA, MAP, or another page, tap Network Settings: it opens immediately with `VIEWER ACCESS` and no access-selection popup.
2. Tap the in-page Viewer/Admin switch: password prompt appears. Cancel leaves the page in Viewer mode.
3. Enter the correct admin password: page changes to Admin.
4. Navigate away from an Admin session and enter Network Settings again: it must start in Viewer.
5. Repeatedly switch DOA <-> Network: only one side-toolbar mode should have selected styling at a time.
6. Verify LAN1/LAN2/Endpoints/WiFi/5G permissions in Viewer and LAN1-LAN4/Endpoints/WiFi/5G/VPN control in Admin.
