#!/usr/bin/env python3
import re, sys, pathlib
log = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else '/tmp/iScanMR10-debug.log')
text = log.read_text(errors='replace').splitlines()
# Last fatal PC unless user supplies one.
pc = None
if len(sys.argv) > 2:
    pc = int(sys.argv[2], 0)
else:
    for line in reversed(text):
        if '[R13-FATAL]' in line:
            m = re.search(r'\bpc=(0x[0-9a-fA-F]+)', line)
            if m:
                pc = int(m.group(1), 16)
                break
if pc is None:
    raise SystemExit('No fatal pc=... found')
# Keep the final /proc/self/maps block in the appended log.
maps=[]; cur=[]; inside=False
for line in text:
    if line.startswith('----- /proc/self/maps begin -----'):
        cur=[]; inside=True; continue
    if line.startswith('----- /proc/self/maps end -----'):
        maps=cur[:]; inside=False; continue
    if inside:
        cur.append(line)
pat=re.compile(r'^([0-9a-fA-F]+)-([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)\s+\S+\s+\S+\s*(.*)$')
for line in maps:
    m=pat.match(line)
    if not m: continue
    start,end,off=int(m.group(1),16),int(m.group(2),16),int(m.group(3),16)
    path=m.group(4).strip()
    if start <= pc < end:
        elf_off=off+(pc-start)
        print(f'pc=0x{pc:x}')
        print(f'map=0x{start:x}-0x{end:x} file_offset=0x{off:x}')
        print(f'path={path or "[anonymous]"}')
        print(f'ELF relative address=0x{elf_off:x}')
        if path.startswith('/'):
            print(f'Run: addr2line -Cfipe {path!r} 0x{elf_off:x}')
        sys.exit(0)
raise SystemExit(f'PC 0x{pc:x} not found in last maps block')
