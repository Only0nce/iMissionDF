#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
cd "$ROOT"
pass=0; fail=0
check(){ local n="$1"; shift; if "$@"; then echo "PASS  $n"; pass=$((pass+1)); else echo "FAIL  $n"; fail=$((fail+1)); fi; }
check "40 ms audio buffer default" grep -q 'ISCAN_AUDIO_BUFFER_MS.*, 40' alsaaudioplayer.cpp
check "10 ms audio period default" grep -q 'ISCAN_AUDIO_PERIOD_MS.*, 10' alsaaudioplayer.cpp
check "20 ms staging default" grep -q 'ISCAN_AUDIO_STAGE_MS.*, 20' alsaaudioplayer.cpp
check "bounded 8 chunk default" grep -q 'ISCAN_AUDIO_MAX_QUEUE_CHUNKS.*, 8' alsaaudioplayer.cpp
check "aggregated live-edge diagnostic" grep -q '\[AUDIO-LIVE-EDGE\]' alsaaudioplayer.cpp
check "old per-packet overflow warning removed" bash -c "! grep -q 'Audio queue overflow - dropped' alsaaudioplayer.cpp"
check "QML undefined row guard" grep -q 'if (!it) return false' iScreenDFqml/pages/QMLMap.qml
check "QML timestamp numeric guard" grep -q 'updated <= 0' iScreenDFqml/pages/QMLMap.qml
printf '\nStatic result: %d PASS / %d FAIL\n' "$pass" "$fail"
(( fail == 0 ))
