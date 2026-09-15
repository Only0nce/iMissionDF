# NET-AUTH2 — Viewer / Admin Network Access Roles

Base: `src-NET-AUTH1-NETWORK-ENTRY-PASSWORD-20260915.tar.xz`

## Goal

Replace the single mandatory administrator-entry flow with an explicit access-mode choice while preserving the existing network backend.

## Access model

| Area | Viewer | Admin |
|---|---|---|
| LAN1 / enP8p1s0 | Edit + Apply | Edit + Apply |
| LAN2 / enP1p1s0 | View only | Edit + Apply |
| LAN3 / end0 | View only | Edit + Apply |
| LAN4 / end1 | View only | Edit + Apply |
| WiFi | Edit + Apply | Edit + Apply |
| 5G | Edit + Apply | Edit + Apply |

Viewer entry does not require the administrator password. Admin entry reuses the existing `NetworkPasswordPopup` and `NetworkSecurityController` verification/lockout policy.

## Navigation

1. User selects `NETWORK SETTINGS`.
2. `NetworkAccessModePopup` asks for Viewer or Admin.
3. Viewer navigates directly to `Setting.qml` with `networkAccessRole=viewer`.
4. Admin opens the existing password popup; after successful verification the page is pushed with `networkAccessRole=admin`.
5. Leaving Network Settings clears the drawer's access role back to Viewer so administrator access is not carried across page exits.

## LAN permission enforcement

`Setting.qml` keeps LAN2-LAN4 visible in Viewer mode for status/diagnostics, but:

- the IPv4 configuration card is disabled,
- LAN cards are marked `VIEW ONLY`,
- the form is marked `READ ONLY`,
- Apply becomes disabled and displays `View Only`, and
- `applyLanSetting()` repeats the permission check before any validation/backend call.

This means the restriction is not only cosmetic inside the Network Settings page.

## Preserved behavior

No changes are made to:

- NetworkController networking logic,
- `/etc/network_config.json` persistence,
- Network2 database integration,
- LAN1/LAN2 NetworkManager apply behavior,
- LAN3=end0 / LAN4=end1 mapping,
- RFSoC `setIpConfig` TCP contract,
- WiFi/5G backend behavior,
- administrator password hash / failed-attempt / lockout implementation.

## Files changed

- `iScreenDFqml/pages/SideSettingsDrawer.qml`
- `MainPage.qml`
- `Setting.qml`
- `qml.qrc`
- added `NetworkAccessModePopup.qml`

