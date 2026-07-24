# C++ and Qt 5 Rules

- Preserve QObject ownership and parent-child lifetime.
- Verify thread affinity before accessing a QObject.
- Use a complete worker lifecycle: construction, move, connections, start,
  shutdown, `deleteLater`, thread quit/wait, and destruction.
- Create and start QTimers in their owner thread.
- Preserve signal/slot signatures, connection semantics, metatype registration,
  and QML-visible `Q_PROPERTY`/NOTIFY contracts.
- Avoid blocking I/O, process waits, long locks, and heavy work on the GUI thread.
- Define lock ownership and ordering; check atomics, shared state, and queued data lifetime.
- Bound buffers, process output, retry loops, and network reconnect behavior.
- Clean up QProcess, sockets, files, memory, and descriptors on every exit path.
- Keep platform-specific code behind the existing qmake and preprocessor guards.
- Use APIs available in the project's supported Qt 5 versions; do not assume Qt 6.
