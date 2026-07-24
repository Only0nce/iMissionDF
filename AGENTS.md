# AGENTS.md — iSense / iScanMR10

## Application Purpose

iScanMR10 is a Qt 5 C++/QML application for receiver control, recording,
direction finding and maps, DoA viewing, network configuration, and
Jetson/Orin hardware integration. `main.cpp` loads `qrc:/main.qml`.

## Source Tree and Important Modules

- `main.cpp`: application setup, platform environment, QML registrations and
  context properties, single-instance local socket, and C++/QML wiring.
- `Mainwindows.cpp/.h`: primary QML bridge and command dispatcher.
- `NetworkController.cpp/.h`: LAN, Wi-Fi, cellular, NTP, and system command
  integration. Keep `nmcli`, `mmcli`, and interface parsing here.
- `NetworkSecurityController.cpp/.h`: authorization for protected network
  changes. Treat password hashes and related environment configuration as
  sensitive.
- `Wifi5GController.cpp/.h`: adapter between the Wi-Fi/5G QML workflow and
  `NetworkController`; it must not become a second network implementation.
- `ReceiverConfigManager.*`, `ReceiverRecorderConfigManager.*`, and
  `OpenWebRxConfig.*`: receiver, recorder, and OpenWebRX configuration.
- `iScreenDF/` and `iScreenDFqml/`: direction-finding, maps, logging, network,
  and associated QML.
- `iRecordManage/`: Jetson-only recorder backend and persistence.
- `DoaViewer/`: direction-of-arrival client/viewer integration.
- `DesignDSP_REC_V1/`: generated SigmaStudio data. Do not hand-edit it unless
  the DSP export itself is intentionally replaced.

Root QML screens and components are compiled through `qml.qrc`. Important
screens include `MainPage.qml`, `Setting.qml`, `Wifi5GPage.qml`,
`Wifi5GView.qml`, `RecConfigPage.qml`, `RadioScanner.qml`, and
`OpenWebRXProfiles.qml`.

## Authoritative Build System

The authoritative build definition is `iScanMR10.pro` using qmake. A generated
root `Makefile` is tracked historically but is machine-specific and is not the
source of truth. Do not migrate this project to CMake unless explicitly
requested.

Desktop:

```bash
./scripts/build-desktop.sh
```

Equivalent qmake configuration:

```bash
qmake iScanMR10.pro -spec linux-g++
make -j"$(nproc)"
```

Jetson/Orin:

```bash
./scripts/build-target.sh
```

The target build requires the project-specific `linux-jetson-orin-g++` mkspec,
sysroot, compiler, and libraries. A desktop build does not validate the target
toolchain or hardware.

## Hardware and Platform Selection

Hardware capability and build platform are independent:

- `HW_5G` versus `HW_NONE_5G` selects hardware features.
- `PLATFORM_JETSON` versus `PLATFORM_X86` is selected by the qmake mkspec.

At the inspected development commit, `iScanMR10.pro` actively selects
`CONFIG+=HW_5G`; the nearby comment claiming a non-5G default is stale. Never
enable both hardware selectors. Inspect the active source line before every
build and record it in validation output.

Keep Jetson-only recorder, GPIO, DSP, audio, RF, and device access behind the
existing platform guards. Do not assume a desktop compile exercises those
paths.

## Qt, QML, and Threading Contracts

- Preserve QObject ownership, parent-child lifetime, and thread affinity.
- Create and start QTimers in the thread that owns them.
- For worker objects, verify construction, `moveToThread`, queued connections,
  shutdown, `deleteLater`, and QThread destruction as one lifecycle.
- Never block the GUI thread with device, process, network, or database I/O.
- Preserve signal/slot signatures, connection types, `Q_PROPERTY` names, and
  NOTIFY semantics used by QML.
- Keep `Wifi5GView.qml` presentation-focused. Runtime wiring belongs in
  `Wifi5GPage.qml` and the controller layer.
- Avoid binding loops, render-path JavaScript, unbounded models, and repeated
  allocation in high-rate spectrum/waterfall paths.

## Network and Protocol Contracts

QML commonly submits JSON through
`mainWindows.cppSubmitTextFiled(JSON.stringify(obj))`.
`Mainwindows::cppSubmitTextFiled(const QString &)` dispatches the request and
responses return through `cppCommand(QVariant)`.

Preserve:

- stable `menuID` values;
- response keys such as `ok`, `message`, and existing payload fields;
- WebSocket framing and reconnect behavior;
- local socket name and single-instance commands;
- network interface resolution and process timeouts;
- the separation between read-only status operations and state-changing
  network operations.

Never log or return Wi-Fi passwords, VPN secrets, tokens, authorization values,
or the network administrator password/hash. Existing code that reads saved
NetworkManager secrets requires security review before expansion.

## Database and Persistent-Data Contracts

The repository contains multiple Qt SQL/SQLite implementations in the root,
`iScreenDF/`, and `iRecordManage/`. Database filenames, table/column names,
types, defaults, and migration behavior are compatibility contracts. Before a
schema change, identify every reader/writer and define backup, migration, and
rollback behavior. There is no verified centralized migration framework.

Receiver, recorder, OpenWebRX, network, and QML settings files are also
persistent contracts. Do not silently rename keys or paths.

## Hardware Interfaces

Hardware-facing code includes GPIO, SPI, I2C, ALSA, GPS, RFDC/NCO, DSP,
recorder, modem, and RF/network control. Read-only inspection and active
hardware testing are separate operations. Obtain approval before commands that
can transmit RF, write buses/registers, toggle GPIO, reset a modem/device,
change power, or alter network state.

Never infer hardware correctness from source review or compilation. Record the
target, firmware/hardware version, command, expected result, observed result,
logs, and rollback.

## Generated and Machine-Specific Files

Do not edit or newly commit:

- `build*/`, `.qtc_clangd/`, object files, binaries, and generated Qt files;
- `Makefile`, `.qmake.stash`, `*.pro.user`, or `*.pro.user.*`;
- `.claude/settings.local.json`, `CLAUDE.local.md`, local inventories, secrets,
  credentials, sessions, caches, or histories.

Some of these artifacts are already tracked historically. Ignore rules do not
untrack them. Removing them requires a separately reviewed cleanup.

## Validation

Run before handoff:

```bash
./scripts/verify-dev-env.sh
git diff --check
./scripts/build-desktop.sh
```

Run the target build only with the correct toolchain. No dedicated automated
application test suite was found during migration; add focused tests when a
change can be exercised deterministically.

Report static review, build, unit/integration tests, runtime checks, target
build, and physical hardware tests as separate results.

## Known Risks and Unsupported Assumptions

- The development branch contains tracked IDE/cache/generated files with
  machine-specific paths.
- A legacy network administrator password hash is embedded in source as a
  fallback. Its value is sensitive and should be rotated/removed in a separate
  security change.
- Current desktop Qt is not proof of compatibility with every target Qt 5
  installation.
- The README contains historical machine-specific paths and must not be treated
  as portable setup data.
- Exact production hardware, runtime services, database contents, and target
  toolchain availability cannot be inferred from this repository alone.
