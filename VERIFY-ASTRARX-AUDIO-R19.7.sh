#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
cd "$ROOT"

pass=0
fail=0
check() {
    local label="$1"; shift
    if "$@"; then
        printf 'PASS  %s\n' "$label"
        pass=$((pass+1))
    else
        printf 'FAIL  %s\n' "$label"
        fail=$((fail+1))
    fi
}

check "R19.7 startup marker" grep -q 'R19.7 ASTRARX AUDIO' Mainwindows.cpp
check "Qt5 AstraRX endpoint" grep -q '127.0.0.1:8074/ws/qt5' Mainwindows.cpp
check "single 48k ALSA owner" grep -q 'new AlsaAudioPlayer(48000' websocketclient.cpp
check "digital type4 16k input" grep -q 'pushAudioAtRate(data, 16000)' websocketclient.cpp
check "40 ms low-latency default" grep -q 'ISCAN_AUDIO_BUFFER_MS.*, 40' alsaaudioplayer.cpp
check "10 ms period default" grep -q 'ISCAN_AUDIO_PERIOD_MS.*, 10' alsaaudioplayer.cpp
check "20 ms staging default" grep -q 'ISCAN_AUDIO_STAGE_MS.*, 20' alsaaudioplayer.cpp
check "ALSA start threshold" grep -q 'snd_pcm_sw_params_set_start_threshold' alsaaudioplayer.cpp
check "ALSA delay telemetry" grep -q 'R19.7 QT AUDIO LATENCY' alsaaudioplayer.cpp

if grep -q 'QThread::msleep(remaining)' alsaaudioplayer.cpp; then
    printf 'FAIL  legacy post-write manual pacing still active\n'
    fail=$((fail+1))
else
    printf 'PASS  no legacy post-write manual pacing\n'
    pass=$((pass+1))
fi

printf '\nStatic result: %d PASS / %d FAIL\n' "$pass" "$fail"

if command -v systemctl >/dev/null 2>&1; then
    printf '\n===== Optional runtime preflight =====\n'
    if systemctl is-active --quiet astrarx.service 2>/dev/null; then
        echo 'PASS  astrarx.service active'
    else
        echo 'INFO  astrarx.service not active/not available in this environment'
    fi
fi

if command -v ss >/dev/null 2>&1 && ss -ltn 2>/dev/null | grep -q ':8074'; then
    echo 'PASS  TCP/8074 listening'
else
    echo 'INFO  TCP/8074 not observed in this environment'
fi

if command -v aplay >/dev/null 2>&1; then
    echo
    echo '===== ALSA playback devices ====='
    aplay -l 2>/dev/null || true
fi

(( fail == 0 ))
