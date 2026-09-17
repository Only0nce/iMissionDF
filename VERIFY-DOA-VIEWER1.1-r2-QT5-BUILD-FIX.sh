#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

pass=0
fail=0
check() {
    local name="$1"; shift
    if "$@"; then
        printf 'PASS  %s\n' "$name"
        pass=$((pass+1))
    else
        printf 'FAIL  %s\n' "$name"
        fail=$((fail+1))
    fi
}

check "Qt5 moc fragile doaRx Q_PROPERTY removed" \
    bash -c '! grep -q "Q_PROPERTY.*doaRx" Mainwindows.h'
check "DoA RX QVariantList invokable getter present" \
    grep -q 'Q_INVOKABLE QVariantList doaRxFftMagDb() const' Mainwindows.h
check "DoA RX center invokable getter present" \
    grep -q 'Q_INVOKABLE double doaRxCenterHz() const' Mainwindows.h
check "DoA RX sample-rate invokable getter present" \
    grep -q 'Q_INVOKABLE int doaRxSampleRate() const' Mainwindows.h
check "QML calls FFT getter" \
    grep -q 'm.doaRxFftMagDb()' DoaViewer/ViewerPage.qml
check "QML calls center getter" \
    grep -q 'm.doaRxCenterHz()' DoaViewer/ViewerPage.qml
check "QML calls sample-rate getter" \
    grep -q 'm.doaRxSampleRate()' DoaViewer/ViewerPage.qml
check "WebSocket shutdown declaration present" \
    grep -q 'Q_INVOKABLE void shutdown();' websocketclient.h
check "WebSocket shutdown implementation present" \
    grep -q 'void WebSocketClient::shutdown()' websocketclient.cpp
check "Shutdown disables reconnect" grep -q 'm_shuttingDown = true;' websocketclient.cpp
check "Shutdown stops reconnect timer" grep -q 'm_reconnectTimer.stop();' websocketclient.cpp
check "Shutdown clears reconnect target" grep -q 'm_targetUrl = QUrl();' websocketclient.cpp
check "Destructor reuses shutdown path" grep -q 'shutdown();' websocketclient.cpp

printf '\nRESULT: %d PASS / %d FAIL\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
