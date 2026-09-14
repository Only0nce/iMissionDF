# R-LAN4B — Legacy RFSoC IP Apply Path Hardening

## Objective

Make LAN3/LAN4 IP configuration use the RFSoC communication path that already existed in the reconciled legacy source, instead of inventing a new transport or protocol.

This revision is deliberately conservative. It does **not** redefine the RFSoC protocol and does **not** repoint the RFSoC management TCP connection to the IP address being configured on `end0`/`end1`.

## Legacy source contract verified

The pre-R-LAN reconciled source already used this path:

1. `DatabaseDF::GetIPDFServerFromDB()` reads `Parameter.ipdfserver`.
2. `iScreenDF::GetIPDFServer()` connects `TcpClientDF` to `<ipdfserver>:5555`.
3. Network Apply updates `Network2`.
4. After successful SQL UPDATE:
   - Network2 row 3 emits `updateNetworkDfDevice("end0", ...)`.
   - Network2 row 4 emits `updateNetworkDfDevice("end1", ...)`.
5. `iScreenDF::onUpdateNetworkDfDevice()` sends the exact legacy JSON packet over the existing RFSoC TCP client.

Exact packet contract retained:

```json
{
  "menuID": "setIpConfig",
  "ifname": "end0",
  "ip": "192.168.99.8",
  "netmask": "255.255.255.0",
  "gateway": "...",
  "dns1": "...",
  "dns2": "..."
}
```

LAN4 is identical except `ifname` is `end1`.

`dhcp` is intentionally **not** added to the packet because the proven legacy packet did not contain it.

## Management TCP vs configured LAN IP

These are different addresses and must stay independent:

- RFSoC management/control TCP: `Parameter.ipdfserver:5555`
- LAN3 target IP: remote RFSoC `end0`
- LAN4 target IP: remote RFSoC `end1`

Example:

- control TCP = `192.168.10.5:5555`
- LAN3/end0 configured IP = `192.168.99.8`

The control socket remains on `192.168.10.5:5555` while it commands `end0` to become `192.168.99.8`.

## Hardening added

### 1. Exact remote-interface guard

The legacy `setIpConfig` path now accepts only `end0` and `end1`. Any other interface is rejected before TCP dispatch.

### 2. Dispatch visibility

`sendRfsocJsonLine()` now returns the existing `TcpClientDF::sendLine()` result so the LAN path can distinguish:

- `DISPATCHED`: QTcpSocket accepted the packet immediately.
- `QUEUED`: RFSoC control socket is currently disconnected, target is known, packet is queued for the existing reconnect path.
- `QUEUED_NO_TARGET`: packet is queued but no RFSoC management endpoint has been loaded yet.
- `REJECTED`: unsupported interface.
- `ERROR`: local RFSoC client/dispatch error.

`DISPATCHED` does **not** mean the RFSoC has applied the address. A true remote apply ACK requires RFSoC-side protocol support and is a later phase.

### 3. QML feedback

The existing Network page receives the dispatch state so LAN3/LAN4 Apply can show whether the RFSoC command was dispatched or queued for reconnect.

## Preserved behavior

- LAN1/LAN2 local NetworkManager behavior unchanged.
- `/etc/network_config.json` persistence unchanged.
- `Network2` database schema and update path unchanged.
- LAN3 = Network2 row 3 = `end0` unchanged.
- LAN4 = Network2 row 4 = `end1` unchanged.
- RFSoC management endpoint source (`Parameter.ipdfserver`) unchanged.
- TCP port `5555` unchanged.
- `setIpConfig` JSON field names unchanged.
- Existing reconnect queue unchanged.

## Bench verification

For LAN3 configure `192.168.99.8/24` and Apply. Expected application log includes:

```text
[iScreenDF][setIpConfig][JSON] = {"gateway":"...","ifname":"end0","ip":"192.168.99.8",...}
[LAN][RFSoC][setIpConfig] DISPATCHED iface= end0 control=<management-ip>:5555 ... target-ip=192.168.99.8
```

If control TCP is down but management endpoint is configured:

```text
[LAN][RFSoC][setIpConfig] QUEUED iface= end0 ...
```

After reconnect, `TcpClientDF::flushPendingWrites()` sends the queued packet.

On RFSoC verify:

```sh
ip addr show end0
```

For LAN4 use `end1`.

## Next phase

Add an RFSoC response/telemetry contract so the controller can distinguish "packet dispatched" from "RFSoC applied successfully", and retrieve real `end0/end1` link/IP state.
