# NET-ENDPOINTS1.10 — DB-Authoritative DF Endpoint Presentation

Date: 2026-09-17

## Problem
The Network Settings > Endpoints > DF Server IP field could return to an old value (observed as `192.168.10.28`) even after another endpoint had been successfully saved.

## Root cause
`Parameter.id=1.ipdfserver` is the authoritative DF control endpoint, but `TopNetworkDrawer.qml` still treated the legacy per-NIC `Network2.krakenserver` value as if it were the same setting:

- `refillFields()` copied `g(selectedNic, "server")` into `serverField`.
- every `networkRowUpdated` copied `row.krakenserver` into `serverField`.
- typing into `serverField` wrote the draft back into the legacy Network2/runtime cache before the endpoint transaction had committed.

A stale `Network2.krakenserver` row could therefore arrive after the correct `updateServeripDfserver()` signal and visually overwrite the saved `Parameter.ipdfserver` value.

## Fix

### 1. One source of truth
`Parameter.id=1.ipdfserver` is now the only source allowed to populate the DF Server IP field.

`TopNetworkDrawer.qml` no longer reads DF Server IP from `Network2.krakenserver`, and draft edits stay local until the existing connect -> verify -> DB commit transaction succeeds.

### 2. Fresh DB snapshot whenever the network drawer is opened
A new `requestDfServerEndpointSnapshot()` path reads `Parameter.id=1.ipdfserver` in the DB thread and returns it to `iScreenDF`, which reconciles the runtime committed cache and emits `updateServeripDfserver()` to QML.

Opening Network Settings therefore shows what is actually stored in the database instead of an old in-memory Network2 value.

### 3. Legacy database mirror is kept consistent
For backward compatibility only, `Network2.krakenserver` is mirrored from the authoritative `Parameter.ipdfserver`:

- on startup DB restore;
- when the Endpoints page requests a DB snapshot;
- inside the same DB transaction that commits a newly verified endpoint.

The mirror is verified with a read-back count. It is never used as the source of truth.

### 4. Remove compiled-in endpoint fallback
`Parameter::m_ipdfServer` no longer defaults to a hard-coded legacy IP. If DB state is missing, the application will not silently resurrect an unrelated old endpoint.

## Expected result
If the database contains:

```sql
SELECT id, ipdfserver FROM Parameter WHERE id=1;
```

and returns, for example, `192.168.99.8`, opening Network Settings > Endpoints must show exactly `192.168.99.8` regardless of which LAN row is selected.

After a successful Apply, both of these should agree:

```sql
SELECT id, ipdfserver FROM Parameter WHERE id=1;
SELECT id, krakenserver FROM Network2 ORDER BY id;
```

`Parameter.ipdfserver` remains authoritative; the second query is compatibility verification only.
