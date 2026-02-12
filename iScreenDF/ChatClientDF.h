#ifndef CHATCLIENTDF_H
#define CHATCLIENTDF_H

#include <QObject>
#include <QDebug>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QTimer>

class ChatClientDF : public QObject
{
    Q_OBJECT
public:
    explicit ChatClientDF(QObject *parent = nullptr);

    void sendTextMessage(const QString &message);
    void createConnection(const QString &ipaddress, const quint16 &port);

    // ให้ Mainwindows เรียกเพื่อบอกว่า client ตัวนี้อยู่ใน groupIndex ไหน
    void setGroupIndex(int index) { m_groupIndex = index; }

    QString pageSelector = "";
    QString ip_address = "";
    void setServerAddress(const QString &host, quint16 port);
    void reconnect2();

    QString m_ipaddress;
    bool isConnected = false;
    QWebSocket *m_webSocket = nullptr;
    int  m_socketID = -1;
    void disconnectFromServer();
    void stopReconnectTimer();
    QString getUniqueIdInGroup() const { return m_uniqueIdInGroup; }
    // QString uniqueIdInGroup;
    QString m_uniqueIdInGroup;
    void setUniqueIdInGroup(const QString &id);

signals:
    void TextMessageReceived(const QString &message);
    void closed(int socketID, const QString &ip);
    void SendupdateNetworkServerKraken();
    void SendNetworkiScreentoServerKraken(const QString &message);
    // void SendNetworkiScreentoServerKraken();
    void onDeviceConnected(const QString &uniqueIdInGroup, const QString &ipaddress);

private slots:
    void onConnected();
    void onTextMessageReceived(const QString &message);
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void reconnect();
    void ensureConnected();

private:
    void connectSignals();
    QUrl m_url;
    quint16 m_port = 0;
    // int  m_socketID = -1;    // ใช้เป็น groupIndex หรือ socketID ตามที่คุณต้องการ
    int  m_groupIndex = -1;  // แยก field ไว้ชัด ๆ
    QTimer m_reconnectTimer;
    bool m_debug = true;
    QString m_host;
};

#endif // SOCKETCLIENT_H
