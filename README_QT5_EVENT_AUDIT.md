# Qt5 Event Process Audit WebSocket Publisher — v10

This version adds only the Qt5/PLCServer publisher and protocol.  No HTML, CSS,
or JavaScript UI is included.  The existing web application consumes the JSON
through the PLCServer WebSocket connection already used by `webapp_address`.

## WebSocket subscription / snapshot

A connected web client can request the current state with either:

```json
{"menuID":"getEventProcessAudit"}
```

or:

```json
{"objectName":"GET_EVENT_PROCESS_AUDIT"}
```

PLCServer immediately replies with one `SNAPSHOT`, then pushes future audit
messages in real time on the same socket.

## Stable protocol

All audit records use:

```json
"objectName": "EVENT_PROCESS_AUDIT",
"schemaVersion": 2
```

`messageType` is one of:

- `EVENT_START`
- `STEP`
- `EVENT_COMPLETE`
- `SNAPSHOT`

A STEP contains the fields required for a table/checklist/timeline:

```json
{
  "objectName": "EVENT_PROCESS_AUDIT",
  "messageType": "STEP",
  "schemaVersion": 2,
  "eventId": "Manual_20260819_161837_630",
  "eventMode": "Manual",
  "eventName": "MANUAL_TEST_EVENT",
  "eventDate": "2026-08-19",
  "eventTime": "16-18-37",
  "sequence": 12,
  "expectedOrder": 13,
  "processCode": "VERIFY_STAGE1_PICTURE",
  "processName": "Picture verification stage #1",
  "mandatory": true,
  "status": "PASS",
  "passed": true,
  "timestamp": "2026-08-19T16:18:38.250+07:00",
  "elapsedMs": 1250,
  "description": "Picture passed stage #1 physical verification",
  "source": "/mnt/sdcard/event_records/Pic/...png",
  "destination": "verified picture snapshot",
  "details": {},
  "runtime": {}
}
```

## Status values

Qt5 publishes these status values:

- `START`
- `PASS`
- `FAIL`
- `RETRY`
- `BLOCK`
- `INFO`
- `SKIP`

`PATTERN_OPTIONAL` and `FTP_PATTERN` are `mandatory:false`.  An absent Pattern is
reported as `SKIP`; it does not fail the event.

Event CSV and Picture steps remain mandatory.  Existing mandatory gates still
block FTP/socket publication if either mandatory file fails verification.

## Event checklist

The current checklist definition is included in `EVENT_START` and `SNAPSHOT`.
The main process codes are:

1. EVENT_START
2. EVENT_MODE_RESOLVED
3. DATA_PHASE_A
4. DATA_PHASE_B
5. DATA_PHASE_C
6. CSV_LOCAL_READY
7. SCREEN_REQUEST_RECEIVED
8. PICTURE_DIRECTORY_READY
9. PICTURE_DOWNLOAD
10. PICTURE_RESOLVED
11. CSV_RESOLVED
12. VERIFY_STAGE1_PICTURE
13. VERIFY_STAGE1_CSV
14. VERIFY_STAGE1_GATE
15. VERIFY_STAGE2_PICTURE
16. VERIFY_STAGE2_CSV
17. VERIFY_STAGE2_GATE
18. PATTERN_OPTIONAL (optional)
19. PRE_FTP_GATE
20. FTP_REMOTE_DIR
21. FTP_EVENT_CSV
22. FTP_PICTURE
23. FTP_PATTERN (optional)
24. FTP_BUNDLE
25. FINAL_LOCAL_VERIFY
26. SENDMAIL_NOTIFICATION
27. LOOP_WAIT_PIC
28. SYNC_VERIFY
29. SYNC_PUBLISH
30. EVENT_COMPLETE

## Machine/runtime information

`EVENT_START` and `SNAPSHOT` contain a full `machine` object with host/kernel,
architecture, memory, storage, network addresses, socket states and configured
equipment information.

Every real-time step contains the lighter `runtime` object (RSS, available RAM,
load, event-storage free bytes and key socket/client states).  This avoids
repeating large static machine information for every process update.

No FTP/SMTP password or token is published in audit JSON.

## Snapshot behavior

`SNAPSHOT` contains:

- current event identity
- checklist definition
- latest state for every processCode
- current counters (`pass`, `fail`, `retry`, `block`, `info`, `skip`, `start`)
- the latest 20 completed-event summaries
- full machine information
- current runtime information

The web UI can therefore refresh/reconnect without waiting for a new event.

## Local persistence

Audit records are also appended as JSONL under:

```text
/mnt/sdcard/event_records/Audit/<yyyy-MM-dd>/<eventId>.jsonl
```

This is diagnostic persistence only.  The web application does not need to read
these files directly.

## WebSocket/thread rule

Audit WebSocket transmission is marshalled to the PLCServer/QWebSocket owner
thread.  Event processing never waits for a browser acknowledgement.  The web
client is an observer and cannot unblock or control the mandatory event flow.
