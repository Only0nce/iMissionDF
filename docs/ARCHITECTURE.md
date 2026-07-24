# Architecture

## Overview

iScanMR10 is a Qt 5 C++/QML embedded user interface for receiver control,
recording, direction finding/maps, DoA viewing, and network/hardware management.
qmake builds `iScanMR10.pro`; `main.cpp` starts the application and loads
`qrc:/main.qml`.

## Runtime Layers

```text
QML screens and controls
        |
        | signals, context properties, Q_INVOKABLE, JSON menuID
        v
Mainwindows and feature adapters
        |
        +--> receiver/OpenWebRX configuration
        +--> NetworkController / NetworkSecurityController
        +--> direction finding and DoA clients
        +--> Jetson recorder backend (target only)
        |
        v
Qt network/SQL/process APIs, Linux services, device files, and hardware
```

## Entry and QML Registration

`main.cpp` configures the desktop or Jetson Qt platform, registers QML types and
singletons, exposes context properties, creates the primary backend objects,
loads `main.qml`, and connects root QML signals to C++ slots. It also exposes
hardware capability flags derived from qmake defines.

`qml.qrc` is the resource manifest for root and nested QML/assets. A QML file
added to source but omitted from the resource manifest is not available through
the deployed `qrc:/` path.

## Command and Response Contract

QML commonly serializes a request and submits it to
`Mainwindows::cppSubmitTextFiled`. Requests use stable `menuID` routing.
Responses return through `cppCommand(QVariant)` and existing keys such as
`ok`, `message`, and feature-specific data.

Treat menu IDs, object names, signal signatures, context-property names, and
payload keys as public internal APIs. Change both producers and consumers
together and preserve backward compatibility where possible.

## Networking

`NetworkController` is the source of truth for LAN, Wi-Fi, cellular, and time
operations. It invokes Linux tools and reads/writes system configuration.
`Wifi5GController` and QML pages adapt this backend to the UI.

Read-only status/scan operations must remain distinguishable from connect,
disconnect, forget, reset, or configuration writes. Passwords and saved
NetworkManager secrets must not cross into logs or persistent QML state.

## Persistence

Qt SQL/SQLite code exists in the root, `iScreenDF/`, and `iRecordManage/`.
Receiver, recorder, OpenWebRX, network, and QML settings also persist state in
files or Qt settings. The repository has no verified centralized schema
migration framework. Identify all readers/writers before changing names, types,
defaults, or paths.

## Threads and High-Rate Data

The project uses workers, QThread patterns, timers, sockets, audio, and
high-rate spectrum/waterfall updates. Ownership and shutdown must be reviewed
end to end. Bound queues and UI update rates, keep blocking work off the GUI
thread, and preserve queued payload lifetime.

## Platform and Hardware

The qmake mkspec selects desktop (`PLATFORM_X86`) or Jetson
(`PLATFORM_JETSON`) sources and libraries. The hardware selector independently
sets 5G/Wi-Fi capability defines. Target-only code includes recorder, GPIO,
DSP/audio, RF, GPS, buses, and target libraries.

At the inspected development commit the active hardware line is `HW_5G`.
Always inspect source rather than relying on nearby comments.

## Unresolved Architecture Facts

- Production database contents and deployed schema versions are not in Git.
- Production service versions and target device permissions are not verified.
- The full target toolchain/sysroot is machine-specific.
- Physical hardware revisions and firmware versions require operator records.
- Historical README paths are examples from one machine, not portable contracts.
