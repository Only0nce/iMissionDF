#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/mute_state2_orig}"
H="$ROOT/HomeDisplay.qml"
D="$ROOT/MyDrawer.qml"

fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

# State owner / helper
grep -q 'property bool scanMuteOn: false' "$H" || fail "HomeDisplay mute owner missing"
grep -q 'function unmuteFromUserVolumeAdjustment()' "$H" || fail "user-adjustment unmute helper missing"
grep -q 'wsClient.setSpeakerVolumeMute(0)' "$H" || fail "backend unmute request missing"
grep -q 'function onMutedChanged(muted)' "$H" || fail "backend mute readback missing"
pass "HomeDisplay remains authoritative mute-state owner"

# Rotary: two Speaker branches + two Volume branches.
[[ $(grep -c 'unmuteFromUserVolumeAdjustment()' "$H") -eq 5 ]] || fail "expected helper definition + four rotary calls"
pass "Speaker/Volume rotary adjustments clear mute"

# Sliders: two drawer modes each expose Volume + Speaker movement.
[[ $(grep -c 'unmuteFromUserVolumeAdjustment()' "$D") -eq 4 ]] || fail "expected four slider unmute calls"
# Headphone blocks must not call the helper.
python3 - "$D" <<'PY'
import sys,re
s=open(sys.argv[1]).read()
for m in re.finditer(r'volumeHeadphoneCtrlLevel\.slider\.onMoved\s*:\s*\{(.*?)\n\s*\}', s, re.S):
    if 'unmuteFromUserVolumeAdjustment' in m.group(1):
        raise SystemExit('Headphone adjustment unexpectedly clears speaker mute')
PY
pass "Volume/Speaker sliders clear mute; Headphone remains independent"

# Ensure child drawers still do not own/write mute state.
! grep -Eq '(^|[^A-Za-z0-9_])mute\s*=\s*(true|false|!)' "$D" || fail "MyDrawer directly writes mute state"
pass "child drawer does not directly overwrite mute state"

# Backend/network scope unchanged.
for f in websocketclient.cpp websocketclient.h Setting.qml ServiceEndpointsPage.qml NetworkController.cpp NetworkController.h MainPage.qml; do
  if [[ -f "$BASE/$f" && -f "$ROOT/$f" ]]; then
    cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
  fi
done
pass "audio backend and network/endpoints implementation unchanged"

# Lightweight QML structural scan.
python3 - "$H" "$D" <<'PY'
import sys
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=open(fn).read(); stack=[]; i=0; quote=None; line=False; block=False
    while i<len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if c=='\\': i+=2; continue
            if c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"', "'"): quote=c; i+=1; continue
        if c in '([{': stack.append(c)
        elif c in ')]}':
            if not stack or stack[-1] != pairs[c]:
                raise SystemExit(f'{fn}: delimiter mismatch at offset {i}')
            stack.pop()
        i+=1
    if stack or quote or block:
        raise SystemExit(f'{fn}: unterminated QML structure')
print('[PASS] modified QML delimiter validation')
PY

pass "R20.4 RADIO UX1.3 structural verification complete"
