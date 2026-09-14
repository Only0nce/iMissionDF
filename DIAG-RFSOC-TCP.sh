#!/bin/bash
set -u
HOST="${1:-}"
PORT="${2:-5555}"
if [[ -z "$HOST" ]]; then
  echo "Usage: $0 <RFSoC-management-IP> [port]"
  echo "Example: $0 192.168.10.5 5555"
  exit 2
fi

echo "===== RFSoC TCP PATH DIAGNOSTIC ====="
echo "Target: $HOST:$PORT"
echo

echo "--- route ---"
ip route get "$HOST" 2>&1 || true
echo

echo "--- ping ---"
ping -c 3 -W 1 "$HOST" 2>&1 || true
echo

echo "--- tcp port ---"
if command -v nc >/dev/null 2>&1; then
  nc -vz -w 2 "$HOST" "$PORT" 2>&1 || true
else
  echo "nc is not installed; skipping TCP connect probe"
fi

echo
cat <<'TXT'
Interpretation:
  succeeded / open       -> network path + RFSoC TCP server are reachable
  Connection refused     -> host reachable, but nothing is listening on that port
  timed out              -> routing/firewall/link/host reachability problem
  Network is unreachable -> local route/interface problem

On RFSoC, verify the server side with:
  ss -lntp | grep ':5555'
TXT
