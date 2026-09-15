# NET-VPN2 — VPN Runtime Lifecycle Hardening

Baseline: `NET-UX3` / `NET-VPN1` integrated Network Settings source.

## Goal

Continue VPN development using the proven lifecycle ideas from the older VPN implementation while keeping the current iScanMR10 NetworkManager architecture. The older design treated VPN as a real runtime network interface with connection state and IP address; NET-VPN2 carries that model forward without importing SIP-specific binding/routing code.

## Changes

### NetworkController.cpp

`vpnStatus()` now returns a richer runtime contract:

- `state`: `connected`, `disconnected`, or `failed`
- `activeName`
- `activeUuid`
- `activeType`
- `activeDevice`
- `activeIpv4`
- `activeProfiles[]`
- each profile also exposes `device`, `ipv4`, `active`, and `state`

Runtime DEVICE is read from `nmcli connection show --active`, not assumed from saved profile metadata. Active tunnel IPv4 is resolved from the actual NetworkManager device. This supports OpenVPN-style `tun*`, WireGuard `wg*`, and other NetworkManager-assigned tunnel names without hard-coding `tun0`.

`setVpnConnectionActive()` remains off the GUI thread. A successful `nmcli connection up/down` result is no longer presented as proof that the tunnel is connected/disconnected. Instead it reports that the command completed and immediately performs authoritative status reconciliation.

### VpnPage.qml

The VPN page now has explicit UI lifecycle states:

- `CONNECTING`
- `CONNECTED`
- `DISCONNECTING`
- `DISCONNECTED`
- `FAILED`

The runtime summary shows profile name, VPN type, actual tunnel interface, tunnel IPv4, and active tunnel count.

A 3-second reconciliation timer updates state while the page is visible, allowing changes made outside the application (for example by `nmcli`) to appear automatically. Status requests are guarded so repeated timer ticks do not start overlapping status queries.

Connect/Disconnect now uses a confirmation dialog because bringing a VPN tunnel up/down may change routing or interrupt traffic.

Viewer remains read-only. Admin can connect/disconnect profiles.

## Deliberately not imported from the legacy VPN code

- SIP/PJSUA account creation/destruction
- forced SIP socket binding to `tun0`
- SIP default-account switching
- any assumption that every VPN tunnel is named `tun0`

These belonged to the legacy radio/SIP architecture and are not part of the iScanMR10 Network Settings VPN contract.

## Safety / compatibility

- Existing LAN/WiFi/5G behavior is unchanged.
- VPN secrets, passwords, private keys, and tokens are not exposed to QML or logs.
- NetworkManager remains the source of truth.
- No GUI-thread blocking VPN command was introduced.
- VPN profile import/create/edit remains deferred to a later phase.

## Validation status

Structural verification is provided by `VERIFY-NET-VPN2.sh`.

The container does not provide qmake/qmllint, so a target Qt 5.15 build and Jetson runtime test are still required.
