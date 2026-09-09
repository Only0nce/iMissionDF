# iScanMR10 R13 Diagnostic Instrumentation

Purpose: preserve the R12 runtime behavior while adding persistent crash evidence for unexplained `terminated abnormally` events.

## Diagnostic file

Default:

```text
/tmp/iScanMR10-debug.log
```

Override without source changes:

```bash
export ISCAN_DIAG_LOG=/var/log/iScanMR10-debug.log
```

The file is append-only across application restarts. Every run begins with:

```text
========== iScanMR10 R13 diagnostic boot ==========
```

and includes the process memory map for that exact ASLR run.

## What is recorded

- fatal signals: SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL
- fault address, program counter (PC), stack pointer (SP) on AArch64/x86_64
- global and crashing-thread checkpoints
- SIGTERM/SIGINT requests through the existing graceful signal bridge
- `std::terminate()`
- `atexit()` so a direct `exit()` path is distinguishable from a signal crash
- 5-second process heartbeat: VmRSS, VmSize, VmSwap, thread count, FD count, event-loop interval
- Mainwindows constructor/destructor and owned-pointer snapshot
- 5G async reset lifecycle
- WebSocket binary/text/audio checkpoints
- ALSA init/start/stop/write/recovery/close and queue-drop events
- ChatServer connect/disconnect/send-to-recorder lifecycle
- recorder configuration/check-alive lifecycle
- FileUpdateWatcher update trigger
- I2C abnormal reads/writes
- SigmaDSP SPI/register I/O checkpoints
- Network/WiFi/5G short-lived async workers

## Interpreting the end of the file

### SIGSEGV / pointer / array / use-after-free

Example:

```text
[R13-FATAL] signal=11 ... thread_checkpoint=307(AUDIO_WRITE) addr=0x... pc=0x...
```

`signal=11` is SIGSEGV. `thread_checkpoint` is more important than the global checkpoint because it describes the thread that actually faulted.

### SIGABRT / qFatal / assertion / heap corruption / std::terminate

```text
[R13-FATAL] std::terminate invoked
[R13-FATAL] signal=6 ...
```

or just signal 6 after an assertion/abort.

### External SIGTERM

```text
[R13-TERM] signal=15 ...
```

This means the application was asked to terminate; it is not an unexplained pointer crash.

### Direct `exit()` from code

If the last marker is:

```text
[R13-FATAL] atexit reached
```

but there is no preceding:

```text
domain=APP event=aboutToQuit
... event=event loop returned
```

then code/library likely called libc `exit()` directly.

### SIGKILL / OOM / launcher kill / power loss

SIGKILL cannot be handled. If heartbeats simply stop and there is **no** R13-FATAL, R13-TERM or atexit marker, inspect kernel/OOM/service/launcher state. The last heartbeat still shows memory/thread/FD trends immediately before death.

### Leak indicators

Heartbeat fields should stay approximately bounded:

```text
VmRSS=...
VmSwap=...
Threads=...
FDs=...
```

Monotonic growth across many heartbeats is suspicious.

## Checkpoint groups

```text
100-106  Mainwindows / SQL / command / 5G
200-206  AstraRX WebSocket client / audio receive
300-310  ALSA playback thread / queue / write / recovery
400-406  ChatServer / recorder WebSocket client lifetime
500-505  AlsaRecConfigManager
600      FileUpdateWatcher
700-701  I2C
800      SigmaDSP / SPI
900      Network/WiFi/5G async workers
```

## After the next termination

Do not restart the application repeatedly before copying the end of the log. The file is append-only, so older runs remain, but the newest boot separator should be used.

Run:

```bash
tail -n 300 /tmp/iScanMR10-debug.log
```

and:

```bash
grep -E 'R13-FATAL|R13-TERM|atexit|heartbeat|thread wait|queue drop|disconnect|worker' \
    /tmp/iScanMR10-debug.log | tail -n 200
```

If a PC is reported, keep the executable that produced the crash. The `/proc/self/maps` snapshot in the same run can be used to resolve PIE/shared-library addresses with GDB/addr2line.

## Scope limitation

The provided source archive still does not contain all Jetson-compiled `iRecordManage`, `iScreenDF`, and `DoaViewer` implementation files. R13's process-wide fatal handler can still record a signal and PC if one of those modules crashes, but function-level checkpoints cannot be inserted into source that is absent from the archive.
