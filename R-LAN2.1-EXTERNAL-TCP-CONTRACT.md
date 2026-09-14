# R-LAN2.1 External TCP Contract Lock

Scope: LAN3/LAN4 only. LAN1/LAN2 behavior is unchanged.

## Required Apply path

- LAN3 (`index=2`) -> Network2 row 3 -> `updateNetworkDfDevice("end0", ...)` -> `iScreenDF::onUpdateNetworkDfDevice()` -> `sendRfsocJsonLine(..., true)`.
- LAN4 (`index=3`) -> Network2 row 4 -> `updateNetworkDfDevice("end1", ...)` -> `iScreenDF::onUpdateNetworkDfDevice()` -> `sendRfsocJsonLine(..., true)`.

The TCP JSON contract remains the proven legacy format:

```json
{
  "menuID": "setIpConfig",
  "ifname": "end0",
  "ip": "192.168.1.10",
  "netmask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "8.8.8.8",
  "dns2": "1.1.1.1"
}
```

LAN4 is identical except `ifname` is `end1`.

## Safety change

`Mainwindows::applyLanSettings()` now rejects LAN3/LAN4 Apply if the existing `iScreenDF` integration backend is unavailable. This avoids reporting an accepted Apply while no TCP command can be dispatched.

No new TCP protocol was introduced. The existing `setIpConfig` path is preserved.
