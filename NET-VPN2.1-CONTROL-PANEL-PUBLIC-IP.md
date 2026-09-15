# NET-VPN2.1 — VPN Control Panel + Public IP

Base: `NET-VPN2-RUNTIME-LIFECYCLE-HARDENING`

## Objective
Make the VPN page operationally complete and obvious at a glance. The primary control area now exposes the required operator information/actions directly:

- Public IP
- Runtime Status
- VPN Enable state
- Refresh
- Connect
- Disconnect
- Selected profile
- Active profile, type, interface, tunnel IPv4 and active tunnel count

## UX architecture
The top card is now the single control surface. The profile list below is selection-oriented, so the operator first selects a NetworkManager VPN/WireGuard profile and then uses the explicit top-level controls.

Viewer remains read-only. Admin may enable/disable VPN and connect/disconnect tunnels.

## VPN Enable semantics
`vpn.enabled` is persisted under `/etc/network_config.json` using the existing atomic QSaveFile + process-wide mutex transaction helper.

Backward compatibility: if `vpn.enabled` does not exist, VPN defaults to enabled so upgrades do not unexpectedly disable existing VPN behavior.

- Enable VPN: persists `vpn.enabled=true`; does not auto-connect a profile.
- Disable VPN: persists `vpn.enabled=false`, then best-effort disconnects all active NetworkManager VPN/WireGuard profiles.
- Backend connect is rejected while VPN is disabled, so the policy is not only visual.
- If runtime disconnect cleanup fails, authoritative status readback exposes the remaining active tunnel instead of claiming it is gone.

## Public IP
Public IP cannot be reliably inferred from local interface addresses when the device is behind NAT. `requestVpnPublicIp()` therefore performs a non-blocking HTTPS request to `https://api.ipify.org` using Qt Network.

- 6 second timeout
- IP syntax validated with `QHostAddress`
- No credentials, VPN keys, SSIDs, device identifiers, or profile metadata are sent
- Public IP is refreshed on page entry, explicit Refresh, and after VPN lifecycle changes
- The 3-second NetworkManager reconciliation timer does **not** query the external public-IP service

If internet/DNS/TLS is unavailable, the UI shows `Unavailable`; VPN runtime control remains independent.

## Runtime transaction model
Connect/Disconnect remains authoritative-readback based:

1. UI enters CONNECTING/DISCONNECTING.
2. `nmcli connection up/down uuid ...` runs off the GUI thread.
3. Backend requests NetworkManager status again.
4. UI becomes CONNECTED/DISCONNECTED only from that readback.
5. Public IP is refreshed after the readback.

## Files changed from NET-VPN2
- `NetworkController.h`
- `NetworkController.cpp`
- `VpnPage.qml`

No LAN, WiFi, 5G, RFSoC, database, spectrum/waterfall, or access-popup implementation is modified in this revision.

## Build validation note
The container does not have the project Qt/qmake toolchain at its original target path, so the revision is structurally verified here and must be compiled/tested on the normal Qt 5.15.2 Jetson build host.
