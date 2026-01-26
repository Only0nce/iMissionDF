#ifndef RECORDERSOCKETCLIENT_H
#define RECORDERSOCKETCLIENT_H

#include <QWebSocket>
#include <QObject>
#include <QTimer>
#include <QDebug>

class RecorderSocketClient : public QObject {
    Q_OBJECT

public:
    RecorderSocketClient(const QUrl &url, QObject *parent = nullptr);
    void sendCommand(const QString &command) {
        if (webSocket->isValid()) {
            qDebug() << "Sending command:" << command;
            webSocket->sendTextMessage(command);
        } else {
            qDebug() << "WebSocket not connected. Cannot send command.";
        }
    }

private slots:
    void onConnected() ;

    void onMessageReceived(const QString &message);

    void onDisconnected();

private:
    QWebSocket *webSocket;
};

#endif // RECORDERSOCKETCLIENT_H
