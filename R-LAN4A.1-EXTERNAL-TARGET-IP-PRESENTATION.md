# R-LAN4A.1 External Target IP Presentation

## Goal
LAN3/LAN4 represent remote RFSoC interfaces (`end0`/`end1`). Their user-facing IP must be the configured interface IPv4, not the RFSoC management TCP endpoint.

## Contract
- LAN3 -> `end0` -> display configured IPv4 (example `192.168.99.8`).
- LAN4 -> `end1` -> display configured IPv4.
- The existing RFSoC management/control socket host/port remains internal and unchanged.
- Existing `setIpConfig` TCP write path remains unchanged.

## Why the control endpoint stays separate
The control socket is the stable management path used to change the remote interface IP. Re-pointing the control socket to the address being changed could disconnect control during Apply and prevents configuring an interface that does not yet own the requested address.

## UI changes
- RFSoC TCP card subtext: `<iface> · <configured IPv4>`.
- Device Information shows `Configured IPv4` using the configured LAN value.
- Management host/port is no longer rendered as the LAN3/LAN4 interface IP.

## Non-changes
- LAN1/LAN2 NetworkManager flow unchanged.
- JSON schema unchanged.
- Network2 database update unchanged.
- LAN3/LAN4 `setIpConfig` protocol unchanged.
- RFSoC control host/port and reconnection behavior unchanged.
