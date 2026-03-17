#ifndef CHATCLIENTIGATE_H
#define CHATCLIENTIGATE_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include "QWebSocketServer"
#include "QWebSocket"
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QTimer>
QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)


#define ERROR 0
#define INFO 1

class ChatClientiGate : public QObject
{
    Q_OBJECT
public:
    explicit ChatClientiGate(quint16 port, QString ipaddress, QString initMessage,int index,bool autoConnect, QObject *parent = Q_NULLPTR);

    void broadcastMessageNodeState(uint8_t nodeID, QString conn, QString duration, QString trxStatus, QString radioStatus, QString vswr, QString durationRx, QString connRx, QString radioStatusRx,int softPhoneID);
    void broadcastSystemMessage(QString nodeSelected, int softPhoneID);
    bool autoConnectEnable = false;
    QWebSocket *m_pWebSocketClient = nullptr;

    void setDevice(QString _deviceName, QString _deviceAddress);
    void createDataLog(int state, QString dataLog);
public slots:
    void sendTextMessage(QString message);
    void autoConnectServer();
signals :
    //    void updateRadio(QString radioName, QString trxMode, QString uri, QString ipAddress, int r2sPeriod, int sipPort, QString frequency, int radioID);
    //    void removeRadio(int radioID);

    //    void updateChannel(QString channelName, QString mainRadioType, int mainRadio01ID, int mainRadio02ID, bool standbyEnable, QString standbyRadioType, int standbyRadio01ID, int standbyRadio02ID, QString mainRadioName, QString standbyRadioName, int channelID);
    //    void removeChannel(int channelID);

    //    void updateChannelTxRxEnable(bool mainRadioTransmitterUsed, bool mainRadioReceiverUsed, int channelID);
    //    void updateControler(QString sipUser, int keepAlivePeroid, int sipPort, int rtpStartPort, int channelID, int softPhoneID);

    void connected(int index);
    void textMessageReceived(QString message);



private:

    QList<QWebSocket *> m_clients;
    QWebSocket *newSocket;
    QString init_message;
    QTimer *autoConnectTimer;
    int socketIndex = 0;
    QString url = "";
    QString deviceName = "" ;
    QString deviceAddress = "" ;

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(QString message);
};

#endif // CHATCLIENT_H
