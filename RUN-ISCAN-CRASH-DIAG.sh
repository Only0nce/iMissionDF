#!/usr/bin/env bash
# Run iScanMR10 in the foreground, preserve core dumps, and print the actual
# process exit code plus the most useful crash evidence. No log archive is made.

BIN="${1:-/opt/iScanMR10/bin/iScanMR10}"

ulimit -c unlimited 2>/dev/null || true

printf '\n[DIAG] binary: %s\n' "$BIN"
printf '[DIAG] core limit: '
ulimit -c
printf '[DIAG] core pattern: '
cat /proc/sys/kernel/core_pattern 2>/dev/null || true
printf '\n[DIAG] starting foreground process...\n\n'

"$BIN"
rc=$?

printf '\n============================================================\n'
printf '[DIAG] iScanMR10 EXIT CODE = %s\n' "$rc"
case "$rc" in
  134) printf '[DIAG] meaning: SIGABRT / abort / assertion / heap corruption\n' ;;
  135) printf '[DIAG] meaning: SIGBUS\n' ;;
  137) printf '[DIAG] meaning: SIGKILL (check OOM/external kill)\n' ;;
  139) printf '[DIAG] meaning: SIGSEGV\n' ;;
  143) printf '[DIAG] meaning: SIGTERM\n' ;;
  *)   printf '[DIAG] meaning: inspect core/kernel/user-space output\n' ;;
esac
printf '============================================================\n\n'

printf '[DIAG] matching kernel faults (grep before tail):\n'
if dmesg >/dev/null 2>&1; then
  dmesg -T 2>/dev/null | grep -Ei 'iScanMR10|segfault|oom|out of memory|killed process|abort|bus error|NVRM|Xid|gpu fault' | tail -n 120 || true
else
  printf '  dmesg requires privilege. Run:\n'
  printf "  sudo dmesg -T | grep -Ei 'iScanMR10|segfault|oom|out of memory|killed process|abort|bus error|NVRM|Xid|gpu fault' | tail -n 120\n"
fi

printf '\n[DIAG] coredumpctl latest match:\n'
if command -v coredumpctl >/dev/null 2>&1; then
  coredumpctl list --no-pager 2>/dev/null | grep -i 'iScanMR10' | tail -n 5 || true
  printf '\n[DIAG] if a core exists, run:\n'
  printf '  coredumpctl info iScanMR10\n'
  printf '  coredumpctl gdb iScanMR10\n'
  printf '  then in gdb: thread apply all bt full\n'
else
  printf '  coredumpctl is not installed. Inspect core_pattern above for core file location.\n'
fi
