# R-LAN2 — LAN Validation & Transaction Boundary

Baseline: `pop-R20.4-RECONCILED-LATEST-20260914-source.tar(1).xz`

## Scope

This revision hardens only the LAN apply entry point used by the redesigned
`Setting.qml`. It does not change Wi-Fi, 5G, LAN JSON schema, LAN interface
mapping, Network2 database fields, recorder protocol, or the external
`setIpConfig` protocol.

## Preserved contracts

- LAN1 = `enP8p1s0` -> JSON `lan1`
- LAN2 = `enP1p1s0` -> JSON `lan2`
- LAN3 = `end0` -> JSON `rfsoc1`
- LAN4 = `end1` -> JSON `rfsoc2`
- LAN1/LAN2 keep the local NetworkManager path.
- LAN2 keeps the recorder notification.
- LAN3/LAN4 keep Network2 + external `setIpConfig` integration.
- LAN3/LAN4 local `nmcli` behavior is unchanged.

## What changed

### 1. C++ is the validation authority

`Mainwindows::validateLanSettings()` provides a read-only preflight result to
QML. `Mainwindows::applyLanSettings()` invokes the same validator again before
any mutation. This means bypassing QML does not bypass LAN validation.

Validation rejects:

- unknown LAN index
- unknown DHCP/static mode token
- malformed static IPv4/CIDR
- malformed or non-contiguous IPv4 netmask
- mismatch between netmask and CIDR prefix
- malformed gateway (when provided)
- malformed primary/secondary DNS (when provided)

DHCP does not require a static IPv4 address or netmask. This intentionally
preserves the current DHCP workflow; desired/runtime DHCP state cleanup belongs
to a later LAN phase.

### 2. Side effects occur only after validation

The apply sequence is now:

```
QML preflight (read-only)
        |
        v
C++ apply validation (authoritative)
        |
        +-- invalid -> return false; NO side effects
        |
        v
Network2 / external integration
        |
        v
NetworkController JSON/system apply
        |
        v
LAN2 recorder notification (LAN2 only)
```

No Network2 update, JSON/system mutation, external `setIpConfig`, or LAN2
recorder notification is issued by `applyLanSettings()` when validation fails.

### 3. Netmask conversion is strict in QML

`Setting.qml::netmaskToCidr()` no longer silently converts malformed netmasks
to `/24` or merely counts set bits. It accepts only contiguous masks.

Example:

- `255.255.255.0` -> `/24`
- `255.255.0.0` -> `/16`
- `255.0.255.0` -> invalid

C++ independently validates the mask again.

## Deliberately deferred

The following are not part of R-LAN2 and must not be inferred as fixed here:

- atomic/locked `/etc/network_config.json` writes
- DHCP lease vs desired-state cleanup in JSON
- NetworkManager `ipv4.ignore-auto-dns` reset/read-back verification
- hardware/JSON LAN inventory reconciliation
- LAN3/LAN4 external ACK and DHCP protocol verification
- Web `applyNetwork` unification with the QML coordinator
- transaction UI states such as APPLYING/VERIFYING/WAITING_DHCP

These are planned as subsequent LAN revisions so regressions remain easy to
localize.
