#!/bin/sh
set -eu

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

python3 - <<'PY' || exit 1
from pathlib import Path
import re, sys

chat = Path('ChatServer.cpp').read_text(errors='ignore')
# Count generic dispatcher calls, not comments/signal declarations.
count = len(re.findall(r'^[ \t]*emit\s+newCommandProcess\s*\(', chat, re.M))
if count != 1:
    raise SystemExit(f'[FAIL] expected one generic newCommandProcess emit, found {count}')
print('[PASS] ChatServer generic command dispatch occurs once')

mw = Path('Mainwindows.cpp').read_text(errors='ignore')
m = re.search(r'void Mainwindows::setLocation\(QString location\)(.*?)(?=\n(?:bool|void|Q_INVOKABLE) Mainwindows::)', mw, re.S)
if not m:
    raise SystemExit('[FAIL] setLocation body not found')
if re.search(r'\bsystem\s*\(', m.group(1)):
    raise SystemExit('[FAIL] setLocation still uses system()')
print('[PASS] setLocation does not execute a shell command string')

m = re.search(r'Q_INVOKABLE void Mainwindows::sCanfreq\(\)(.*?)(?=\nvoid Mainwindows::sendNextScanFrequencyStep)', mw, re.S)
if not m:
    raise SystemExit('[FAIL] sCanfreq body not found')
if 'msleep' in m.group(1) or 'while(' in m.group(1).replace(' ', ''):
    raise SystemExit('[FAIL] sCanfreq still contains blocking scan logic')
print('[PASS] sCanfreq is non-blocking')

alsa = Path('alsaaudioplayer.cpp').read_text(errors='ignore')
if 'm_thread.terminate' in alsa:
    raise SystemExit('[FAIL] ALSA still calls QThread::terminate()')
if 'm_queue.size() >= m_maxQueuedChunks' not in alsa:
    raise SystemExit('[FAIL] ALSA producer queue hard bound missing')
print('[PASS] ALSA has no terminate fallback and producer queue is bounded')

pcm = Path('pcmImaadpcmcodec.cpp').read_text(errors='ignore')
if 'stepIndex = std::clamp(stepIndex, 0, 88);' not in pcm:
    raise SystemExit('[FAIL] ADPCM step index pre-clamp missing')
print('[PASS] synchronized ADPCM step index is bounded')

worker = Path('DspInitWorker.cpp').read_text(errors='ignore')
active_null = [ln for ln in worker.splitlines() if 'setFIRfilter' in ln and 'nullptr' in ln and not ln.lstrip().startswith('//')]
if active_null:
    raise SystemExit('[FAIL] active null FIR coefficient call remains')
print('[PASS] no active null FIR coefficient programming')

i2ch = Path('I2CReadWrite.h').read_text(errors='ignore')
if 'int file_i2c = -1;' not in i2ch or '~I2CReadWrite();' not in i2ch:
    raise SystemExit('[FAIL] I2C fd lifetime hardening missing')
print('[PASS] I2C descriptor starts invalid and has destructor cleanup')

ws = Path('websocketclient.cpp').read_text(errors='ignore')
if 'scheduleReconnect' not in ws or 'attemptReconnect' not in ws:
    raise SystemExit('[FAIL] AstraRX reconnect state missing')
print('[PASS] AstraRX reconnect state is present')
PY

if grep -R --line-number --include='*.qml' 'R20 BACKEND HARDENING' . >/dev/null 2>&1; then
    fail 'R20 backend marker leaked into QML'
fi
pass 'No R20 backend edits are embedded in QML'

echo '[PASS] R20 backend static validation complete'
