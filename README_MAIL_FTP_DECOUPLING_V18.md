# PLCServer v18 - Mail / FTP Dependency Fix

## Problem
`sendMail` was published only after the mandatory FTP bundle succeeded. If FTP remote directory creation, CSV upload, Picture upload, or download-back verification failed, the code returned before the `sendMail` JSON was emitted even when the local Event CSV and Picture were already verified and stable.

## Policy
- Event CSV + Picture are still mandatory local evidence.
- If either mandatory local file fails verification: no mail, no FTP/SYNC publish.
- FTP remains mandatory for SYNC because SYNC contains verified remote FTP paths.
- FTP failure no longer suppresses email when the local Event CSV + Picture pass a fresh final verification.
- Pattern remains optional.
- `PicFtpUploaded` is `false` in a mail payload sent through the local-file fallback after FTP failure.

## Runtime behavior
### FTP success
1. Stage 1/2 local verification PASS.
2. FTP bundle PASS.
3. Final local verification PASS.
4. `sendMail` publishes with `PicFtpUploaded=true`.
5. SYNC continues normally.

### FTP failure
1. Stage 1/2 local verification PASS.
2. FTP bundle FAIL.
3. Local Event CSV + Picture are verified again.
4. If still stable, `sendMail` publishes with `PicFtpUploaded=false`.
5. SYNC remains BLOCK because remote FTP paths were not verified.
6. Event overall remains FAIL because the FTP/SYNC transaction did not complete.

## Mail diagnostics
`SENDMAIL_NOTIFICATION.details` now includes connected target counts and `snmpConnected`. The application logs warnings when no WebSocket notification consumer is connected, and a separate warning when the SNMP service is disconnected.

## Duplicate protection
One live Event publishes at most one `sendMail` notification to connected consumers, even if the Picture/FTP transaction re-enters for recovery.
