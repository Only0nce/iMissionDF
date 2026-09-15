# NET-UX2 — Network Tab / Main Panel Spacing Polish

## Base

`src-NET-AUTH2.2-VIEWER-LAN1-LAN2-EDIT-20260915.tar.xz`

## Problem

The LAN / WiFi / 5G tab row started at `y: 176` and used 48 px high buttons, so the row ended at y=224. The main Network panel started at `y: 220`, creating a 4 px overlap instead of a visual gutter. This made the header and content panel look cramped and visually unfinished.

## Change

Only `Setting.qml` presentation geometry was changed:

- Give the network tab row an id: `networkTabRow`.
- Increase tab-to-tab spacing from 14 px to 16 px.
- Move `mainPanel.y` from 220 to 248.
- Because panel height is still `parent.height - y - 60`, the lower edge remains unchanged; only the top gutter and content height change.

Result:

- Tab row bottom = 176 + 48 = 224.
- Main panel top = 248.
- Visual gutter = 24 px.

## Preserved behavior

No navigation, permissions, DHCP behavior, Apply confirmation, database, NetworkManager, RFSoC TCP, or persistence behavior is modified.

Viewer/Admin policy remains:

- Viewer: LAN1, LAN2, WiFi, 5G editable.
- Viewer: LAN3, LAN4 read-only.
- Admin: full edit access.

## Validation target

At the 1920x1080 reference resolution, LAN/WiFi/5G should read as a separate navigation layer above the main settings card, with clear whitespace between the buttons and the card border.
