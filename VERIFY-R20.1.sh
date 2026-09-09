#!/usr/bin/env bash
set -u
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
cd "$ROOT" || exit 2
fail=0
pass(){ printf '[PASS] %s\n' "$*"; }
failmsg(){ printf '[FAIL] %s\n' "$*" >&2; fail=$((fail+1)); }

need(){ [[ -f "$1" ]] && pass "file exists: $1" || failmsg "missing: $1"; }
for f in Mainwindows.cpp Mainwindows.h NetworkController.cpp NetworkController.h main.cpp iScanMR10.pro websocketclient.cpp alsaaudioplayer.cpp HomeDisplay.qml MyDrawer.qml VolumeDrawer.qml VolDrawer.qml CrashDiagnostics.cpp CrashDiagnostics.h; do need "$f"; done

grep -q 'suspendCellularRealtime();' Mainwindows.cpp && pass '5G reset suspends cellular realtime polling' || failmsg '5G suspend missing'
grep -q 'resumeCellularRealtime(false);' Mainwindows.cpp && pass '5G realtime resumes without immediate synchronous poll' || failmsg '5G resume missing'
grep -q 'startCellularRealtime deferred while reset is active' NetworkController.cpp && pass 'lte_state requests cannot restart polling during reset' || failmsg '5G suspend guard missing'

python3 - <<'PY' || exit 1
from pathlib import Path
s=Path('Mainwindows.cpp').read_text(errors='replace')
a=s.find('emit self->reset5GModemFinished(ready);')
b=s.find('resumeCellularRealtime(false);')
if a >= 0 and b > a:
    print('[PASS] reset completion is published before 5G polling resumes')
else:
    print('[FAIL] reset completion ordering is wrong')
    raise SystemExit(1)
PY

grep -q 'iScanMR10-5g-reset-debug.log' Mainwindows.cpp && pass 'failure debug is detached to a field log' || failmsg 'detached 5G failure debug missing'
if grep -A15 'reset5GModemNoRebootWorker done - service mode OK' Mainwindows.cpp | grep -q 'schedule5GDebugDump'; then
    failmsg 'success path still schedules heavy debug'
else
    pass 'successful 5G reset has no diagnostic tail delay'
fi

! grep -q 'ctrl\.ctrlLevel = v' HomeDisplay.qml && pass 'rotary no longer writes drawer speaker property' || failmsg 'speaker drawer multi-writer remains'
! grep -q 'ctrl\.headphoneCtrlLevel = v' HomeDisplay.qml && pass 'rotary no longer writes drawer headphone property' || failmsg 'headphone drawer multi-writer remains'
! grep -q 'ctrl\.audioLevel = v' HomeDisplay.qml && pass 'rotary no longer writes drawer software-volume property' || failmsg 'audio drawer multi-writer remains'

grep -q 'onMuteToggleRequested: root_drawerItem.toggleVolumeMute()' MyDrawer.qml && pass 'MyDrawer owns mute policy' || failmsg 'central mute owner missing'
! grep -q 'onMuteChanged:' MyDrawer.qml && pass 'drawer mute binding has no writeback loop' || failmsg 'mute writeback remains'
grep -q 'slider.onMoved:' MyDrawer.qml && grep -q 'slider2.onMoved:' MyDrawer.qml && pass 'volume writeback is user-movement-only' || failmsg 'Slider.moved hardening missing'

python3 - <<'PY' || exit 1
from pathlib import Path
for f in ['VolumeDrawer.qml','VolDrawer.qml']:
    s='\n'.join(x for x in Path(f).read_text().splitlines() if not x.strip().startswith('//'))
    if 'mute = !mute' in s:
        print('[FAIL]', f, 'still mutates externally-owned mute')
        raise SystemExit(1)
print('[PASS] child drawers are passive mute requesters')
PY

grep -q 'return requestedSpeakerVolume' Mainwindows.h && pass 'speaker getter reports requested state' || failmsg 'speaker getter/channel ownership wrong'
grep -q 'return requestedHeadphoneVolume' Mainwindows.h && pass 'headphone getter reports requested state' || failmsg 'headphone getter/channel ownership wrong'

grep -q 'CrashDiagnostics.cpp' iScanMR10.pro && grep -q 'CrashDiagnostics.h' iScanMR10.pro && pass 'CrashDiagnostics is in qmake graph' || failmsg 'CrashDiagnostics not in qmake graph'
grep -q 'DiagnosticGuiApplication app' main.cpp && pass 'Qt event-dispatch diagnostics active' || failmsg 'DiagnosticGuiApplication missing'
grep -q 'SA_SIGINFO' main.cpp && pass 'SIGTERM/SIGINT sender diagnostics active' || failmsg 'SA_SIGINFO termination diagnostics missing'
grep -q 'installFatalSignalHandlers' main.cpp && pass 'fatal signal diagnostics active' || failmsg 'fatal handler install missing'

grep -q 'CpWsAudioPostPush' websocketclient.cpp && grep -q 'CpAudioWrite' alsaaudioplayer.cpp && pass 'low-overhead audio crash breadcrumbs active' || failmsg 'audio crash breadcrumbs missing'

grep -q '^CONFIG += c++17$' iScanMR10.pro && ! grep -q '^CONFIG += c++11$' iScanMR10.pro && pass 'qmake language mode is unambiguous C++17' || failmsg 'conflicting C++ standard flags remain'

printf '\nR20.1 static verifier: %d failure(s)\n' "$fail"
(( fail == 0 ))
