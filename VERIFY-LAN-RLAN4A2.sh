#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

fail() { echo "[FAIL] $*" >&2; exit 1; }
pass() { echo "[PASS] $*"; }

for f in NetworkController.cpp Setting.qml Mainwindows.cpp iScreenDF/DatabaseDF.cpp iScreenDF/functionTcpServer.cpp; do
    [[ -f "$f" ]] || fail "missing $f"
done

# Authoritative mirrored role model.
grep -q 'lanKey == QStringLiteral("lan1") || lanKey == QStringLiteral("rfsoc1")' NetworkController.cpp || fail "LAN1/LAN3 device role mapping missing"
grep -q 'lanKey == QStringLiteral("lan2") || lanKey == QStringLiteral("rfsoc2")' NetworkController.cpp || fail "LAN2/LAN4 network role mapping missing"
grep -q 'oneLan\[QStringLiteral("portRole")\]' NetworkController.cpp || fail "portRole metadata missing"
grep -q 'oneLan\[QStringLiteral("executionScope")\]' NetworkController.cpp || fail "executionScope metadata missing"
pass "LAN1↔LAN3 and LAN2↔LAN4 mirrored role model present"

# Scope must remain distinct from role.
grep -q 'QStringLiteral("Remote RFSoC")' NetworkController.cpp || fail "remote scope label missing"
grep -q 'QStringLiteral("Local Device")' NetworkController.cpp || fail "local scope label missing"
grep -q 'controlScope.*executionScope' NetworkController.cpp || fail "compatibility controlScope not retained"
pass "role and execution scope are independently represented"

# QML consumes role + scope, and keeps remote target IP semantics.
grep -q 'function lanPortRole(info)' Setting.qml || fail "QML port role helper missing"
grep -q 'function lanExecutionScope(info)' Setting.qml || fail "QML scope helper missing"
grep -q 'function lanRoleScopeText(info)' Setting.qml || fail "QML role/scope presentation missing"
grep -q 'Configured IPv4' Setting.qml || fail "configured remote IPv4 presentation missing"
grep -q 'Remote RFSoC' Setting.qml || fail "remote scope presentation missing"
pass "Setting.qml presents mirrored role semantics without hiding execution scope"

# Proven transport/database contract must still exist.
grep -q 'UPDATE Network2' iScreenDF/DatabaseDF.cpp || fail "Network2 database update contract missing"
grep -q 'obj\["menuID"\].*=.*"setIpConfig"' iScreenDF/functionTcpServer.cpp || fail "setIpConfig TCP contract missing"
grep -q 'obj\["ifname"\].*=.*iface' iScreenDF/functionTcpServer.cpp || fail "TCP ifname mapping missing"
grep -q 'index == 2.*end0' Mainwindows.cpp || fail "LAN3/end0 mapping missing"
grep -q 'index == 3.*end1' Mainwindows.cpp || fail "LAN4/end1 mapping missing"
pass "Network2 + LAN3/end0 + LAN4/end1 TCP apply contracts preserved"

# R-LAN3 persistence protections still present.
grep -q '#include <QSaveFile>' NetworkController.cpp || fail "QSaveFile atomic writer missing"
grep -q 'networkConfigMutex' NetworkController.cpp || fail "network config transaction mutex missing"
pass "R-LAN3 atomic persistence protections retained"

# Local/remote executor semantics remain distinct.
grep -q 'const bool remoteLan = (index == 2 || index == 3);' Mainwindows.cpp || fail "remote executor selection missing"
grep -q 'NetworkManager' Setting.qml || true
pass "remote executor remains restricted to LAN3/LAN4"

echo "[PASS] R-LAN4A.2 structural verification complete"
