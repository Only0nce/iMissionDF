# Daily Event Retention 100 - v15

Automatic retention policy for local event storage:

- `Pic`: keep newest 100 event sets
- `Manual`: keep newest 100 event sets
- `Relay`: keep newest 100 event sets
- `Surge`: keep newest 100 event sets
- `Periodic`: keep newest 100 event sets
- `Pattern`: EXCLUDED, unlimited, never scanned/deleted by this policy

One event set is one `yyyy-MM-dd/HH-mm-ss/` folder. The retention job sorts sets newest-first and deletes only the oldest sets beyond 100. All files inside an excess event folder are removed together, so CSV/Margin/support files are not left partially behind.

For Manual/Relay/Surge/Periodic, matching rows in `event_records` are removed transactionally with the physical set. Missing-file stale DB rows under the expected category base are reconciled daily. Paths outside the expected category base are never auto-deleted from the DB.

The legacy slot name `deleteOldFilesAndRecords()` is intentionally retained because PLCServer already invokes it at startup and once on each date change. Its behavior is now count-based instead of the previous one-month age policy.

Safety rules:

- Retention is blocked if `/mnt/sdcard/event_records` is unavailable or does not resolve to a verified `/mnt/sdcard` mount.
- Base folders themselves are never removed.
- `Pattern` is never included in retention lists.
- Empty non-Pattern date/time folders may be removed.
- The old emergency cleanup behavior that could wipe all non-Pattern data at >=70% storage usage is disabled. High-storage handling no longer deletes anything by itself; it only logs a warning. The daily 100-set retention job is the only automatic destructive policy.
