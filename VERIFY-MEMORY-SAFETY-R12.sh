#!/bin/bash
# R12 static contract checks for iScanMR10 memory/lifetime hardening.
# Intentionally does not call `exit`; it prints PASS/FAIL so it is safe to use
# in the user's no-exit maintenance workflow.

ROOT="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0

check() {
    local name="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        printf '[PASS] %s\n' "$name"
        PASS=$((PASS + 1))
    else
        printf '[FAIL] %s\n' "$name"
        FAIL=$((FAIL + 1))
    fi
}

check_absent() {
    local name="$1"
    local pattern="$2"
    shift 2
    if ! grep -R -n -E "$pattern" "$@" >/dev/null 2>&1; then
        printf '[PASS] %s\n' "$name"
        PASS=$((PASS + 1))
    else
        printf '[FAIL] %s\n' "$name"
        FAIL=$((FAIL + 1))
    fi
}

cd "$ROOT" || return 0 2>/dev/null || true

printf 'iScanMR10 R12 memory/lifetime safety verification\n'
printf '================================================\n'

check 'R12 build marker present' grep -q '20260817-memory-lifetime-safety-r12' Mainwindows.cpp
check 'No active libc exit() in available C/C++ source' python3 -c 'from pathlib import Path; import re; bad=[]; [(bad.append((str(p),i,l.strip())) if re.search(r"(?<!::)\bexit\s*\(", l) and not l.lstrip().startswith("//") else None) for p in Path(".").glob("*") if p.suffix in {".cpp",".h"} for i,l in enumerate(p.read_text(errors="ignore").splitlines(),1)]; raise SystemExit(1 if bad else 0)'
check_absent 'No QThread::terminate() / thread.terminate()' '\.terminate[[:space:]]*\(' --include='*.cpp' --include='*.h' .
check_absent 'No QtConcurrent raw-this worker' 'QtConcurrent::run[[:space:]]*\([[:space:]]*\[this\]' --include='*.cpp' --include='*.h' .
check 'No active null FIR coefficient call' python3 -c 'from pathlib import Path; import re; bad=[]; [(bad.append((str(p),i,l.strip())) if "setFIRfilter" in l and "nullptr" in l and not l.lstrip().startswith("//") else None) for p in Path(".").glob("*.cpp") for i,l in enumerate(p.read_text(errors="ignore").splitlines(),1)]; raise SystemExit(1 if bad else 0)'

check 'Update watcher snapshots existing file' grep -q 'baseline existing file' FileUpdateWatcher.cpp
check 'Update flow requests Qt graceful quit' grep -q 'QCoreApplication::quit' Mainwindows.cpp
check '5G work is tracked by QFutureWatcher' grep -q 'QFutureWatcher<bool>' Mainwindows.h
check '5G worker is static' grep -q 'static bool reset5GModemNoRebootWorker' Mainwindows.h
check 'Delayed recorder manager callback has QObject context' grep -q 'QTimer::singleShot(1000, manager' Mainwindows.cpp

check 'PCM IMA ADPCM clamps step index before lookup' python3 -c 'from pathlib import Path; s=Path("pcmImaadpcmcodec.cpp").read_text(); a=s.index("stepIndex = std::clamp(stepIndex, 0, 88)"); b=s.index("step = stepTable[stepIndex]"); raise SystemExit(0 if a < b else 1)'
check 'IMA ADPCM masks nibble at boundary' grep -q 'nibble &= 0x0F' ImaAdpcmCodec.cpp
check 'Sigma delay loop starts at index zero' grep -q 'for (int i = 0; i < length; ++i)' SigmaStudioFW.cpp
check 'DspInitWorker does not feed null FIR coefficients' grep -q 'Never call setFIRfilter() with nullptr' DspInitWorker.cpp

check 'I2C file descriptor starts invalid' grep -q 'int file_i2c = -1' I2CReadWrite.h
check 'I2C device path copy is bounded' grep -q 'snprintf' I2CReadWrite.cpp
check 'I2C class owns fd cleanup' grep -q 'I2CReadWrite::~I2CReadWrite' I2CReadWrite.cpp
check 'SPI init reports status instead of killing process' grep -q 'bool SPIClass::spi_init' SPI.cpp

check 'ALSA producer queue is bounded' grep -q 'while (m_queue.size() >= kMaxQueuedChunks)' alsaaudioplayer.cpp
check 'ALSA stop documents playback-thread handle ownership' grep -q 'Do not touch m_pcmHandle here' alsaaudioplayer.cpp
check_absent 'ALSA stop does not force-kill playback thread' 'm_thread\.terminate[[:space:]]*\(' alsaaudioplayer.cpp

check 'ChatServer sockets use QPointer' grep -q 'QPointer<QWebSocket>' ChatServer.h
check 'ChatServer wrappers stored by value' grep -q 'QList<SoftPhoneSocketClient> recSocketClient' ChatServer.h
check_absent 'No body-less debug for-loops that multiply WebSocket sends' 'for[[:space:]]*\([^\n]+\)[[:space:]]*//' ChatServer.cpp
check 'Screenshot sender uses qobject_cast' grep -q 'qobject_cast<QQuickWindow' screencapture.cpp
check 'Screenshot image cache is capped' grep -q 'kMaxCachedScreenshots' screencapture.h
check 'WebSocket binary input has a hard size cap' grep -q 'kMaxBinaryFrameBytes' websocketclient.h
check 'PCM software volume avoids raw unaligned qint16 pointer' grep -q 'do not assume its data pointer is' websocketclient.cpp
check 'RFDC line receive buffer is capped' grep -q 'kMaxRxBufferBytes' rfdc_nco_client.h
check 'Local IPC input has a hard size cap' grep -q 'kMaxIpcCommandBytes' main.cpp
check 'Sanitizer diagnostic build option exists' grep -q 'SANITIZE_MEMORY' iScanMR10.pro

# Completeness is intentionally a warning/fail so nobody mistakes this archive
# for a complete whole-executable audit.
MISSING_COUNT=$(grep -cE '\.cpp$' R12-MISSING-COMPILED-SOURCES.txt 2>/dev/null || printf '0')
if [ "$MISSING_COUNT" -eq 33 ]; then
    printf '[WARN] Source completeness: 33 Jetson-compiled .cpp files are absent from this supplied archive.\n'
else
    printf '[WARN] Source completeness count changed: missing=%s; re-audit iScanMR10.pro.\n' "$MISSING_COUNT"
fi

printf '\nResult: PASS=%d FAIL=%d\n' "$PASS" "$FAIL"
printf 'Note: static contract checks are not a Qt compile or runtime sanitizer test.\n'
