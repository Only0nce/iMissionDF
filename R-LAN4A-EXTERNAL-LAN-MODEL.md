# R-LAN4A — External RFSoC LAN Model

## Scope

This revision changes only the read/status model and presentation for LAN3/LAN4.
LAN1/LAN2 keep the proven local NetworkManager behavior.

### Fixed product mapping

- LAN1 -> `enP8p1s0` -> local Jetson/NetworkManager
- LAN2 -> `enP1p1s0` -> local Jetson/NetworkManager
- LAN3 -> `end0` -> remote RFSoC interface configured over TCP
- LAN4 -> `end1` -> remote RFSoC interface configured over TCP

The existing JSON keys remain unchanged: `lan1`, `lan2`, `rfsoc1`, `rfsoc2`.
The existing Network2 database integration and `setIpConfig` TCP path remain unchanged.

## Root cause

`NetworkController::lanObjectToMap()` previously called the local `nmcli`/sysfs live-state path for every interface name. `end0` and `end1` exist on the remote RFSoC, not on the local Jetson, so the Network Settings page displayed meaningless local values such as:

- Link: `-`
- MAC: `-`
- status text leaking the implementation detail `nmcli skipped for end* iface`

## R-LAN4A behavior

### NetworkController

`end0`/`end1` are now tagged as:

- `external = true`
- `portType = external-rfsoc`
- `controlTransport = tcp`
- `controlScope = remote`

They do not call local `parseDeviceShow()`.
Their configured IPv4/gateway/DNS values continue to come from the saved network configuration.

`queryDhcpInfo(end0/end1)` also avoids local nmcli probing and returns the remote configured model.

### RFSoC TCP control status

`TcpClientDF` exposes read-only connection state/target data. `iScreenDF` relays connected/disconnected state to `Mainwindows`, and `Mainwindows::externalLanStatus()` exposes it to QML.

Important: this status means **the RFSoC TCP control channel is connected**. It does **not** mean the physical carrier of remote `end0` or `end1` is UP. Physical link state requires explicit RFSoC telemetry in a later phase.

### Setting.qml

For LAN3/LAN4 the page now shows:

- `RFSoC TCP` instead of local `Link`
- TCP `Connected` / `Disconnected`
- RFSoC control endpoint when known
- `Port Type: External RFSoC LAN`
- `Control: TCP ...`

The LAN3/LAN4 list status also reflects the shared RFSoC TCP control channel.
The old `DHCP Info` action becomes `RFSoC Status` for external LANs and refreshes only the TCP control status.

## Apply path intentionally unchanged

LAN3/LAN4 still use the proven mutation path:

`Setting.qml -> Mainwindows::applyLanSettings() -> Network2 DB -> updateNetworkDfDevice(end0/end1) -> iScreenDF::onUpdateNetworkDfDevice() -> sendRfsocJsonLine(setIpConfig)`

JSON persistence remains active through `NetworkController`.

## Not included

- No physical `end0/end1` UP/DOWN telemetry yet.
- No new RFSoC TCP protocol.
- No change to LAN3/LAN4 DHCP semantics.
- No change to LAN1/LAN2 NetworkManager apply behavior.
- No Wi-Fi or 5G changes.

## Bench acceptance

1. LAN1/LAN2 still show local link/speed/duplex and apply local IP normally.
2. LAN3/LAN4 never show local `nmcli skipped for end* iface` as UI status.
3. LAN3/LAN4 show RFSoC TCP Connected when the existing control socket is connected.
4. Disconnect/reconnect of the RFSoC TCP socket updates LAN3/LAN4 status without reopening the page.
5. LAN3 Apply still updates JSON + Network2 and sends existing `setIpConfig` with `ifname=end0`.
6. LAN4 Apply still updates JSON + Network2 and sends existing `setIpConfig` with `ifname=end1`.
