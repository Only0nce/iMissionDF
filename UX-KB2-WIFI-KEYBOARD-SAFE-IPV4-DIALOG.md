# UX-KB2 — WiFi Keyboard-Safe IPv4 Dialog

## Baseline
Derived from `src-UX-KB1.2-PERSISTENT-COMPACT-LAN-20260914.tar.xz`, itself based on the user-authoritative `src.tar(20260914-101931).xz` line.

## Problem
The WiFi IPv4 configuration panel was vertically centered and 370 px tall. On the target display, Qt Virtual Keyboard overlaps the lower part of the panel, hiding DNS fields and the DNS/Cancel/Apply action row.

## Design
The WiFi IPv4 editor now uses a permanent compact layout that does not move when the keyboard opens or closes:

- fixed near the top of the WiFi page (`y: 24`)
- widened up to 1120 px and reduced to 282 px high
- DHCP/Static controls remain at the top-left
- DNS mode / Cancel / Apply remain at the top-right
- IPv4, Subnet Mask, Gateway are arranged in three columns
- Primary DNS, Secondary DNS, and Current status are arranged in the second row
- `Done` appears only while the virtual keyboard is visible and hides the keyboard without changing the panel geometry
- closing/cancelling commits and hides the input method before dismissing the panel

## Preserved contracts
No changes were made to:

- Wifi5GController C++ backend
- NetworkManager/nmcli behavior
- WiFi scan/connect/forget workflow
- WiFi IPv4 save signal or payload schema
- LAN UI/backend
- Network2 database integration
- `/etc/network_config.json`
- RFSoC TCP behavior

Only `Wifi5GView.qml` was changed for runtime UI behavior.

## Validation target
1. Open WiFi tab.
2. Select a WiFi profile and open Config IP.
3. Focus IPv4, Gateway, Primary DNS, and Secondary DNS one by one.
4. Verify keyboard can remain open while all fields, DHCP/Static, DNS mode, Cancel, and Apply remain visible.
5. Press Done and verify keyboard closes without moving the panel.
6. Apply DHCP and Static settings and verify existing backend behavior is unchanged.
