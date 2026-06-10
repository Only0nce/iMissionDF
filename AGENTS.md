# AGENTS.md — iSense / iScanMR10

## Project Structure & Module Organization

The application is a Qt 5 / QML / C++embedded UI project built from `iScanMR10.pro`. The main C++ entry point is `main.cpp`, and the primary QML entry point is loaded from `qrc:/main.qml`. Core QML screens live at the repository root, including `MainPage.qml`, `Setting.qml`, `Wifi5GPage.qml`, `Wifi5GView.qml`, `RecConfigPage.qml`, `RadioScanner.qml`, `OpenWebRXProfiles.qml`, and related shared controls such as `AppButton.qml`, `StatusBadge.qml`, `FieldLabel.qml`, and `SignalBar.qml`.

Core backend logic is split across `Mainwindows.cpp/.h` for the QML bridge, `NetworkController.cpp/.h` for LAN/WiFi/5G/NTP/system networking, `Wifi5GController.cpp/.h` as the WiFi/5G UI adapter, `ReceiverConfigManager.*` and `ReceiverRecorderConfigManager.*` for receiver profiles, and `OpenWebRxConfig.*` for OpenWebRX integration. Direction finding and map logic is under `iScreenDF/`, recorder-specific logic is under `iRecordManage/`, and DoA viewer code is under `DoaViewer/`. Do not edit generated or build-output directories such as `build/`, `.qtc_clangd/`, generated `moc_*`, `qrc_*`, or object files.

## Build, Test, and Development Commands

Use qmake, not CMake, unless the project is explicitly migrated. For a normal desktop build, run:

```bash
qmake iScanMR10.pro -spec linux-g++
make -j$(nproc)
```

For the Jetson Orin target build, run:

```bash
qmake iScanMR10.pro -spec linux-jetson-orin-g++
make -j$(nproc)
```

Check the active Qt/mkspec configuration with:

```bash
qmake -v
qmake -query QMAKE_SPEC
```

Use `make clean` before comparing platform-specific builds or after changing qmake `CONFIG` values. The application target is `iScanMR10`; avoid committing compiled binaries unless explicitly requested.

## Hardware and Platform Configuration

Hardware selection is controlled in `iScanMR10.pro` by enabling exactly one of `CONFIG+=HW_5G` or `CONFIG+=HW_NONE_5G`. Never enable both. The current safe default is `HW_NONE_5G`, which defines `HARDWARE_HAS_5G=0`, `HARDWARE_HAS_WIFI=0`, and `HARDWARE_HAS_WIRELESS=0`.

Platform selection is controlled by the qmake mkspec. `linux-g++` should define `PLATFORM_X86` behavior, while `linux-jetson-orin-g++` should define `PLATFORM_JETSON` behavior. Keep Jetson-only code, GPIO, recorder backend objects, and hardware-device access behind the existing platform guards so the desktop build continues to compile.

## Coding Style & Naming Conventions

Write C++ in the existing Qt style using `QObject`, signals/slots, `QString`, `QVariantMap`, `QJsonDocument`, and Qt containers where they already exist. Keep class names in `PascalCase`, method and variable names in `camelCase`, and existing file names unchanged unless a rename is requested. Prefer small helper functions over large repeated command blocks, especially in `NetworkController`.

QML components should keep the current project style: component filenames in `PascalCase.qml`, clear property names, and UI logic separated from system command logic. Keep `Wifi5GView.qml` mostly UI-focused; route real WiFi/5G actions through `Wifi5GController` and `NetworkController`. Do not duplicate `nmcli`, `mmcli`, rmnet, or quectel-CM parsing logic in QML.

## QML and C++ Communication Rules

QML sends commands mainly through `mainWindows.cppSubmitTextFiled(JSON.stringify(obj))`. C++ receives them in `Mainwindows::cppSubmitTextFiled(const QString &qmlJson)` and sends responses back with `emit cppCommand(QVariant)`. When adding a command, use a stable `menuID`, return useful `ok`, `message`, or error fields, and preserve existing response keys used by QML.

Do not log passwords, WiFi keys, VPN secrets, tokens, or private configuration values. For WiFi/5G features, `NetworkController` is the source of truth. `Wifi5GController` should remain a routing/adapter layer, not a second implementation of network control.

## Testing Guidelines

There is no dedicated automated test suite in this repository. Before submitting changes, at minimum build both the intended target and the unaffected target when practical. For UI changes, open the relevant QML page and verify that existing commands still return expected JSON. For network changes, test harmless read-only commands first, such as status, scan, profile list, modem list, or log tail, before applying changes that modify connections.

For hardware-specific work, document the target platform, active hardware config, tested command, expected result, and observed result. When changing 5G, WiFi, GPIO, SPI, I2C, ALSA, recorder, or OpenWebRX behavior, make sure `HW_NONE_5G` or desktop builds do not break from missing device files.

## Agent Safety Rules

Preserve existing behavior unless the requested task requires changing it. Prefer minimal patches that keep current menuIDs, QML object names, signal names, database fields, and config file paths compatible. Do not remove legacy wrapper components such as `Wifi5GSetting.qml` unless all references are updated. Do not edit generated SigmaStudio assets in `DesignDSP_REC_V1/` unless the DSP firmware data itself is intentionally being updated.

Before making broad changes, inspect `iScanMR10.pro`, `main.cpp`, the relevant QML page, and the matching C++ controller. After changing code, provide the exact files touched, build command used, and any hardware/runtime assumptions.