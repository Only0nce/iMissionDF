#!/bin/sh
set -eu
F="${1:-Setting.qml}"
[ -f "$F" ] || { echo "[FAIL] missing $F"; exit 1; }

grep -q 'property bool lanModeDirty: false' "$F" || { echo '[FAIL] lanModeDirty missing'; exit 1; }
grep -q 'useDhcp = true' "$F" || { echo '[FAIL] DHCP selection missing'; exit 1; }
grep -q 'lanModeDirty = true' "$F" || { echo '[FAIL] draft guard not set on selection'; exit 1; }
grep -q 'if (!lanModeDirty)' "$F" || { echo '[FAIL] async mode overwrite guard missing'; exit 1; }
grep -q 'modeText === "manual"' "$F" || { echo '[FAIL] NetworkManager manual/static normalization missing'; exit 1; }

# The DHCP button must not immediately refresh/reload the old persisted mode.
python3 - "$F" <<'PY'
import re,sys
s=open(sys.argv[1],encoding='utf-8').read()
m=re.search(r'text:\s*"Using DHCP"(?P<body>.*?)(?:text:\s*"Static Manual")',s,re.S)
if not m:
    raise SystemExit('[FAIL] DHCP/Static button region not found')
body=m.group('body')
if 'refreshDhcpInfo()' in body:
    raise SystemExit('[FAIL] DHCP button still calls refreshDhcpInfo() and can bounce back')
print('[PASS] DHCP button no longer reloads persisted mode on selection')

# Lightweight delimiter check ignoring quoted strings/comments sufficiently for this file.
stack=[]
pairs={')':'(',']':'[','}':'{'}
in_s=in_d=False
esc=False
i=0
while i < len(s):
    c=s[i]
    if esc:
        esc=False; i+=1; continue
    if (in_s or in_d) and c=='\\':
        esc=True; i+=1; continue
    if not in_d and c=="'": in_s=not in_s; i+=1; continue
    if not in_s and c=='"': in_d=not in_d; i+=1; continue
    if in_s or in_d: i+=1; continue
    if s.startswith('//',i):
        j=s.find('\n',i); i=len(s) if j<0 else j+1; continue
    if s.startswith('/*',i):
        j=s.find('*/',i+2); i=len(s) if j<0 else j+2; continue
    if c in '([{': stack.append(c)
    elif c in ')]}':
        if not stack or stack.pop()!=pairs[c]:
            raise SystemExit('[FAIL] delimiter mismatch near offset %d'%i)
    i+=1
if stack:
    raise SystemExit('[FAIL] unclosed delimiters: %r'%stack[-10:])
print('[PASS] lightweight QML delimiter validation')
PY

echo '[PASS] DHCP-UX1 structural verification complete'
