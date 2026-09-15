#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/vpn2_work}"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

QML="$ROOT/VpnPage.qml"
CPP="$ROOT/NetworkController.cpp"
HDR="$ROOT/NetworkController.h"

[[ -f "$QML" && -f "$CPP" && -f "$HDR" ]] || fail "VPN files missing"

grep -q 'text: "PUBLIC IP"' "$QML" || fail "Public IP tile missing"
grep -q 'text: "STATUS"' "$QML" || fail "Status tile missing"
grep -q 'text: "VPN ENABLE"' "$QML" || fail "VPN enable tile missing"
grep -q 'text: vpnEnabled ? "Disable VPN" : "Enable VPN"' "$QML" || fail "Enable/Disable VPN control missing"
grep -q 'text:.*"Refresh"' "$QML" || fail "Refresh control missing"
grep -q 'text: "Connect"' "$QML" || fail "Connect control missing"
grep -q 'text: "Disconnect"' "$QML" || fail "Disconnect control missing"
grep -q 'NetworkController.requestVpnPublicIp()' "$QML" || fail "Public IP request not wired"
grep -q 'NetworkController.setVpnEnabled(targetEnabled)' "$QML" || fail "VPN enable action not wired"
grep -q 'NetworkController.setVpnConnectionActive' "$QML" || fail "VPN connect/disconnect backend not wired"
pass "required VPN control-panel UX is present"

grep -q 'requestVpnPublicIp' "$HDR" || fail "public-IP API missing in header"
grep -q 'setVpnEnabled' "$HDR" || fail "VPN enable API missing in header"
grep -q 'vpnPublicIpReady' "$HDR" || fail "public-IP signal missing"
grep -q 'vpnEnableFinished' "$HDR" || fail "VPN enable result signal missing"
grep -q 'https://api.ipify.org' "$CPP" || fail "public-IP HTTPS endpoint missing"
grep -q 'vpnFeatureEnabledFromConfig' "$CPP" || fail "VPN persisted policy helper missing"
grep -q 'vpn\[QStringLiteral("enabled")\] = enabled' "$CPP" || fail "VPN enable persistence missing"
grep -q 'VPN is disabled. Enable VPN before connecting a profile.' "$CPP" || fail "backend connect guard missing"
pass "VPN enable/public-IP backend contract is present"

python3 - "$QML" "$CPP" "$HDR" <<'PY'
import sys
from pathlib import Path

def balanced(path):
    text=Path(path).read_text(errors='replace')
    stack=[]
    pairs={')':'(',']':'[','}':'{'}
    opens=set(pairs.values())
    i=0; quote=None; line=False; block=False; esc=False
    while i < len(text):
        c=text[i]; n=text[i+1] if i+1<len(text) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in opens: stack.append(c)
        elif c in pairs:
            if not stack or stack[-1] != pairs[c]:
                raise SystemExit(f"unbalanced delimiter in {path} at offset {i}")
            stack.pop()
        i+=1
    if stack or quote or block:
        raise SystemExit(f"unterminated structure in {path}")

for p in sys.argv[1:]: balanced(p)
PY
pass "lightweight QML/C++ delimiter validation"

if [[ -d "$BASE" ]]; then
  for f in Setting.qml NetworkAccessModePopup.qml Mainwindows.cpp iScreenDF/DatabaseDF.cpp iScreenDF/functionTcpServer.cpp iScreenDF/functionMonitor.cpp; do
    if [[ -f "$BASE/$f" && -f "$ROOT/$f" ]]; then
      cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
    fi
  done
  pass "unrelated Network/LAN/RFSoC integration files unchanged from NET-VPN2"
fi

pass "NET-VPN2.1 structural verification complete"
