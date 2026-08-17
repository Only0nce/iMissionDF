#!/bin/bash
# Verification helper; intentionally never calls exit.
BIN="${1:-./iScanMR10}"
REV="20260817-src-wifi-astrarx-r8"

echo "============================================================"
echo " AstraRX Qt5 R8 verification"
echo " Binary: $BIN"
echo "============================================================"

if [ ! -f "$BIN" ]; then
    echo "[VERIFY] binary not found: $BIN"
else
    if strings "$BIN" | grep -Fq "$REV"; then
        echo "PASS revision marker: $REV"
    else
        echo "FAIL revision marker missing"
    fi

    if strings "$BIN" | grep -Fq '[SetFreqWorker]'; then
        echo "FAIL legacy SetFreqWorker runtime strings are still present"
    else
        echo "PASS no legacy [SetFreqWorker] runtime strings"
    fi

    if strings "$BIN" | grep -Fq '30000000'; then
        echo "WARN literal 30000000 exists somewhere in binary; inspect whether unrelated"
    else
        echo "PASS no legacy 30 MHz reset literal"
    fi

    if strings "$BIN" | grep -Fq '[QT5-SQUELCH-RX]'; then
        echo "PASS explicit AstraRX squelch handler present"
    else
        echo "FAIL explicit squelch marker missing"
    fi

    if strings "$BIN" | grep -Fq '[QML-FREQ-SYNC]'; then
        echo "PASS receiver QML sync marker embedded"
    else
        echo "FAIL QML receiver sync marker missing; regenerate qrc_qml.cpp"
    fi
fi
