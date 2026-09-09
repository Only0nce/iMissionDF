# iScanMR10 / PLCServer Source-Ownership Recovery

## Root cause

The supplied tree contains two independent qmake applications in one directory:

- `iScanMR10.pro` — Qt Quick/QML iScanMR10 application.
- `PLCServer.pro` — standalone `QCoreApplication` PLC server.

The root `main.cpp` had been overwritten by the PLCServer entry point. qmake therefore compiled an iScan target whose `main.o` constructed `PLCServer`, while `iScanMR10.pro` correctly did not link the PLCServer implementation. That produced the original `undefined reference to PLCServer::PLCServer(QObject*)` / destructor linker error.

Adding `PLCServer.cpp` and `Function*.cpp` to `iScanMR10.pro` was not the correct fix; it merged two application targets and exposed unrelated legacy PLCServer compile errors such as `Function.cpp:334`.

## Recovery changes

1. `iScanMR10.pro` is restored to the iScan-only source graph. Standalone PLCServer files are not linked into iScanMR10.
2. `PLCServer.pro` now uses `plcserver_main.cpp`, so the PLC server has its own entry point and can no longer overwrite/consume the iScan `main.cpp`.
3. `main.cpp` is reconstructed as the iScan Qt/QML entry point using contracts already present in R10/R13/R16/R16.2/R18/R19.2.1:
   - `DiagnosticGuiApplication` / Qt event-dispatch diagnostics.
   - SIGTERM/SIGINT nonblocking-pipe bridge with sender attribution.
   - `qmlRegisterType<FftDisplayItem>("iScan.Display", 1, 0, "FftDisplayItem")` before QML load.
   - `mainWindows`, `wsClient`, `NetworkController` / `networkController` context objects.
   - explicit destruction of `QQmlApplicationEngine` before C++ context backends.
4. `Function.cpp` is restored to the supplied pre-recovery version. Its Qt5 `QJsonValue == NULL` issue is a PLCServer issue and is intentionally not used to make iScanMR10 build.

## Important source-completeness warning

The uploaded source archive itself is an overlay/incomplete source snapshot. `iScanMR10.pro` references external directories such as:

- `iRecordManage/`
- `iScreenDF/`
- `DoaViewer/`

which are not all present in the uploaded archive. The normal development tree at `/home/only/Pictures/iSense` must contain those files before a clean/release build can be claimed.

The recorder-domain `qmlCommand` signal is relayed to the existing `Mainwindows::commandMainCppToRecCpp(QString)` bridge. The actual receiver belongs to the external `iRecordManage` source set; this recovery does not invent a replacement API for missing external sources.

## Apply safely

Apply the recovery overlay to the complete project tree. Do not replace the complete project directory with the incomplete archive.

Then run:

```bash
cd /home/only/Pictures/iSense
./VERIFY-ISCAN-RECOVERY.sh .
```

Resolve any **FAIL** before qmake. Missing external files and duplicate MOC basenames are shown explicitly as warnings/integrity findings.

## Clean build rule

Do not reuse the root `Makefile`, `main.o`, `moc_*`, or another revision's build directory.

Use Qt Creator **Run qmake → Clean/Rebuild**, or create a fresh build directory with the same Jetson kit/mkspec used by the project.

The first expected result of this recovery is that `iScanMR10` compiles its Qt/QML `main.cpp`; it must no longer request `PLCServer::PLCServer(QObject*)` from `main.o`.
