# NET-AUTH2.2 — Viewer LAN1 + LAN2 Edit Access

## Goal
Adjust the Network Settings Viewer permission model so the two local Jetson Ethernet ports are editable without administrator elevation.

## Access Matrix

| Interface / Feature | Viewer | Admin |
|---|---|---|
| LAN1 / enP8p1s0 | Edit + Apply | Edit + Apply |
| LAN2 / enP1p1s0 | Edit + Apply | Edit + Apply |
| LAN3 / end0 | View only | Edit + Apply |
| LAN4 / end1 | View only | Edit + Apply |
| WiFi | Edit + Apply | Edit + Apply |
| 5G | Edit + Apply | Edit + Apply |

## Implementation
`Setting.qml::canEditLanByIndex()` is the authoritative QML permission gate:

```qml
return networkAdminMode || index === 0 || index === 1
```

The same function continues to drive:
- edit-field enable/disable state,
- VIEW ONLY / READ ONLY indicators,
- Apply / Save enablement,
- the permission check inside `applyLanSetting()`,
- protected draft reload when Admin is downgraded to Viewer.

Therefore LAN2 is not only visually unlocked; it can pass the same existing production Apply path as LAN1. LAN3 and LAN4 remain protected in Viewer mode.

## Preserved Behavior
No C++ networking/database/RFSoC backend implementation is changed. The following behavior is retained:
- LAN1/LAN2 local NetworkManager apply,
- LAN2 Recorder side effect,
- LAN3=end0 and LAN4=end1 mapping,
- Network2 database integration,
- `/etc/network_config.json` persistence,
- RFSoC TCP `setIpConfig`,
- Viewer/Admin in-page switching,
- DHCP mode draft guard,
- DHCP disabled-field visual state,
- Apply confirmation popup and button feedback animation.

## Validation
In Viewer mode:
1. Select LAN1 and verify editing + Apply / Save are enabled.
2. Select LAN2 and verify editing + Apply / Save are enabled.
3. Select LAN3 and verify VIEW ONLY / READ ONLY state.
4. Select LAN4 and verify VIEW ONLY / READ ONLY state.
5. Verify WiFi and 5G remain editable.
6. Switch to Admin and verify LAN3/LAN4 unlock without changing LAN1/LAN2 behavior.
