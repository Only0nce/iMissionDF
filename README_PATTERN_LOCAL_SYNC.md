# SYNC Pattern Local Filesystem Path (v11)

Baseline: `src_20082026.tar.xz` (2026-08-20).

SYNC compatibility payload contract in this revision:

```
EVENT <verified Event CSV remote FTP file>
| PICTURE <verified Picture remote FTP file>
| PATTERN <verified Pattern LOCAL filesystem file or empty>
```

The top-level `PatternPATH` field uses the same verified local Pattern path.

Pattern remains optional. If the source Pattern file is missing, outside
`/mnt/sdcard/event_records/Pattern`, unreadable, not a regular file, or size is
zero at SYNC publish time, `PATTERN` and `PatternPATH` are emitted empty.

FTP upload of Pattern remains optional backup behavior and is not used as the
SYNC Pattern path.
