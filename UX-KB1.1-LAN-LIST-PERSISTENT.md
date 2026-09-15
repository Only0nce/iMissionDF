# UX-KB1.1 — Persistent LAN List During Virtual Keyboard Editing

Base: `src.tar(20260914-101931).xz` + UX-KB1 keyboard-safe LAN layout.

## Problem
UX-KB1 intentionally hid the LAN interface list while the Qt Virtual Keyboard was visible. In practice this removed LAN1-LAN4 context and prevented convenient switching while editing. In addition, `Qt.inputMethod.keyboardRectangle.height > 0` can remain non-zero after the keyboard is hidden on Qt 5, which could leave the page stuck in keyboard-safe mode.

## Fix
- `lanKeyboardMode` now follows `Qt.inputMethod.visible` only.
- LAN Interfaces panel remains visible and enabled in both normal and keyboard modes.
- While the keyboard is visible, the LAN list becomes compact instead of disappearing.
- LAN detail panel shifts right of the compact list while retaining the raised IPv4 form and keyboard-safe action controls.
- LAN1-LAN4 remain selectable during keyboard editing.
- No C++, database, JSON, NetworkManager, RFSoC TCP, or protocol behavior is changed.

## Expected behavior
1. Focus any LAN IPv4 field and show the virtual keyboard.
2. LAN1-LAN4 remain visible in a compact list on the left.
3. The active IPv4 form and Apply/Refresh/Status controls remain above the keyboard.
4. Selecting another LAN from the compact list still works.
5. Closing the keyboard restores the original full-size LAN list/layout.
