# Event Base Timestamp v14

Authoritative source baseline: `src_20082026.tar(1).xz`.

## Contract

`objectName=eventRecord` owns the event base timestamp used by FTP and SYNC:

- Date: `yyyy-MM-dd`
- Time: `HH-mm-ss`

Picture and local ADC/CSV timestamps are correlation timestamps only. They may differ by a few seconds and must not move the remote event folder.

Example:

- eventRecord: `2026-08-20 10-15-10`
- local CSV: `..._M20260820_101501.csv`
- Picture: `20260820_101518_193.png`

Remote output:

- folder: `event_record/.../2026-08-20/10-15-10/`
- Event CSV: `..._M20260820_101510.csv`
- Picture: `20260820_101518_193.png`

Local Event CSV/DB path is not renamed. Only the FTP Event CSV object name is canonicalized to the eventRecord base timestamp.

The existing 120-second correlation window is retained for local Event CSV/Picture validation.
