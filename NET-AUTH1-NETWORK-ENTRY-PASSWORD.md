# NET-AUTH1 — Network Settings Entry Password Gate

## Base

Authoritative user base: `src.tar(20260915-020033).xz`.

## Requirement

Ask for the network administrator password once before entering the unified
Network Settings page (`qrc:/Setting.qml`). After successful entry, LAN/WiFi/5G
Apply actions must not ask for the same administrator password again.

Leaving Network Settings ends that UI access session. Selecting Network Settings
again from another page requires the password again.

## Implementation

### SideSettingsDrawer.qml

- Added a `NetworkPasswordPopup` gate before navigation to `qrc:/Setting.qml`.
- Navigation is held in pending state until password verification succeeds.
- Toolbar selection and page navigation are not changed on Cancel or failed
  authentication.
- The existing centralized `networkSecurity` backend remains the verifier, so
  password hashing, failed-attempt counting and lockout behavior are preserved.
- Non-network pages keep the existing navigation behavior.

### Setting.qml

- Removed the per-Apply LAN password popup.
- `Apply / Save` now closes the virtual keyboard and directly executes the
  existing `applyLanSetting()` flow.
- No LAN validation, JSON, database, NetworkManager, RFSoC, or TCP behavior was
  changed.

### Wifi5GPage.qml

- Removed the per-Apply WiFi advanced IPv4 password popup and temporary pending
  authorization state.
- WiFi advanced save now calls the existing `saveWifiAdvanced()` flow directly
  after the page-level entry gate has already authenticated the user.
- WiFi SSID passwords are unrelated credentials and remain unchanged.

## Intentionally unchanged

- `NetworkSecurityController` password hash / lockout behavior.
- LAN1/LAN2 NetworkManager path.
- LAN3=end0 and LAN4=end1 mapping.
- Network2 database integration.
- `/etc/network_config.json` persistence.
- RFSoC `setIpConfig` TCP contract.
- TopNetworkDrawer Advanced Mode password gate (separate legacy UI/security path).

## Expected UX

1. User is on RADIO/MAP/DOA/etc.
2. User selects NETWORK SETTINGS.
3. Password popup appears before page navigation.
4. Wrong password or Cancel leaves the user on the existing page.
5. Correct password opens Network Settings.
6. LAN/WiFi/5G changes can be applied without another administrator-password popup.
7. User leaves Network Settings.
8. Re-entering Network Settings prompts for the password again.
