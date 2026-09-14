# R-LAN4B.1 — RFSoC TCP Startup Race Fix

## Problem
Network Settings could show LAN3/LAN4 configured values correctly while the shared RFSoC TCP control channel remained `Disconnected`.

## Root cause found in the legacy startup path
`iScreenDF` started `dbThread` before connecting `DatabaseDF::GetIPDFServer` and before installing all `TcpClientDF` state handlers. `DatabaseDF::init()` immediately calls `GetrfsocParameter()` and `GetIPDFServerFromDB()`. Therefore the one-shot startup signal carrying `Parameter.ipdfserver` could be emitted before its receiver existed. In that case `TcpClientDF::connectToServer(ip, 5555)` was never started.

There was a second diagnostic weakness: `TcpClientDF::logMessage()` had no consumer, so connection attempts and errors were mostly invisible in the main application log.

## Fix
1. Install all DatabaseDF -> iScreenDF connections before starting `dbThread`.
2. Install all TcpClientDF connected/disconnected/error/data handlers before starting `dbThread`.
3. Start `dbThread` only after startup wiring is complete.
4. Make `GetIPDFServer()` apply the TCP target independently of RF parameter-object timing.
5. Log the DB value, target, connect attempt, connected/disconnected state, and socket error.
6. Preserve the legacy `Parameter.ipdfserver:5555` management/control endpoint and exact `setIpConfig` packet for `end0`/`end1`.

## Runtime verification
On the iScan/Jetson, first identify the configured management target from application logs or DB, then run:

```bash
./DIAG-RFSOC-TCP.sh <ipdfserver> 5555
```

Expected application log sequence:

```text
[LAN][RFSoC-TCP] startup wiring complete; starting DB thread
[LAN][RFSoC-TCP] Parameter.ipdfserver from DB = "<host>"
[LAN][RFSoC-TCP] control target from DB = "<host>" 5555
[LAN][RFSoC-TCP] connectToHost <host>:5555
[LAN][RFSoC-TCP] connected <host>:5555
```

If connection fails, the log now includes the real QTcpSocket error such as `Connection refused` or `Network is unreachable`.

On RFSoC verify the control server is listening:

```bash
ss -lntp | grep ':5555'
```

## Compatibility
No JSON schema changes, no Network2 DB schema changes, no LAN1/LAN2 NetworkManager changes, and no RFSoC protocol changes.
