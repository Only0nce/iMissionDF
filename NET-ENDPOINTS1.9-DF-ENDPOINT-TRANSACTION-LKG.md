# NET-ENDPOINTS1.9 — DF Endpoint Transaction + Last Known Good

## Scope

This revision hardens the DF/RFSoC control endpoint change path without changing the proven LAN3/LAN4 `setIpConfig` JSON contract.

The previous Apply path changed `Parameter.ipdfserver` and queued the database update before proving that the new `:5555` endpoint could be reached. A bad candidate therefore replaced the only known endpoint and the reconnect loop kept targeting the bad address.

## New ownership model

The DF endpoint now has distinct states:

- **draft** — QML text only.
- **candidate** — temporary target while Apply is being tested.
- **committed** — database/runtime endpoint that Reconnect is allowed to use.
- **last-known-good** — most recent committed endpoint that established TCP successfully.

`Apply` performs:

1. validate candidate IPv4;
2. connect candidate on TCP/5555 without changing `Parameter.ipdfserver` or DB;
3. after `connected()`, persist `Parameter.ipdfserver` in the DB thread;
4. read the row back in the same transaction;
5. commit only when stored value exactly matches the candidate;
6. update runtime Parameter, QML and GPSD only after DB verification;
7. on connect/DB failure, reconnect the previous committed/last-known-good endpoint.

`Reconnect` never reads the draft TextField. It always reconnects committed backend state.

## Race protection

Every Apply/Reconnect owns a monotonically increasing generation. Delayed DB callbacks from an older Apply are ignored after a later action. Candidate connection timeout is 3.5 seconds.

`TcpClientDF` now uses a single-shot reconnect watchdog with one immediate retry per outage and bounded 10/20/40/60 second backoff. A target switch suppresses only the intentional disconnect callback from the old socket.

## Database semantics

`DatabaseDF::persistDfServerEndpoint()` uses UPDATE + SELECT read-back in one DB transaction. `numRowsAffected()==0` is accepted when the read-back already equals the requested value; therefore a no-op MySQL update is no longer misclassified as failure.

## Preserved contracts

- LAN3 -> `Network2.id=3` -> `end0` -> legacy `setIpConfig`.
- LAN4 -> `Network2.id=4` -> `end1` -> legacy `setIpConfig`.
- RFSoC control port remains TCP/5555.
- DoA local bridge remains separate.
- RFSoC parameter refresh no longer recreates `Parameter`, so `ipdfserver` cannot fall back to the struct default during a partial refresh.

## Expected bench log

Successful candidate:

```text
[DF-ENDPOINT] state= CONNECTING candidate=192.168.x.x committed=192.168.y.y ...
[LAN][RFSoC-TCP] connected 192.168.x.x:5555
[DF-ENDPOINT] state= TCP_OK_DB_COMMIT ...
[DF-ENDPOINT-DB] requested=... stored=... result=COMMIT_OK
[DF-ENDPOINT] state= COMMITTED ...
```

Bad candidate:

```text
[DF-ENDPOINT] state= CONNECTING ...
[LAN][RFSoC-TCP] socket error ...
[DF-ENDPOINT] state= ROLLBACK ... rollback=192.168.y.y
[LAN][RFSoC-TCP] connected 192.168.y.y:5555
[DF-ENDPOINT] state= ROLLED_BACK ...
```

## Bench acceptance

1. Working A -> Apply valid B -> B connects and DB becomes B.
2. Restart -> B is loaded from DB and connects.
3. Working A -> Apply invalid C -> DB remains A and control returns to A.
4. While C is failing, Reconnect -> draft C is ignored and committed A is used.
5. Apply B/C/B quickly -> stale callbacks cannot replace the newest generation.
6. `SELECT ipdfserver FROM Parameter WHERE id=1` must match the last `COMMITTED` endpoint.
7. LAN3/LAN4 `setIpConfig` packets remain byte/field compatible with the existing server contract.
