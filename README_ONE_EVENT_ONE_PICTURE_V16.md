# PLCServer One Event = One Picture (v16)

Baseline: `src_21082026.tar.xz`

## Production contract

For Manual, Relay, Surge and Periodic events:

- `eventRecord` opens one Picture ownership window.
- The first valid `ScreenPicture` reserves the event Picture slot.
- A second `ScreenPicture` for the same event is ignored while the first is processing, after it is verified, and after the event is closed.
- If the first Picture transaction fails before verification (download, local verification, CSV verification or FTP verification), the reservation returns to `WAITING` so a genuine recovery request can retry.
- `DateKept` / `TimeKept` are no longer allowed to resurrect a completed event for ScreenPicture processing. An active `eventRecord` snapshot is mandatory.
- The exact event Picture folder is checked before download. If a different valid Picture already exists, PLCServer refuses to create a second file.
- Final SYNC success/failure closes the Picture ownership window.

## Picture ownership states

`IDLE -> WAITING -> PROCESSING -> VERIFIED -> CLOSED`

Failure before `VERIFIED` may transition `PROCESSING -> WAITING` for retry.

## Useful logs

Accepted first request:

```
[SCREEN-RX] eventId=... state=WAITING
[PIC-OWNER][PROCESS] ...
[PIC-OWNER][VERIFIED] ...
```

Duplicate/late request:

```
[SCREEN-RX][DROP-DUPLICATE] ...
reason=SAME_PICTURE_ALREADY_RESERVED_OR_COMMITTED
```

or

```
reason=EVENT_ALREADY_OWNS_ANOTHER_PICTURE
```

No active eventRecord:

```
[SCREEN-RX][DROP] reason=NO_ACTIVE_EVENT_OWNER
```

Retry enabled after a failed Picture transaction:

```
[PIC-OWNER][RETRY-READY] ...
```

Event closed after SYNC:

```
[PIC-OWNER][CLOSE] ...
```

## Event notification duplicate suppression

The generic `eventText` notification is no longer sent when it is equivalent to the canonical mode-specific event trap, including formatting variants such as `MANUAL TEST EVENT` vs `MANUAL_TEST_EVENT`. The mode-specific notification remains authoritative.
