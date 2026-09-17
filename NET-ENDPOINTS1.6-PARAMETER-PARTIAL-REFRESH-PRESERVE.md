# NET-ENDPOINTS1.6 — Parameter Partial Refresh Preserve

## Purpose

Fix the DF Server IP runtime reset observed after startup even though
`Parameter.id=1.ipdfserver` is correctly persisted in MariaDB/MySQL.

This revision does **not** add a new database query, table, signal, endpoint
API, or persistence path. It fixes ownership of the existing shared
`iScreenDF::Parameter` runtime object.

## Root cause

`DatabaseDF::GetrfsocParameter()` is a partial query of `Parameter.id=1`.
Its payload contains RF/DoA/global-offset fields, but it does not contain
`ipdfserver` (and also does not own runtime-only `m_scannerAttDb`).

The receiver `iScreenDF::GetrfsocParameter()` previously executed:

```cpp
qDeleteAll(m_parameter);
m_parameter.clear();
Parameter *p = new Parameter();
```

Every partial refresh therefore destroyed the full shared runtime object.
Fields not present in that refresh returned to their struct defaults. For
DF Server IP this meant:

```text
DB Parameter.ipdfserver = 192.168.99.8
        ↓ startup load
runtime m_ipdfServer     = 192.168.99.8
        ↓ later GetrfsocParameter refresh
new Parameter() default  = 192.168.10.78
        ↓ Endpoints replay
UI DF Server IP          = 192.168.10.78
```

## Change

`iScreenDF::GetrfsocParameter()` now:

1. creates `Parameter` only when no runtime object exists;
2. otherwise reuses `m_parameter.first()`;
3. updates only the fields present in the existing GetrfsocParameter payload;
4. does not assign `m_ipdfServer`;
5. therefore preserves `m_ipdfServer` loaded by `GetIPDFServerFromDB()` or
   changed by the existing `connectToDFserver()` path.

The same preservation also prevents an RF/DoA refresh from resetting
`m_scannerAttDb`, whose owner is a separate runtime control path.

## Existing ownership retained

DF Server IP remains owned by the existing paths:

```text
DatabaseDF::GetIPDFServerFromDB()
  -> DatabaseDF::GetIPDFServer(ip)
  -> iScreenDF::GetIPDFServer(ip)
  -> Parameter.m_ipdfServer
```

and for user Apply:

```text
ServiceEndpointsPage
  -> iScreenDF::connectToDFserver(ip)
  -> Parameter.m_ipdfServer
  -> DatabaseDF::UpdateParameterField("ipdfserver", ...)
```

The late-created Endpoints page continues to reuse the existing
`iScreenDF::updateIPServerDF()` replay added in NET-ENDPOINTS1.5.

## Diagnostics

The partial refresh now emits debug traces:

```text
[PARAM][RFSOC-REFRESH] reused ipdfserver_before= "192.168.99.8"
[PARAM][RFSOC-REFRESH] ipdfserver_after= "192.168.99.8"
```

On first creation the first line reports `created`. After the DB endpoint has
loaded, subsequent RF/DoA refreshes must report the same DF Server IP before
and after.

## Files changed

Product code:

- `iScreenDF/functionTcpServer.cpp`

No QML, database schema, SQL query, network protocol, recorder, audio, VPN,
WiFi/5G, or RFSoC IP-configuration packet format is changed.

## Target validation

1. Set/confirm `Parameter.id=1.ipdfserver` in DB, e.g. `192.168.99.8`.
2. Restart the application.
3. Open Network Settings -> Endpoints without pressing Apply.
4. Confirm the DF Server IP field is `192.168.99.8`.
5. Trigger `SideLocal` / RF parameter refresh repeatedly by navigating through
   affected pages.
6. Return to Endpoints and confirm the value remains `192.168.99.8`.
7. Apply a different address, verify the DB value, restart, and repeat.

Expected invariant:

```text
Parameter.ipdfserver in DB
        == runtime Parameter.m_ipdfServer
        == Endpoints DF Server IP
```

after startup and after every partial RF/DoA parameter refresh.
