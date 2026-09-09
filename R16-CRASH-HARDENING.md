# R16 Crash Hardening

## Scope

R16 is a diagnostic/lifecycle hardening change for the recurring iScanMR10 termination/crash issue. It intentionally does not change ALSA playback behavior, AstraRX protocol payloads, tuning rules, recorder behavior, or visible Spectrum functionality.

## Evidence driving the change

The captured fatal runs repeatedly show SIGSEGV on the process main thread with fault address `0x100`, while the resolved PC lands at the same `libQt5Qml.so.5.15.2` offset. Audio worker checkpoints can be the most recent global activity but are not the faulting thread. Separate SIGTERM events execute the normal Qt shutdown path, proving that external termination and QML crashes are different failure classes.

## Changes

### 1. SIGTERM/SIGINT sender attribution

`main.cpp` now installs SIGTERM/SIGINT with `SA_SIGINFO` and forwards a small async-signal-safe record through the existing non-blocking pipe. Diagnostic output preserves the existing `[R13-TERM]` prefix and adds:

- `sender_pid`
- `sender_uid`
- `si_code`

This lets the next SIGTERM distinguish systemd, a launcher, a shell/user process, or another sender without changing shutdown behavior.

### 2. Main-thread Qt -> QML checkpoints

`Mainwindows.cpp` now brackets the highest-value Qt-to-QML emissions with BEGIN/END checkpoints:

- `MAIN_FFT_EMIT_BEGIN/END`
- `MAIN_SMETER_EMIT_BEGIN/END`
- `MAIN_WFCOLOR_EMIT_BEGIN/END`
- `MAIN_FREQ_EMIT_BEGIN/END`

If synchronous QML/JS execution faults during one of these emissions, the fatal handler's thread-local checkpoint remains at the matching BEGIN marker.

### 3. SpectrumGLPlot lifecycle-scoped signal handling

The live `mainWindows.*.connect(...)` JavaScript callbacks were removed from `Component.onCompleted`. `Connections { target: mainWindows }` now owns these subscriptions so Qt/QML tears them down with the component lifecycle.

The live FFT path now calls `consumeFftFrame()` directly. Legacy local signals remain declared for compatibility, but the primary Mainwindows path no longer performs the extra JavaScript signal bridge.

## Files changed

- `CrashDiagnostics.h`
- `CrashDiagnostics.cpp`
- `main.cpp`
- `Mainwindows.cpp`
- `SpectrumGLPlot.qml`

## Static validation performed

- QML delimiter balance: PASS
- Executable JavaScript `.connect()` remaining in `SpectrumGLPlot.qml`: NONE
- Mainwindows signal names vs QML Connections handlers: PASS
- New checkpoint enum/name/order audit: PASS
- `SA_SIGINFO` sender capture audit: PASS
- `git diff --check`: PASS

A full qmake/Jetson compile was not run in the analysis environment because the required Qt 5.15.2 qmake/toolchain paths are not installed there. Build on the normal Jetson/Qt development host before deployment.

## Required target validation

Build using the existing project toolchain, deploy the new binary/QML resources, then verify startup contains:

```text
R16 crash hardening active
```

For SIGTERM, expect a line similar to:

```text
[R13-TERM] signal=15 sender_pid=1234 sender_uid=1000 si_code=0 ...
```

For a recurring SIGSEGV, inspect `thread_checkpoint` first. A fatal line ending at one of the new `*_EMIT_BEGIN` checkpoints identifies which Qt-to-QML transaction was executing when the main thread faulted.

## Soak-test decision rule

Do not broaden the patch to every QML page yet. Run the R16 build long enough to reproduce the former failure window. If the crash disappears, the Spectrum lifecycle change is a strong confirmation. If it remains, use the new main-thread checkpoint to select the next signal/page instead of modifying unrelated audio code.
