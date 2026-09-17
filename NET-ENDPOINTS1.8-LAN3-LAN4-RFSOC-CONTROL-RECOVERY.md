# NET-ENDPOINTS1.8 — LAN3/LAN4 RFSoC Control Recovery

Date: 2026-09-16
Baseline: NET-ENDPOINTS1.7

## Scope

This revision applies the RFSoC control-link recovery to **both external RFSoC ports**:

- LAN3 -> `end0` -> `Network2.id=3` -> JSON `rfsoc1`
- LAN4 -> `end1` -> `Network2.id=4` -> JSON `rfsoc2`

Both ports continue to use the same existing RFSoC management/control TCP client selected by **DF Server IP** (`Parameter.ipdfserver`, port 5555). LAN3/LAN4 do not become TCP endpoints themselves.

The architectural split from NET-ENDPOINTS1.7 is preserved:

- LAN3/LAN4 = CONFIG remote RFSoC interfaces (`end0` / `end1`)
- DF Server IP = CONNECT target for the shared RFSoC/DF TCP control channel
- DoA Viewer = DISPLAY live data relayed from the connected RFSoC/DF stream

## Root cause addressed

The older proven RFSoC client kept a reconnect timer active during `connectToHost()` and queued an immediate retry after socket error/disconnect.

R20.4-STAB2 intentionally replaced that behavior with a lifecycle-safe single-shot exponential backoff. The safety change was correct, but two useful recovery properties were lost:

1. no immediate first recovery attempt after a disconnect/error;
2. no watchdog remained armed while `QTcpSocket` stayed in `ConnectingState` without promptly producing an error signal.

That can leave the shared RFSoC control channel shown as disconnected for LAN3 and LAN4 even when the configured DF Server IP is correct.

## Changes

### 1. Immediate retry once per outage

`TcpClientDF` now allows one queued immediate reconnect after the first error/disconnect of an outage.

A guard (`m_immediateRetryConsumed`) prevents the old `ConnectionRefused -> retry -> error -> retry` tight loop.

After the one immediate retry is consumed, the existing bounded STAB2 backoff remains authoritative:

`10 s -> 20 s -> 40 s -> 60 s cap`

The guard resets only after a successful connection or an explicit new `connectToServer()` target request.

### 2. Bounded connect watchdog restored

A single-shot reconnect watchdog is armed whenever `connectToHost()` is issued:

- initial DF Server connection;
- every timed reconnect attempt.

If the socket becomes connected, `onConnected()` stops the watchdog immediately.

If the socket remains stuck in `ConnectingState`, the watchdog aborts that attempt and starts a fresh one. This restores the useful behavior of the older proven implementation without returning to a permanently repeating reconnect timer.

### 3. Queued LAN3/LAN4 command nudges the control link

When LAN3/end0 or LAN4/end1 `setIpConfig` is queued while the shared RFSoC control link is down, `sendLine()` requests the same guarded recovery path.

This does **not** create a second socket and does **not** change the DF Server IP. It only nudges the already configured management target.

### 4. LAN3 and LAN4 status text clarified

Both external rows now report the shared link explicitly as:

- `RFSoC Control Connected`
- `RFSoC Control Disconnected`

This avoids implying that the displayed state is the physical carrier state of `end0` or `end1`.

The configured LAN3/LAN4 IPv4 remains the remote interface configuration value. The control target remains the independent DF Server IP.

### 5. Disconnected color fix

The previous QML color test checked the substring `connected` before `disconnect`. Because `disconnected` contains `connected`, a disconnected LAN3/LAN4 row could be rendered with the connected/accent color.

Negative states are now checked first.

## Preserved contracts

No change to:

- LAN3 mapping: index 2 / Network2 row 3 / `end0` / `rfsoc1`
- LAN4 mapping: index 3 / Network2 row 4 / `end1` / `rfsoc2`
- RFSoC packet fields: `menuID`, `ifname`, `ip`, `netmask`, `gateway`, `dns1`, `dns2`
- `Parameter.ipdfserver` ownership
- DF Server IP persistence and connection semantics
- TCP port 5555
- DoA bridge `127.0.0.1:9000`
- STAB2 lifecycle guards (`m_shuttingDown`, `m_userDisconnect`, single-shot bounded backoff)
- LAN1/LAN2, WiFi, 5G, VPN, audio, recorder, spectrum/waterfall

## Expected runtime behavior

When the RFSoC control target is reachable:

```text
[LAN][RFSoC-TCP] connectToHost 192.168.x.x:5555
[LAN][RFSoC-TCP] reconnect scheduled ... reason= initial-connect-watchdog delay_ms= 10000
[LAN][RFSoC-TCP] connected 192.168.x.x:5555
```

If the first attempt fails:

```text
[LAN][RFSoC-TCP] socket error ...
[LAN][RFSoC-TCP] immediate reconnect queued ... reason= socket-error
```

If that retry still fails, recovery continues only through the bounded timer.

For either external Apply:

```text
LAN3 -> ifname=end0
LAN4 -> ifname=end1
```

If disconnected, the exact `setIpConfig` packet remains queued and the existing target is nudged for recovery.

## Bench validation

1. Set a valid DF Server IP and verify TCP 5555 is reachable from Jetson.
2. Start iScan and verify `RFSoC Control Connected` appears for **both LAN3 and LAN4**.
3. Stop the RFSoC server or disconnect the cable; both rows must show `RFSoC Control Disconnected` with warning color.
4. Restore the server/cable. The client should perform one immediate retry and then bounded retries until connected.
5. Apply LAN3 and verify JSON contains `ifname=end0`.
6. Apply LAN4 and verify JSON contains `ifname=end1`.
7. While disconnected, Apply LAN3/LAN4 and verify `QUEUED`, then restore the control connection and verify the queued packet is flushed.
8. Verify changing LAN3/LAN4 does not mutate `Parameter.ipdfserver`.
9. Verify DoA live data resumes after the shared control link reconnects.

## Build status

Structural/static verification is performed in this environment. Target Qt 5.15.2 / Jetson compile and hardware bench validation are still required.
