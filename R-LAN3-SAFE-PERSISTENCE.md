# R-LAN3 — Safe JSON Persistence & Concurrent Writer Protection

## Scope

Baseline: `pop-R20.4-RLAN2.1-EXTERNAL-TCP-source.tar.xz`.

This revision hardens persistence of `/etc/network_config.json` without changing the existing LAN UI, LAN mapping, JSON schema, Network2 database schema, LAN3/LAN4 TCP `setIpConfig` protocol, or the proven local LAN1/LAN2 NetworkManager behavior.

## Preserved product contract

- LAN1 -> `enP8p1s0` -> JSON `lan1` -> local NetworkManager
- LAN2 -> `enP1p1s0` -> JSON `lan2` -> local NetworkManager + recorder notification
- LAN3 -> `end0` -> JSON `rfsoc1` -> Network2 DB row 3 -> TCP `setIpConfig`
- LAN4 -> `end1` -> JSON `rfsoc2` -> Network2 DB row 4 -> TCP `setIpConfig`

The existing Network2 update path is preserved. DatabaseDF only emits `updateNetworkDfDevice(end0/end1, ...)` after its SQL UPDATE succeeds, so LAN3/LAN4 TCP dispatch remains gated by successful SQL execution.

## Changes

### 1. Atomic JSON replacement

Direct `QFile(... WriteOnly | Truncate)` persistence is replaced by `QSaveFile`.

The writer disables direct-write fallback. If atomic replacement cannot be completed, the save fails and the previous config file is retained.

### 2. Complete read-modify-write lock

A process-wide `QMutex` now protects the entire read-modify-write transaction, not just the final write.

This prevents in-process LAN/WiFi/5G workers from doing this:

1. LAN reads config A
2. WiFi reads config A
3. LAN writes A+LAN
4. WiFi writes A+WiFi and accidentally removes the LAN change

LAN, WiFi and 5G section updates now use the same `updateNetworkConfigRoot()` helper.

### 3. Malformed file protection

- Missing file: accepted as first boot / empty root.
- Empty or whitespace-only file: accepted as an empty root for legacy compatibility.
- Non-empty malformed JSON: update is rejected; the file is not silently overwritten.

### 4. Local system apply is blocked if persistence fails

For local LAN1/LAN2, if the atomic JSON save fails, the NetworkManager apply is skipped. This avoids changing the local system while the desired persistent configuration remains old.

## Database consistency status

R-LAN3 deliberately does not redesign the asynchronous Network2 DB API. The proven DB path remains intact:

`Mainwindows -> iScreenDF::updateNetworkfromDisplayIndex -> DatabaseDF::updateNetworkfromDisplay -> UPDATE Network2`

For LAN3/LAN4, `DatabaseDF` emits the external update only after SQL UPDATE succeeds.

There is still one transaction-level gap to close in the next phase: `Mainwindows` currently does not receive an explicit SQL success/failure acknowledgement. Therefore a higher-level Apply cannot yet prove that JSON + DB + actual target all committed as one coordinated transaction.

## Next phase

R-LAN4 should add a Network2 update result/transaction ID and move LAN Apply orchestration to an acknowledgement-driven sequence so the UI can distinguish:

- VALIDATED
- JSON_SAVED
- DB_SAVED
- SYSTEM_APPLIED / TCP_DISPATCHED
- VERIFIED / FAILED

This should be done without changing the existing Network2 fields or the LAN3/LAN4 TCP packet format.
