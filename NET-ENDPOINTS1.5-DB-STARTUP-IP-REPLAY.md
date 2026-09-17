# NET-ENDPOINTS1.5 — DB Startup IP Replay

## Purpose

Ensure the Endpoints page displays the DF Server IP already loaded from `Parameter.id=1.ipdfserver` when the application starts, instead of falling back to the legacy `Network2.krakenserver` value when the page is created later.

## Existing path reused

The application already reads the authoritative DF endpoint at startup:

`DatabaseDF::init()` -> `GetIPDFServerFromDB()` -> `DatabaseDF::GetIPDFServer(ip)` -> `iScreenDF::GetIPDFServer(ip)` -> `m_parameter.first()->m_ipdfServer`.

`iScreenDF::updateIPServerDF()` already exists and emits the existing `updateServeripDfserver(ip)` signal from that runtime value. NET-ENDPOINTS1.5 does not add a new DB query, persistence owner, signal, or endpoint mutation API.

## Change

1. Expose the existing `updateIPServerDF()` method through the existing `public slots` section so QML can invoke it.
2. When `ServiceEndpointsPage.qml` is created, call `updateIPServerDF()` before the legacy Network2 refresh.
3. The existing `onUpdateServeripDfserver(ip)` handler stores the authoritative IP in `appliedDfServerIp` and the field.
4. The existing Network2 fallback runs afterward for LAN/global-offset state but cannot overwrite the already-applied `Parameter.ipdfserver` value.

## Expected startup flow

```
MariaDB iScreen.Parameter.id=1.ipdfserver
        |
        v
DatabaseDF::GetIPDFServerFromDB()
        |
        v
iScreenDF::GetIPDFServer()
        |
        v
m_parameter.m_ipdfServer
        |
        |  later, when Endpoints page is loaded
        v
iScreenDF::updateIPServerDF()
        |
        v
updateServeripDfserver(ip)
        |
        v
ServiceEndpointsPage DF Server IP field
```

## Persistence behavior unchanged

Apply still uses the existing `connectToDFserver(ip)` path, which updates `Parameter.ipdfserver` and reconnects DF TCP/GPSD. No new persistence logic was introduced.

## Bench validation

1. Query `SELECT id, ipdfserver FROM Parameter WHERE id=1;` and note the value.
2. Start/restart the application.
3. Open Network Settings -> Endpoints without pressing Apply.
4. DF Server IP must equal `Parameter.ipdfserver`.
5. Change the IP and press Apply.
6. Verify the database value changed.
7. Restart the application and open Endpoints again.
8. The field must show the newly persisted database value, not an older `Network2.krakenserver` value.
