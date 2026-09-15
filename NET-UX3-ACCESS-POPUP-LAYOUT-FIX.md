# NET-UX3 — Network Access Popup Layout Fix

## Problem
`NetworkAccessModePopup.qml` was sized at 640x360 while the Viewer/Admin cards now contain longer access-policy text after LAN2 and VPN permissions were added. The card content used a plain `Column` inside a height-constrained `Button`, so wrapped policy/help text could exceed the available vertical space and visually overlap the footer text.

## Fix
- Increase popup size from 640x360 to 720x460 for the 1920x1080 target UI.
- Increase outer padding to 28 px and card gap to 18 px.
- Give the Viewer/Admin card row a minimum height of 248 px.
- Replace fixed-flow `Column` card content with `ColumnLayout` so wrapped text participates in layout sizing.
- Add flexible vertical space and a separator above each footer/help line.
- Allow heading/footer text to wrap safely.
- Preserve all Viewer/Admin permission behavior and VPN behavior; this revision is layout-only.

## Expected result
Viewer/Admin policy text and footer text no longer overlap or stack on top of each other. Both cards remain balanced and readable at 1920x1080.
