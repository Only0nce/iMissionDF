# LAN Phase A — UI Compatibility Restore

## Scope

This phase restores the proven LAN side effects that were bypassed when the redesigned `Setting.qml` started calling `NetworkController` directly.

No Wi-Fi or 5G behavior is changed in this phase.

## Preserved LAN contract

| UI port | Interface | JSON key | Runtime/integration path |
|---|---|---|---|
| LAN1 | `enP8p1s0` | `lan1` | JSON + local NetworkManager |
| LAN2 | `enP1p1s0` | `lan2` | JSON + local NetworkManager + recorder notification |
| LAN3 | `end0` | `rfsoc1` | JSON + Network2 DB + existing `setIpConfig` external-device path |
| LAN4 | `end1` | `rfsoc2` | JSON + Network2 DB + existing `setIpConfig` external-device path |

`end0/end1` intentionally keep the existing `NetworkController` behavior that skips local `nmcli`. Whether the Jetson side also needs local IP configuration must be proven on hardware before changing this contract.

## Changes

1. `Setting.qml` sends production LAN mutations through `Mainwindows::applyLanSettings()` instead of directly mutating `NetworkController`.
2. `Mainwindows` receives the existing `iScreenDF` object and restores `updateNetworkfromDisplayIndex()` for Network2/RFSoC integration.
3. LAN2 keeps the recorder `Networks` notification.
4. `NetworkController` accepts the legacy DHCP strings `on/1/true/enabled` in addition to `dhcp/auto/automatic`.
5. The old synchronous `loadAllLanConfig()` readback immediately after apply is removed from `setNetworkFormDisplay()`.
6. Full-LAN WebSocket snapshots are generated off the GUI/audio thread after the background apply stage completes.

## Runtime verification

### LAN1 / LAN2

Verify JSON and NetworkManager:

```bash
cat /etc/network_config.json
nmcli device status
nmcli connection show
nmcli device show enP8p1s0
nmcli device show enP1p1s0
```

LAN2 must additionally produce the existing recorder notification.

### LAN3 / LAN4

Verify JSON keys `rfsoc1/rfsoc2`, Network2 DB update, and logs:

```text
[iScreenDF][setIpConfig][JSON]
```

The external device must receive `ifname=end0` or `ifname=end1` and the configured IP/netmask/gateway/DNS values.

## Deferred to the next phase

- Strict IPv4/gateway/DNS validation.
- Contiguous-netmask validation and removal of the QML fallback-to-/24 behavior.
- Atomic JSON writes and cross-thread/process write locking.
- Clear desired-state versus runtime-state reporting.
- DHCP cleanup (`ipv4.ignore-auto-dns=no`) and NetworkManager read-back verification.
- Web `applyNetwork` unification with the same LAN coordinator.
- LAN3/LAN4 DHCP protocol audit and external acknowledgement/verification.
- Hardware-driven LAN1/LAN2 inventory reconciliation.
