#!/usr/bin/env bash
set -u
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
pass=0
fail=0
check() {
    local desc="$1"; shift
    if "$@"; then
        printf 'PASS  %s\n' "$desc"
        pass=$((pass+1))
    else
        printf 'FAIL  %s\n' "$desc"
        fail=$((fail+1))
    fi
}
contains() { grep -qE "$1" "$2"; }
not_contains() { ! grep -qE "$1" "$2"; }

check "R15 build revision" contains 'volume-qml-state-hardening-r15' "$ROOT/Mainwindows.cpp"
check "R15 QML revision" contains 'volume-qml-state-hardening-r15' "$ROOT/HomeDisplay.qml"
check "speaker requested state exists" contains 'requestedSpeakerVolume' "$ROOT/Mainwindows.h"
check "headphone requested state exists" contains 'requestedHeadphoneVolume' "$ROOT/Mainwindows.h"
check "speaker setter writes CH4" contains 'VolumeOutCH4 = static_cast<unsigned char>\(effective\)' "$ROOT/Mainwindows.cpp"
check "headphone setter writes CH2" contains 'VolumeOutCH2 = static_cast<unsigned char>\(bounded\)' "$ROOT/Mainwindows.cpp"
check "speaker legacy threshold preserved" contains '\(bounded < 165\) \? 50 : bounded' "$ROOT/Mainwindows.cpp"
check "speaker getter returns requested state" contains 'getSpeakerVolume1\(\).*requestedSpeakerVolume' "$ROOT/Mainwindows.h"
check "headphone getter returns requested state" contains 'getHeadphoneVolume\(\).*requestedHeadphoneVolume' "$ROOT/Mainwindows.h"
check "rotary direct ctrlLevel write removed" not_contains 'ctrl\.ctrlLevel[[:space:]]*=' "$ROOT/HomeDisplay.qml"
check "rotary direct audioLevel write removed" not_contains 'ctrl\.audioLevel[[:space:]]*=' "$ROOT/HomeDisplay.qml"
check "rotary direct headphone write removed" not_contains 'ctrl\.headphoneCtrlLevel[[:space:]]*=' "$ROOT/HomeDisplay.qml"
check "manual rotary signal connection removed" not_contains 'updateRotaryProfiles\.connect' "$ROOT/HomeDisplay.qml"
check "declarative rotary connection present" contains 'function onUpdateRotaryProfiles' "$ROOT/HomeDisplay.qml"
check "audio drawer uses user moved signal" contains 'slider2\.onMoved' "$ROOT/MyDrawer.qml"
check "hardware drawer uses user moved signal" contains 'slider\.onMoved' "$ROOT/MyDrawer.qml"
check "volume drawer valueChanged writeback removed" not_contains 'audioCtrl>>' "$ROOT/MyDrawer.qml"
check "VolumeDrawer requests mute" contains 'signal muteToggleRequested' "$ROOT/VolumeDrawer.qml"
check "VolDrawer requests mute" contains 'signal muteToggleRequested' "$ROOT/VolDrawer.qml"
check "VolumeDrawer no active local mute assignment" bash -lc "! grep -Ev '^[[:space:]]*//' '$ROOT/VolumeDrawer.qml' | grep -qE 'mute[[:space:]]*=[[:space:]]*!?mute|mute[[:space:]]*=[[:space:]]*false|mute[[:space:]]*=[[:space:]]*m'"
check "VolDrawer no active local mute assignment" bash -lc "! grep -Ev '^[[:space:]]*//' '$ROOT/VolDrawer.qml' | grep -qE 'mute[[:space:]]*=[[:space:]]*!?mute|mute[[:space:]]*=[[:space:]]*false|mute[[:space:]]*=[[:space:]]*m'"
check "single wsClient muted connection in MyDrawer" contains 'function onMutedChanged\(m\)' "$ROOT/MyDrawer.qml"
check "volume checkpoint enum" contains 'CpVolumeRotary = 1000' "$ROOT/CrashDiagnostics.h"
check "volume checkpoint names" contains 'VOLUME_HARDWARE' "$ROOT/CrashDiagnostics.cpp"
check "frequency QML R14 marker retained" contains 'frequency-transaction-hardening-r14' "$ROOT/SpectrumGLPlot.qml"

python3 - "$ROOT" <<'PY'
from pathlib import Path
import re,sys
root=Path(sys.argv[1])
files=[root/'HomeDisplay.qml',root/'MyDrawer.qml',root/'VolumeDrawer.qml',root/'VolDrawer.qml',root/'Mainwindows.cpp',root/'Mainwindows.h',root/'CrashDiagnostics.cpp',root/'CrashDiagnostics.h']

def strip(text):
    out=[]; i=0; n=len(text); state='code'; quote=''
    while i<n:
        c=text[i]; d=text[i+1] if i+1<n else ''
        if state=='code':
            if c=='/' and d=='/': state='line'; out.extend('  '); i+=2; continue
            if c=='/' and d=='*': state='block'; out.extend('  '); i+=2; continue
            if c in ('"', "'"): state='str'; quote=c; out.append(' '); i+=1; continue
            out.append(c); i+=1; continue
        if state=='line':
            if c=='\n': state='code'; out.append('\n')
            else: out.append(' ')
            i+=1; continue
        if state=='block':
            if c=='*' and d=='/': state='code'; out.extend('  '); i+=2
            else: out.append('\n' if c=='\n' else ' '); i+=1
            continue
        if state=='str':
            if c=='\\': out.extend('  '); i+=2; continue
            if c==quote: state='code'; out.append(' '); i+=1; continue
            out.append('\n' if c=='\n' else ' '); i+=1
    return ''.join(out)

ok=True
for p in files:
    t=strip(p.read_text(errors='replace'))
    for left,right,name in [('{','}','brace'),('(',')','paren'),('[',']','bracket')]:
        depth=0
        for ch in t:
            if ch==left: depth+=1
            elif ch==right:
                depth-=1
                if depth<0:
                    print(f'FAIL  {p.name} {name} closes before open')
                    ok=False; break
        if depth!=0:
            print(f'FAIL  {p.name} {name} balance={depth}')
            ok=False
if ok:
    print('PASS  structural delimiter balance for changed source files')
sys.exit(0 if ok else 1)
PY
rc=$?
if [ "$rc" -eq 0 ]; then pass=$((pass+1)); else fail=$((fail+1)); fi

printf '\nTOTAL PASS=%d FAIL=%d\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
