#!/usr/bin/env bash
set -u

ROOT="${1:-$(pwd)}"
cd "$ROOT" || exit 2

fail=0
warn=0
pass() { printf '[PASS] %s\n' "$*"; }
failmsg() { printf '[FAIL] %s\n' "$*" >&2; fail=$((fail+1)); }
warnmsg() { printf '[WARN] %s\n' "$*" >&2; warn=$((warn+1)); }

for f in main.cpp iScanMR10.pro PLCServer.pro plcserver_main.cpp Mainwindows.h Mainwindows.cpp FftDisplayItem.h FftDisplayItem.cpp CrashDiagnostics.h CrashDiagnostics.cpp; do
    [[ -f "$f" ]] || failmsg "missing required recovery/core file: $f"
done

if [[ -f main.cpp ]]; then
    grep -q 'DiagnosticGuiApplication' main.cpp \
        && pass 'iScan main uses DiagnosticGuiApplication' \
        || failmsg 'iScan main is missing DiagnosticGuiApplication/R16.2 event diagnostics'

    grep -q 'qmlRegisterType<FftDisplayItem>("iScan.Display", 1, 0, "FftDisplayItem")' main.cpp \
        && pass 'native FFT QML type registration present' \
        || failmsg 'FftDisplayItem iScan.Display 1.0 registration missing'

    grep -q 'qrc:/main.qml' main.cpp \
        && pass 'iScan main loads qrc:/main.qml' \
        || failmsg 'iScan main does not load qrc:/main.qml'

    if grep -qE '#include[[:space:]]*[<"]PLCServer\.h[>"]|\bPLCServer[[:space:]]+[A-Za-z_]' main.cpp; then
        failmsg 'iScan main still contains PLCServer standalone entry logic'
    else
        pass 'iScan main is separated from PLCServer'
    fi
fi

if [[ -f iScanMR10.pro ]]; then
    # Standalone PLCServer translation units must never be pulled into iScanMR10.
    bad=0
    for f in PLCServer.cpp Function.cpp FunctionGPS.cpp FunctionPLC.cpp EventAudit.cpp DataStorage.cpp Database.cpp NetworkMng.cpp; do
        if grep -Eq "^[[:space:]]*${f//./\\.}[[:space:]]*\\\\?[[:space:]]*$" iScanMR10.pro; then
            failmsg "standalone PLCServer source leaked into iScanMR10.pro: $f"
            bad=1
        fi
    done
    [[ $bad -eq 0 ]] && pass 'iScanMR10.pro has no standalone PLCServer source leakage'
fi

if [[ -f PLCServer.pro ]]; then
    grep -q 'plcserver_main.cpp' PLCServer.pro \
        && pass 'PLCServer.pro uses dedicated plcserver_main.cpp' \
        || failmsg 'PLCServer.pro does not use dedicated plcserver_main.cpp'

    if grep -Eq '^[[:space:]]*main\.cpp[[:space:]]*\\?[[:space:]]*$' PLCServer.pro; then
        failmsg 'PLCServer.pro still consumes iScan main.cpp'
    else
        pass 'PLCServer.pro no longer consumes iScan main.cpp'
    fi
fi

python3 - "$ROOT" <<'PY'
import os, re, sys
from pathlib import Path
root = Path(sys.argv[1])
pro = root/'iScanMR10.pro'
if not pro.exists():
    sys.exit(0)
text = pro.read_text(errors='replace').splitlines()

def collect(var):
    out=[]; active=False
    for raw in text:
        line=raw.split('#',1)[0].rstrip()
        m=re.match(rf'^\s*{re.escape(var)}\s*\+=\s*(.*)$', line)
        if m:
            active=True; part=m.group(1)
        elif active:
            part=line.strip()
        else:
            continue
        cont=part.endswith('\\')
        if cont: part=part[:-1]
        part=part.strip()
        if part:
            out.extend(part.split())
        if not cont:
            active=False
    return out

sources=collect('JETSON_SOURCES')
headers=collect('JETSON_HEADERS')
missing=[]
for kind, vals in [('source',sources),('header',headers)]:
    for rel in vals:
        if '$$' in rel: continue
        if not (root/rel).exists():
            missing.append((kind,rel))

print(f'[INFO] Jetson qmake graph: {len(sources)} sources, {len(headers)} headers')
if missing:
    print(f'[WARN] qmake graph references {len(missing)} files absent from this tree:')
    for kind,rel in missing:
        print(f'       {kind}: {rel}')
    print('[WARN] Do NOT perform a release/clean build from an incomplete overlay archive.')
    print('[WARN] Apply this recovery overlay to the complete /home/only/Pictures/iSense tree first.')
else:
    print('[PASS] every JETSON_SOURCES/JETSON_HEADERS path exists')

# qmake/moc basename collision warning. Distinct headers with the same basename
# can both generate moc_<basename>.cpp and overwrite each other in one build dir.
from collections import defaultdict
bybase=defaultdict(list)
for rel in headers:
    bybase[Path(rel).name].append(rel)
dups={k:v for k,v in bybase.items() if len(set(v))>1}
if dups:
    print('[WARN] duplicate header basenames in Jetson qmake graph (MOC collision candidates):')
    for base, vals in sorted(dups.items()):
        print('       '+base+': '+', '.join(vals))
else:
    print('[PASS] no duplicate Jetson header basenames')
PY

# Build artifacts from another target/revision are dangerous after source ownership repair.
artifacts=()
for f in main.o Makefile moc_alsarecconfigmanager.cpp; do
    [[ -e "$f" ]] && artifacts+=("$f")
done
if ((${#artifacts[@]})); then
    warnmsg "stale/generated build artifacts exist in source tree: ${artifacts[*]} (use a fresh build directory)"
else
    pass 'no obvious stale main/Makefile/MOC artifacts in source root'
fi

printf '\nRecovery verifier: %d failure(s), %d warning(s)\n' "$fail" "$warn"
(( fail == 0 )) || exit 1
