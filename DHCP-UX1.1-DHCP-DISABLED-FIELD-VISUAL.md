# DHCP-UX1.1 — DHCP Disabled Field Visual State

Base: `src-DHCP-UX1-DHCP-MODE-DRAFT-GUARD-20260915.tar.xz`

## Goal
Make the IPv4 editor visually distinguish fields that are editable in Static Manual mode from fields managed by DHCP.

## Behavior
- `Using DHCP`: IP Address, Subnet Mask, Gateway, Primary DNS, Secondary DNS are disabled and rendered with a gray background, muted border, and muted text/labels.
- `Static Manual`: those fields return to the normal active theme and accept input.
- Existing DHCP draft-state protection remains unchanged.
- Viewer/Admin permission behavior remains unchanged.
- No C++ networking, JSON, database, NetworkManager, or RFSoC TCP code is changed.

## Files changed
- `Setting.qml`

## Validation
Run `VERIFY-DHCP-UX1.1.sh`, then build with the project's normal qmake target and bench-test both DHCP and Static Manual modes.
