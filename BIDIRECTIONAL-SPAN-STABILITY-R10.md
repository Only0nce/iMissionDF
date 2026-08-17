# AstraRX / Qt5 Bidirectional Span + Stability R10

Revision: `20260817-bidirectional-span-stability-r10`

## Frequency ownership

AstraRX remains the sole Active RF Source / RF center owner.

For a Qt receiver request:
- inside the safe IQ span: Qt sends `dspcontrol.offset_freq`; RF center is unchanged.
- outside the safe IQ span: Qt sends one `setfrequency`; QML keeps its last confirmed center until AstraRX confirms the new RF center.

For an AstraRX/browser/API RF-center change:
- if the current Qt receiver still fits inside the new safe span, AstraRX preserves the receiver and sends the new center + recomputed offset to Qt/QML.
- if the current Qt receiver no longer fits, AstraRX resets the Qt5 session receiver to the new confirmed RF center (`offset=0`) and pushes that state to Qt/QML.
- QML also contains a defensive span clamp for old/partial backend snapshots.

Safe offset limit is shared conceptually by both sides:

`sample_rate / 2 - demod_bandwidth / 2`

For 7.680000 MHz and 22.5 kHz demod bandwidth, the safe limit is +/-3.828750 MHz.

## Stability hardening

- Removed the raw `pthread` SQL watcher; replaced with a `QTimer` owned by `Mainwindows`.
- Input event readers and `AlsaRecConfigManager` are now QObject-owned.
- SIGTERM/SIGINT no longer call Qt APIs inside the POSIX signal handler. A non-blocking pipe + `QSocketNotifier` performs graceful shutdown in the Qt thread.
- Qt warnings/info remain visible; only debug output is suppressed by default.
- The QML engine is explicitly destroyed before C++ context backends during shutdown.
- SetFreqWorker remains excluded and the legacy 30 MHz reset is absent.

## Runtime evidence to expect

AstraRX external center push:

`[QT5-FREQ-PUSH] reason=broadcast_status ... outside_new_span=0|1 ...`

Qt/QML reception:

`[QT5-CENTER-RX] ...`
`[QT5-RECEIVER-RX] ...`
`[QML-FREQ-ASTRARX-PUSH] ...`

If the old receiver was outside the new span:

`outside_new_span=1`
`receiver_after=<new center>`
`offset_after=0`

## Remaining scope note

This archive does not contain external project directories referenced by the qmake project (for example `iRecordManage`, `iScreenDF`, `DoaViewer`). Runtime termination inside those external modules cannot be statically repaired from this source-only archive. R10 removes/hardens the unsafe lifecycle paths visible in the provided Qt source and preserves warnings so any remaining external-module failure is visible.
