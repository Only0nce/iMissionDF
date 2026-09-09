#!/bin/sh

LOG="${ISCAN_DIAG_LOG:-/tmp/iScanMR10-debug.log}"

echo "============================================================"
echo " iScanMR10 R13 crash diagnostic check"
echo "============================================================"
echo "LOG=$LOG"
echo

if [ -f "$LOG" ]; then
    echo "===== LAST 250 DIAGNOSTIC LINES ====="
    tail -n 250 "$LOG"
    echo
    echo "===== CRASH / TERMINATION / HEALTH MARKERS ====="
    grep -E 'R13-FATAL|R13-TERM|atexit|heartbeat|event-loop-delay|thread wait|queue drop|disconnect|worker' "$LOG" | tail -n 200
else
    echo "Diagnostic log not found: $LOG"
fi

echo
echo "===== KERNEL OOM / SEGFAULT / GPU SUMMARY ====="
sudo dmesg -T 2>/dev/null | grep -Ei 'iScanMR10|segfault|oom|out of memory|killed process|abort|bus error|NVRM|Xid|gpu fault' | tail -n 120

echo
echo "===== COREDUMP SUMMARY ====="
coredumpctl list 2>/dev/null | grep -i iScanMR10 | tail -n 20

echo
echo "===== CURRENT PROCESS (if still alive) ====="
pgrep -af iScanMR10
