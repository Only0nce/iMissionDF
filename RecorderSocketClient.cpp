#include "RecorderSocketClient.h"

RecorderSocketClient::RecorderSocketClient(const QUrl &url, QObject *parent)
    : QObject(parent), webSocket(new QWebSocket)
{
    connect(webSocket, &QWebSocket::connected, this, &RecorderSocketClient::onConnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &RecorderSocketClient::onMessageReceived);
    connect(webSocket, &QWebSocket::disconnected, this, &RecorderSocketClient::onDisconnected);

    qDebug() << "Connecting to WebSocket server...";
    webSocket->open(url);
}

void RecorderSocketClient::onConnected() {
    qDebug() << "Connected to server!";
    //sendCommand("INIT");  // Send an initial message

    // Send a single command after 5 seconds (one-time, no loop)
    QTimer::singleShot(3000, this, [this]() { sendCommand("0,status"); });
}

void RecorderSocketClient::onMessageReceived(const QString &message) {
    //qDebug() << "Received from server:" << message;
    if (message.isEmpty()) {
        qDebug() << "Received an empty message from the server.";
    } else {
        qDebug() << "Received from server:" << message;
        QStringList parts = message.split(":"); // Split by ':'
        if (!parts.isEmpty() ) {
            if (parts[0] == "EMPTY") {
                // First ANNOUNCE request
                QTimer::singleShot(1000, this, [this]() { sendCommand("0,ANNOUNCE,recin1"); });
            }
            if (parts[0] == "SETUP") {
                // Start recording
                QTimer::singleShot(1000, this, [this]() { sendCommand("0,RECORD"); });
            }
            //if (parts[0] == "PAUSE") {
            //    // Resume record again
            //    QTimer::singleShot(10000, this, [this]() { sendCommand("0,RECORD"); });
            //}
        }
    }
}

void RecorderSocketClient::onDisconnected() {
    qDebug() << "Disconnected from server.";
}
