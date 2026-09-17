# NET-ENDPOINTS2.0 — LAN3/LAN4 TCP-Authoritative Remote Config

Date: 2026-09-16
Baseline: NET-ENDPOINTS1.9

## Goal

Make LAN3/end0 and LAN4/end1 behave as remote RFSoC configuration views only:

- Connected/Disconnected comes only from the real RFSoC `QTcpSocket` control session.
- LAN3 sends the existing `setIpConfig` JSON for `end0`.
- LAN4 sends the existing `setIpConfig` JSON for `end1`.
- A remote IP-change command is never queued for delayed delivery.
- If TCP is disconnected, Apply is rejected before DB/config persistence.
- DF Server IP remains the independent owner of the RFSoC control target on TCP port 5555.

## Important source review result

`NetworkController::applyNetworkConfig()` already has an `end*` guard that skips `nmcli` for end0/end1. The call is therefore retained only to preserve the existing `/etc/network_config.json` restart/display mirror. It does not configure a local Linux interface for LAN3/LAN4.

Remote execution remains exclusively:

```
LAN3/end0 or LAN4/end1
        -> Network2 persistence
        -> updateNetworkDfDevice
        -> setIpConfig JSON
        -> already-connected RFSoC TCP control session
```

## TCP status ownership

Authoritative state:

```
QTcpSocket::connected
QTcpSocket::disconnected
QTcpSocket::errorOccurred + actual socket state
```

`Mainwindows::externalLanStatus()` reads `iScreenDF::isRfsocControlConnected()`, which reads `TcpClientDF::isConnected()`, which is exactly:

```cpp
m_socket.state() == QAbstractSocket::ConnectedState
```

No IP address, saved config, Network2 row, or QML state is allowed to manufacture Connected.

## Unsolicited heartbeat

The RFSoC protocol has no documented `menuID:"ping"` heartbeat contract. NET-ENDPOINTS2.0 therefore disables the synthetic heartbeat packet by default. Reconnect handling remains available through the existing QTcpSocket error/disconnect path and bounded retry logic.

## LAN3/LAN4 command safety

A new `TcpClientDF::sendLineIfConnected()` path is used only for remote network mutation. Unlike the generic sender, it never queues traffic while disconnected.

Disconnected Apply result:

```
CONTROL_DISCONNECTED
command not sent
```

Connected Apply result after socket write acceptance:

```
DISPATCHED
```

Generic DoA/control messages retain the existing deferred queue; only `setIpConfig` is prohibited from entering it.

## Unchanged protocol

LAN3:

```json
{"menuID":"setIpConfig","ifname":"end0","ip":"...","netmask":"...","gateway":"...","dns1":"...","dns2":"..."}
```

LAN4:

```json
{"menuID":"setIpConfig","ifname":"end1","ip":"...","netmask":"...","gateway":"...","dns1":"...","dns2":"..."}
```

No field was added, renamed, or removed.

## Bench validation

1. Set DF Server IP to the actual RFSoC server address.
2. Verify TCP port 5555 externally.
3. Start iScan and confirm LAN3/LAN4 show RFSoC Control Connected only after the TCP session connects.
4. Stop the RFSoC server. Both LAN3/LAN4 must show Disconnected.
5. While disconnected, Apply LAN3 and LAN4. No `setIpConfig` packet must be queued or sent later.
6. Restart RFSoC server and wait for TCP reconnection.
7. Apply LAN3 and verify `ifname=end0` JSON is dispatched.
8. Apply LAN4 and verify `ifname=end1` JSON is dispatched.
9. Confirm LAN1/LAN2 local NetworkManager behavior is unchanged.
10. Confirm DoA live-data forwarding still uses the same DF Server IP TCP session.


## Legacy caller guard

`iScreenDF::updateNetworkfromDisplayIndex()` also rejects LAN3/LAN4 changes while the TCP control session is disconnected. This keeps the temporary legacy TopNetworkDrawer from bypassing the same safety rule used by the current Network Settings page.
