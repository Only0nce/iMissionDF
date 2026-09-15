# UX-KB1 — Network LAN Keyboard-Safe Layout

## Base
Authoritative base: `src.tar(20260914-101931).xz`.

## Problem
On the 1920x1080 Network Settings LAN page, the Qt Virtual Keyboard overlays the lower half of the screen. The existing fixed-position IPv4 editor and bottom action row can therefore be hidden while the user is editing an address, subnet mask, gateway, or DNS value.

## Design
This revision changes QML layout only. No networking backend, JSON persistence, database schema, RFSoC TCP protocol, or LAN mapping is modified.

When the virtual keyboard is active on the LAN tab:

1. The left LAN interface list is temporarily hidden.
2. The selected-interface panel expands horizontally.
3. The main panel moves upward from y=270 to y=220.
4. The IPv4 Configuration card moves from y=250 to y=95.
5. The telemetry summary row and Device Information card are temporarily hidden because they are not required while typing.
6. A compact editing summary appears beside the IPv4 form.
7. Apply / Refresh / RFSoC Status (or DHCP Info) move beside the form and remain above the keyboard area.
8. A `Done` button explicitly removes TextField focus and hides the virtual keyboard.
9. Apply / Save closes the keyboard first so the active TextField commits its editing state before the apply path runs.
10. When the keyboard closes, the normal layout is restored automatically.

## Keyboard detection
The editing layout uses both:

- `Qt.inputMethod.visible`
- `Qt.inputMethod.keyboardRectangle.height > 0`

This makes the page responsive to the Qt Virtual Keyboard/InputPanel state without coupling `Setting.qml` to the `KeyboardInput` object id in `main.qml`.

## Files changed
- `Setting.qml`

## Preserved contracts
- LAN1 / LAN2 apply behavior
- LAN3 = end0, LAN4 = end1 mapping
- Network2 database integration
- `/etc/network_config.json` persistence
- RFSoC TCP `setIpConfig` format/path
- WiFi / 5G loader behavior
- Existing normal (keyboard closed) LAN layout

## Runtime validation
1. Open Network Settings -> LAN.
2. Select LAN1, LAN2, LAN3, and LAN4 in turn.
3. Tap IP Address, Subnet Mask, Gateway, Primary DNS, and Secondary DNS.
4. Confirm the keyboard opens and the selected IPv4 form stays fully visible.
5. Confirm Apply / Save stays visible and clickable.
6. Press Done and confirm the keyboard closes and the original LAN overview layout returns.
7. Apply a harmless test value on the intended bench setup and verify the existing backend behavior is unchanged.
