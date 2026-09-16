#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-/mnt/data/rec_state1_orig}"
fail(){ echo "[FAIL] $*" >&2; exit 1; }
pass(){ echo "[PASS] $*"; }

for f in HomeDisplay.qml MyDrawer.qml RadioScanner.qml Mainwindows.h Mainwindows.cpp websocketclient.cpp; do
  [[ -f "$ROOT/$f" ]] || fail "$f missing"
done

python3 - "$ROOT" <<'PY'
import re, sys
from pathlib import Path
root=Path(sys.argv[1])

def func_block(text, name):
    m=re.search(r'function\s+'+re.escape(name)+r'\s*\([^)]*\)\s*\{', text)
    if not m: raise SystemExit(f'missing function {name}')
    i=m.end()-1; depth=0
    for j in range(i, len(text)):
        if text[j]=='{': depth+=1
        elif text[j]=='}':
            depth-=1
            if depth==0: return text[m.start():j+1]
    raise SystemExit(f'unclosed function {name}')

home=(root/'HomeDisplay.qml').read_text()
drawer=(root/'MyDrawer.qml').read_text()
for label, block in [
    ('HomeDisplay.unmuteFromUserVolumeAdjustment', func_block(home,'unmuteFromUserVolumeAdjustment')),
    ('MyDrawer.toggleVolumeMute', func_block(drawer,'toggleVolumeMute')),
]:
    for forbidden in ('setSqlLevel', 'setSqlOffManual', 'squelch_level', 'sendmessage'):
        if forbidden in block:
            raise SystemExit(f'{label} still mutates SQL via {forbidden}')
PY
pass "mute/unmute transaction is playback-only"

grep -q 'Q_INVOKABLE bool getRecActive() const;' "$ROOT/Mainwindows.h" || fail "getRecActive declaration missing"
grep -q 'Q_INVOKABLE QString getRecorderState() const;' "$ROOT/Mainwindows.h" || fail "getRecorderState declaration missing"
grep -q 'bool Mainwindows::getRecActive() const' "$ROOT/Mainwindows.cpp" || fail "getRecActive definition missing"
grep -q 'QString Mainwindows::getRecorderState() const' "$ROOT/Mainwindows.cpp" || fail "getRecorderState definition missing"
grep -q '\[REC-STATE\]' "$ROOT/Mainwindows.cpp" || fail "REC state diagnostic missing"
grep -q '\[SQL-STATE\]' "$ROOT/Mainwindows.cpp" || fail "SQL state diagnostic missing"
grep -q '\[AUDIO-MUTE\]' "$ROOT/websocketclient.cpp" || fail "audio mute diagnostic missing"
pass "recorder/SQL/audio diagnostics and replay APIs present"

grep -q 'mainWindows.getRecActive()' "$ROOT/RadioScanner.qml" || fail "REC icon does not replay recorder state"
grep -q 'function onOnRecStatusChanged(active)' "$ROOT/RadioScanner.qml" || fail "REC icon does not follow recorder signal"
if grep -q 'getSqlActive()' "$ROOT/RadioScanner.qml"; then fail "RadioScanner REC still reads SQL state"; fi
if grep -q 'function onSqlActiveChanged' "$ROOT/RadioScanner.qml"; then fail "RadioScanner REC still follows SQL signal"; fi
pass "REC indicator follows actual recorder state, not SQL"

# Playback level handlers must not replay SQL/squelch.
python3 - "$ROOT/HomeDisplay.qml" <<'PY'
import re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
for start,end in [
    ('onScanAudioLevelChanged:', 'onScanReceiverModeSelectedChanged:'),
]:
    pass
# Direct bounded checks around the three handlers.
patterns=[
    r'onScanAudioLevelChanged\s*:\s*\{(.*?)\n\s*\}',
    r'onScanVolLevelChanged\s*:\s*\{(.*?)\n\s*\}\n\n\s*onScanVolLevelHeadphoneChanged',
    r'onScanVolLevelHeadphoneChanged\s*:\s*\{(.*?)\n\s*\}\n\n\s*onScanSqlLevelChanged',
]
for pat in patterns:
    m=re.search(pat,s,re.S)
    if not m: raise SystemExit('volume handler block not found')
    b=m.group(1)
    for bad in ('setSqlLevel','setSqlOffManual','squelch_level'):
        if bad in b: raise SystemExit(f'volume handler still contains {bad}')
PY
pass "Volume/Speaker/Headphone handlers no longer replay SQL"

# Files outside the intended product scope must remain exact.
for f in Setting.qml ServiceEndpointsPage.qml NetworkController.cpp NetworkController.h MainPage.qml; do
  [[ -f "$BASE/$f" ]] || continue
  cmp -s "$BASE/$f" "$ROOT/$f" || fail "$f changed unexpectedly"
done
pass "network/endpoints integration unchanged"

# Lightweight delimiter check for modified QML files.
python3 - "$ROOT/HomeDisplay.qml" "$ROOT/MyDrawer.qml" "$ROOT/RadioScanner.qml" <<'PY'
import sys
from pathlib import Path
for fn in sys.argv[1:]:
    s=Path(fn).read_text(); stack=[]; pairs={'}':'{',')':'(',']':'['}; opens=set(pairs.values())
    quote=None; line=False; block=False; i=0
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
        if c in ('"',"'"): quote=c; i+=1; continue
        if c in opens: stack.append(c)
        elif c in pairs:
            if not stack or stack[-1]!=pairs[c]: raise SystemExit(f'{fn}: delimiter mismatch at {i}')
            stack.pop()
        i+=1
    if stack: raise SystemExit(f'{fn}: unclosed delimiter(s) {stack[-8:]}')
PY
pass "modified QML delimiter validation"

pass "R20.4 RADIO UX1.4 structural verification complete"
