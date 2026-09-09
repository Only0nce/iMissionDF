# PLCServer v17 - FTP Remote Directory Diagnostics

This version focuses only on the centralized Event FTP transaction and Event Audit behavior after v16 One Event One Picture.

## Fixed FTP configuration mapping

Configuration loading now uses the correct keys:

- `MANUAL_FILE <- PATH_MANUAL_FILE`
- `PATTERN_FILE <- PATH_PATTERN_FILE`

Previously Manual incorrectly loaded `PATH_RELAY_FILE`, and Pattern incorrectly loaded `PATH_SURGE_FILE`.

At configuration load PLCServer logs the FTP host, whether a username is present, and all configured roots. Passwords are never logged.

## Exact FTP remote directory verification

`FTP_REMOTE_DIR` is no longer one opaque mkdir command. PLCServer walks the hierarchy one level at a time:

1. `cd <directory>`
2. when missing, `mkdir <directory>`
3. `cd <directory>` again to prove it exists

The audit details contain `directoryChecks` for every level and, on failure:

- `failedDir`
- `operation`
- `failureClass`
- `exitCode`
- `timedOut`
- `stderr`
- `stdout` when available
- process error when lftp cannot start

FTP passwords are redacted from diagnostics and diagnostic strings are bounded in size.

## Detailed Event CSV / Picture FTP diagnostics

The centralized Event FTP PUT and download-back GET verification now also publish detailed lftp process results for every failed/retried attempt.

Failure classification includes:

- `TIMEOUT`
- `PROCESS_START_FAILED`
- `AUTHENTICATION_FAILED`
- `PERMISSION_DENIED`
- `CONNECTION_FAILED`
- `REMOTE_PATH_REJECTED`
- `FTP_COMMAND_FAILED`

## Audit state after fatal FTP failure

A fatal FTP failure no longer leaves later checklist rows as `PENDING`.

Example when `FTP_REMOTE_DIR` fails:

- `FTP_REMOTE_DIR` -> `FAIL`
- `FTP_EVENT_CSV` -> `BLOCK`
- `FTP_PICTURE` -> `BLOCK`
- `FTP_PATTERN` -> `SKIP`
- `FTP_BUNDLE` -> `FAIL`
- `FINAL_LOCAL_VERIFY` -> `BLOCK`
- `SENDMAIL_NOTIFICATION` -> `BLOCK`
- `LOOP_WAIT_PIC` -> `BLOCK`
- `SYNC_VERIFY` -> `BLOCK`
- `SYNC_PUBLISH` -> `BLOCK`
- `EVENT_COMPLETE` -> `FAIL`

No notification/SYNC behavior is relaxed; mandatory Event CSV + Picture still have to pass the FTP bundle verification.
