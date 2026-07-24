---
name: verify-qt5
description: Verify Qt 5 C++, QML, qmake, ownership, threading, and interface compatibility for a scoped change.
---

# Procedure

1. Confirm qmake and Qt versions and inspect active platform/hardware selectors.
2. Verify changed files are present in the correct qmake source/header/resource lists.
3. Review QObject ownership, thread affinity, QTimer lifecycle, connections,
   Q_PROPERTY/NOTIFY, and cleanup.
4. Review QML imports, bindings, signals, context properties, JSON keys, and render cost.
5. Run `git diff --check`, environment verification, and the relevant desktop build.
6. Run target validation only with the correct target toolchain.
7. Report static, compile, runtime, and hardware evidence separately.

# Required Output

## Qt/qmake Environment
## Static Review
## Desktop Build
## Target Build
## Runtime/QML Checks
## Remaining Risks
