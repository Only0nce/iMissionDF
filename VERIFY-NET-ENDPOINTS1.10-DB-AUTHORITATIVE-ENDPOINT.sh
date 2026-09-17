#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(cd "$(dirname "$0")" && pwd)}"
cd "$ROOT"
pass=0; fail=0
ok(){ echo "PASS  $1"; pass=$((pass+1)); }
bad(){ echo "FAIL  $1"; fail=$((fail+1)); }
need(){ local pat="$1" f="$2" n="$3"; grep -Eq "$pat" "$f" && ok "$n" || bad "$n"; }
absent(){ local pat="$1" f="$2" n="$3"; if grep -Eq "$pat" "$f"; then bad "$n"; else ok "$n"; fi; }

QML=iScreenDFqml/pages/TopNetworkDrawer.qml
DBH=iScreenDF/DatabaseDF.h
DBC=iScreenDF/DatabaseDF.cpp
ISH=iScreenDF/iScreenDF.h
ISC=iScreenDF/iScreenDF.cpp
FTC=iScreenDF/functionTcpServer.cpp

need 'SELECT ipdfserver FROM Parameter WHERE id = 1' "$DBC" 'authoritative Parameter snapshot SELECT exists'
need 'requestDfServerEndpointSnapshot' "$DBH" 'DB snapshot API declared'
need 'dfServerEndpointSnapshotReady' "$DBH" 'DB snapshot signal declared'
need 'dfServerEndpointSnapshotReady' "$ISC" 'snapshot signal wired to iScreenDF'
need 'void iScreenDF::requestDfServerEndpointSnapshot' "$FTC" 'QML/backend snapshot request implemented'
need 'void iScreenDF::onDfServerEndpointSnapshotReady' "$FTC" 'runtime/UI snapshot reconciliation implemented'
need 'UPDATE Network2 ' "$DBC" 'legacy Network2 endpoint mirror exists'
need 'Network2 mirror mismatch' "$DBC" 'legacy mirror read-back verification exists'
need 'syncLegacyDfEndpointMirror\(requested' "$DBC" 'new endpoint commit mirrors legacy DB inside transaction'
need 'requestDfServerEndpointSnapshot' "$QML" 'drawer requests fresh authoritative endpoint'
need 'committedDfServerIp' "$QML" 'QML keeps explicit authoritative endpoint state'
absent 'serverField\.text = row\.krakenserver' "$QML" 'Network2 row cannot overwrite DF Server IP'
absent 'setIfNotEmpty\(serverField,[[:space:]]*"server"\)' "$QML" 'NIC refill cannot overwrite DF Server IP'
absent 's\(selectedNic,[[:space:]]*"server",[[:space:]]*text\)' "$QML" 'draft DF endpoint does not mutate legacy NIC cache'
absent 'm_ipdfServer[[:space:]]*=[[:space:]]*"192\.' "$ISH" 'compiled-in DF endpoint fallback removed'

printf '\nStatic result: %d PASS / %d FAIL\n' "$pass" "$fail"
(( fail == 0 ))
