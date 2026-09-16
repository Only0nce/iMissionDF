#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/mute_state1_orig}"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

HOME="$ROOT/HomeDisplay.qml"
DRAWER="$ROOT/MyDrawer.qml"
RADIO="$ROOT/RadioScanner.qml"
VOL="$ROOT/VolDrawer.qml"
VOLUME="$ROOT/VolumeDrawer.qml"

for f in "$HOME" "$DRAWER" "$RADIO" "$VOL" "$VOLUME"; do [[ -f "$f" ]] || fail "missing $f"; done

grep -q 'target: wsClient' "$HOME" || fail "HomeDisplay wsClient Connections missing"
grep -q 'function onMutedChanged(muted)' "$HOME" || fail "HomeDisplay mutedChanged owner missing"
grep -q 'scanMuteOn = wsClient.muted' "$HOME" || fail "initial mute synchronization missing"
! grep -q 'function onMutedChanged' "$DRAWER" || fail "MyDrawer still owns backend mute readback"
pass "HomeDisplay is the single backend mute-state owner"

python3 - "$VOL" "$VOLUME" <<'PY'
import re,sys
for fn in sys.argv[1:]:
    s=open(fn,encoding='utf-8').read()
    s=re.sub(r'/\*.*?\*/','',s,flags=re.S)
    s=re.sub(r'//.*','',s)
    if re.search(r'\bmute\s*=\s*(?:true|false|!\s*mute|wsClient)',s):
        raise SystemExit(f'active child mute writer remains in {fn}')
print('[PASS] drawer mute properties are presentation-only')
PY

grep -q '? "Speaker\\nMute"' "$RADIO" || fail "Main Speaker mute label missing"
grep -q 'source: mute ? "images/speaker_mute.png" : "images/speaker.png"' "$VOL" || fail "VolDrawer mute icon binding missing"
grep -q 'source: mute ? "images/speaker_mute.png" : "images/speaker.png"' "$VOLUME" || fail "VolumeDrawer mute icon binding missing"
[[ -f "$ROOT/images/speaker_mute.png" && -f "$ROOT/images/speaker.png" ]] || fail "speaker image assets missing"
grep -q '<file>images/speaker_mute.png</file>' "$ROOT/qml.qrc" || fail "speaker mute asset not in qrc"
pass "Main label and speaker icon assets/bindings are present"

python3 - "$HOME" <<'PY'
import sys
s=open(sys.argv[1],encoding='utf-8').read()
def section(a,b):
    i=s.index(a); j=s.index(b,i); return s[i:j]
checks=[
 ('software volume', section('    onScanAudioLevelChanged:{','    // onCurrentLowcutChanged:')),
 ('speaker volume', section('    onScanVolLevelChanged: {','    onScanVolLevelHeadphoneChanged:')),
 ('headphone volume', section('    onScanVolLevelHeadphoneChanged: {','    onScanSqlLevelChanged:')),
]
for name,blk in checks:
    if 'setSpeakerVolumeMute(0)' in blk:
        raise SystemExit(f'{name} still forces unmute')
# Existing SQL policy must remain explicitly unchanged in this revision.
sql=section('    onScanSqlLevelChanged: {','    onGpiokeyProfileChanged:')
if sql.count('setSpeakerVolumeMute(0)') != 2:
    raise SystemExit('SQL-specific legacy unmute policy changed unexpectedly')
print('[PASS] volume changes no longer force unmute; SQL policy preserved')
PY

# Backend and network work must remain byte-identical.
for f in websocketclient.cpp websocketclient.h Setting.qml ServiceEndpointsPage.qml NetworkController.cpp NetworkController.h MainPage.qml; do
  if [[ -f "$BASE/$f" && -f "$ROOT/$f" ]]; then
    cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
  fi
done
pass "audio backend and network/endpoints implementation unchanged"

python3 - "$HOME" "$DRAWER" "$RADIO" "$VOL" "$VOLUME" <<'PY'
import sys
pairs={')':'(',']':'[','}':'{'}
for fn in sys.argv[1:]:
    s=open(fn,encoding='utf-8').read(); st=[]; i=0; q=None; line=False; block=False
    while i<len(s):
        c=s[i]; n=s[i+1] if i+1<len(s) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if q:
            if c=='\\': i+=2; continue
            if c==q: q=None
            i+=1; continue
        if c=='/' and n=='/': line=True; i+=2; continue
        if c=='/' and n=='*': block=True; i+=2; continue
        if c in ('"',"'"): q=c; i+=1; continue
        if c in '([{': st.append(c)
        elif c in ')]}':
            if not st or st[-1]!=pairs[c]: raise SystemExit(f'delimiter mismatch in {fn}')
            st.pop()
        i+=1
    if st or q or block: raise SystemExit(f'unbalanced structure in {fn}')
print('[PASS] modified QML delimiter validation')
PY

pass "R20.4 RADIO UX1.2 mute-state structural verification complete"
