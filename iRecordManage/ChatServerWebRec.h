#ifndef CHATSERVERWEBREC_H
#define CHATSERVERWEBREC_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QDebug>

#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>

class ChatServerWebRec : public QObject
{
    Q_OBJECT
public:
    explicit ChatServerWebRec(quint16 port = 1235, QObject *parent = nullptr);
    ~ChatServerWebRec();
    Q_INVOKABLE void sendMessageTo(QWebSocket *client, const QString &message);
    void sendMessageToRecService(QString message, int softPhoneID);
    int clientCount() const { return m_clients.size(); }
    bool enableDataLogger = false;
    QList<QWebSocket *> m_WebSocketClients;

signals:
    void clientConnected(QWebSocket *client);
    void clientDisconnected(QWebSocket *client);
    void messageFromWeb(const QString &message, QWebSocket *sender);
    void clientCountChanged(int count);
    void cppCommandToWeb(QString);
    void getSystemPage(QWebSocket *pSender);
    void getVuMeter(QWebSocket *pSender);
    void onSendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freq);

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString &message);
    void onSocketDisconnected();

private:
    QWebSocketServer *m_server = nullptr;
    QList<QWebSocket*> m_clients;
    struct SoftPhoneSocketClient
    {
        int softPhoneID;
        QWebSocket *SocketClients;
    };
    QList<SoftPhoneSocketClient *> softPhoneSocketClient;
    QList<SoftPhoneSocketClient *> recSocketClient;

public Q_SLOTS :
    void sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, QString state, double freq);
    Q_INVOKABLE void broadcastMessage(const QString &message);
    void sendToWebMessageClientWebSender(QString message,QWebSocket *webClient);
    void updateAllowedUri(uint8_t softPhoneID, uint8_t numConn, QString uri1, QString uri2, QString uri3, QString uri4, QString uri5, QString uri6, QString uri7, QString uri8);
    void commandProcess(QString message, QWebSocket *pSender);

};

#endif // CHATSERVERWEBREC_H
