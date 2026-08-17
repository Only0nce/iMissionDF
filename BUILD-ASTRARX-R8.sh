#!/bin/bash
# Intentionally does not use `set -e` or `exit`; suitable for the project's no-exit workflow.

echo "============================================================"
echo " AstraRX Qt5 R8 clean build"
echo " Revision: 20260817-src-wifi-astrarx-r8"
echo "============================================================"

if ! command -v qmake >/dev/null 2>&1; then
    echo "[BUILD] qmake not found. Install/use the target Qt5 build environment."
else
    if [ ! -d iScreenDF ] || [ ! -d iRecordManage ] || [ ! -d DoaViewer ]; then
        echo "[BUILD] WARNING: this directory does not look like the complete product tree."
        echo "[BUILD] Overlay R8 onto the complete source tree before building."
    fi

    if [ -f Makefile ]; then
        make clean
    fi

    rm -f qrc_qml.cpp qrc_qml.o
    rm -f moc_*.cpp moc_*.o

    qmake iScanMR10.pro
    make -j"$(nproc)"

    echo "[BUILD] finished; run VERIFY-ASTRARX-R8.sh against the produced binary."
fi
