# NET-UX1 — Button Feedback + Apply Confirmation

Base: `src-NET-AUTH2.1-IN-PAGE-ROLE-SWITCH-20260915.tar.xz`

## Goal

Make Network Settings interactions visibly responsive and remove uncertainty around whether `Apply / Save` was actually pressed.

## Changes

### 1. Press feedback

The main Network Settings controls now use short scale animations on press/release:

- Viewer/Admin access switch
- LAN/WiFi/5G tabs
- LAN1-LAN4 cards
- Using DHCP / Static Manual
- keyboard `Done`
- Apply / Save
- Refresh
- DHCP Info / RFSoC Status
- Viewer/Admin entry cards and password-dialog action buttons
- confirmation-dialog buttons

The animation is intentionally short (roughly 80-100 ms) so it feels responsive and does not slow operation.

### 2. Two-step Apply / Save

The LAN `Apply / Save` button no longer calls the backend immediately.

New flow:

1. User taps `Apply / Save`.
2. The button gives press feedback.
3. A modal `Confirm Apply / Save` dialog appears.
4. The dialog shows the selected LAN interface and requested DHCP/Static mode. Static mode also shows IPv4/mask/gateway.
5. `Cancel` closes the dialog with no mutation.
6. `Confirm Apply` closes the dialog and calls the existing `applyLanSetting()` function.

The dialog intentionally says *Confirm Apply*, not *Applied successfully*. The existing backend/status path remains authoritative because a local dispatch/acceptance is not the same as a remote RFSoC apply ACK.

## Safety / compatibility

No networking backend contract was changed. The following production files remain byte-identical to NET-AUTH2.1:

- `NetworkController.cpp`
- `Mainwindows.cpp`
- `iScreenDF/DatabaseDF.cpp`
- `iScreenDF/functionTcpServer.cpp`
- `iScreenDF/functionMonitor.cpp`

Existing Viewer/Admin permissions, DHCP draft guard, DHCP disabled-field visuals, Network2 persistence, JSON persistence, LAN3=end0/LAN4=end1 and RFSoC `setIpConfig` behavior are preserved.

## Files changed

- `Setting.qml`
- `NetworkAccessModePopup.qml`
- `NetworkPasswordPopup.qml`

## Device validation

1. Enter Network Settings.
2. Tap LAN tabs/cards and confirm visible press feedback.
3. Toggle DHCP/Static and confirm the pressed button visibly responds.
4. Tap `Apply / Save`; verify no backend apply occurs until `Confirm Apply` is tapped.
5. Tap `Cancel`; verify configuration is not submitted.
6. Tap `Apply / Save` again, then `Confirm Apply`; verify the existing apply/status path runs.
7. Repeat in Viewer mode on LAN2-LAN4; `Apply / Save` must remain disabled/read-only.
