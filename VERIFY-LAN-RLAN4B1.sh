#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CPP="$ROOT/iScreenDF/iScreenDF.cpp"
TCP="$ROOT/iScreenDF/TcpClientDF.cpp"
FUNC="$ROOT/iScreenDF/functionTcpServer.cpp"
DB="$ROOT/iScreenDF/DatabaseDF.cpp"

fail(){ echo "[FAIL] $*"; exit 1; }
pass(){ echo "[PASS] $*"; }

start_line=$(grep -n 'dbThread->start();' "$CPP" | cut -d: -f1 | head -1)
ip_signal_line=$(grep -n 'DatabaseDF::GetIPDFServer' "$CPP" | cut -d: -f1 | head -1)
tcp_handler_line=$(grep -n 'TcpClientDF::updateFromTcpServer' "$CPP" | cut -d: -f1 | head -1)
[[ -n "$start_line" && -n "$ip_signal_line" && -n "$tcp_handler_line" ]] || fail "startup markers missing"
(( start_line > ip_signal_line )) || fail "DB thread starts before GetIPDFServer signal wiring"
(( start_line > tcp_handler_line )) || fail "DB thread starts before TcpClientDF handlers are wired"
pass "DB worker starts only after RFSoC startup signal wiring"

grep -q 'Parameter.ipdfserver from DB' "$DB" || fail "DB target logging missing"
grep -q 'control target from DB' "$FUNC" || fail "GetIPDFServer target logging missing"
grep -q 'applying TCP target independently' "$FUNC" || fail "GetIPDFServer still depends on RF parameter timing"
pass "Parameter.ipdfserver is applied independently and visibly"

grep -q '\[LAN\]\[RFSoC-TCP\] connectToHost' "$TCP" || fail "connect attempt logging missing"
grep -q '\[LAN\]\[RFSoC-TCP\] socket error' "$TCP" || fail "socket error logging missing"
pass "RFSoC TCP connect/error diagnostics present"

grep -q 'obj\["menuID"\]  = "setIpConfig"' "$ROOT/iScreenDF/functionTcpServer.cpp" || fail "setIpConfig contract missing"
grep -q 'iface != QStringLiteral("end0") && iface != QStringLiteral("end1")' "$ROOT/iScreenDF/functionTcpServer.cpp" || fail "end0/end1 guard missing"
pass "legacy end0/end1 setIpConfig contract retained"

echo "[PASS] R-LAN4B.1 structural verification complete"
