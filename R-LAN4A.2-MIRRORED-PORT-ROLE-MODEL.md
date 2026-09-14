# R-LAN4A.2 — Mirrored Port Role Model

## Goal

Represent the four product LAN ports using two independent dimensions:

1. **Port role** — what the Ethernet port is used for.
2. **Execution scope** — which device owns/applies the configuration.

This prevents the remote RFSoC transport from being mistaken for the functional role of LAN3/LAN4.

## Frozen product mapping

| Port | Interface | Port role | Execution scope | Apply executor |
|---|---|---|---|---|
| LAN1 | `enP8p1s0` | External Device (`device`) | Local Device (`local`) | NetworkManager |
| LAN2 | `enP1p1s0` | Network (`network`) | Local Device (`local`) | NetworkManager |
| LAN3 | `end0` | External Device (`device`) | Remote RFSoC (`remote`) | TCP `setIpConfig` |
| LAN4 | `end1` | Network (`network`) | Remote RFSoC (`remote`) | TCP `setIpConfig` |

Therefore LAN1 mirrors LAN3 by role, and LAN2 mirrors LAN4 by role. The only architectural difference is execution scope.

## Backend model

`NetworkController::lanObjectToMap()` now publishes:

- `portRole`: `device` / `network`
- `portRoleLabel`: `External Device` / `Network`
- `executionScope`: `local` / `remote`
- `executionScopeLabel`: `Local Device` / `Remote RFSoC`

Legacy compatibility metadata is retained:

- `external`
- `portType`
- `controlTransport`
- `controlScope`

The product role is derived from the stable JSON LAN key (`lan1`, `lan2`, `rfsoc1`, `rfsoc2`) before falling back to a display index. This prevents a missing/reordered entry from changing the role accidentally.

## UI behavior

`Setting.qml` now treats role and scope separately:

- LAN list rows show `Device` or `Network` independent of local/remote ownership.
- Selected interface header shows, for example:
  - `External Device · Local Device`
  - `Network · Local Device`
  - `External Device · Remote RFSoC`
  - `Network · Remote RFSoC`
- LAN3/LAN4 still show the configured target IP (`end0` / `end1`) rather than the management TCP endpoint.
- LAN3/LAN4 TCP status is explicitly control-channel status and is not presented as physical Ethernet carrier state.

## Apply behavior intentionally unchanged

R-LAN4A.2 does **not** alter the proven mutation paths:

- LAN1/LAN2 continue through local NetworkManager.
- LAN3/LAN4 continue through the existing `Network2` database path and RFSoC TCP `setIpConfig` path.
- `/etc/network_config.json` remains the same schema.
- Existing `Network2` schema is unchanged.
- Existing TCP JSON schema is unchanged.
- LAN2 recorder side effect remains unchanged.

## Next phase

Recommended next step: **R-LAN4B — Apply Transaction Acknowledgement & Ordering**.

The goal is to report each Apply stage explicitly (`JSON`, `Network2 DB`, executor dispatch) and avoid reporting success before the authoritative stage completes. After that, add RFSoC status telemetry so LAN3/LAN4 can display real remote `end0/end1` physical link state.
