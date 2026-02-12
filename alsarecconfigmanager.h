#ifndef ALSARECDCONFIGMANAGER_H
#define ALSARECDCONFIGMANAGER_H

#include "ChatServer.h"
#include <QTimer>
#include <QObject>
#include <QMap>
#include <QSettings>

#define ALSARECCONF "/etc/alsarecd.conf"
struct AlsaRecConfig
{
    int recordID;
    QString alsa_dev;
    QString client_as_ip;
    double client_as_freq;
    QString rtsp_server_ip;
    int rtsp_server_port;
    QString rtsp_server_uri;
    QString service;
    bool enable;
    QString status;
    quint64 lastMessage;

    bool operator==(const AlsaRecConfig &other) const {
        return alsa_dev == other.alsa_dev
               && client_as_ip == other.client_as_ip
               && client_as_freq == other.client_as_freq
               && rtsp_server_ip == other.rtsp_server_ip
               && rtsp_server_port == other.rtsp_server_port
               && rtsp_server_uri == other.rtsp_server_uri
               && service == other.service
               && enable == other.enable;
    }

    bool operator!=(const AlsaRecConfig &other) const {
        return !(*this == other);
    }
};


class AlsaRecConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit AlsaRecConfigManager(QObject *parent = nullptr);
//    explicit AlsaRecConfigManager(ChatServer *server, QObject *parent=nullptr);
    bool loadConfig(const QString &filePath);
    bool saveConfig(const QString &filePath, const QString &targetKey);
    bool saveConfig(const QString &filePath);


    QMap<QString, AlsaRecConfig> getConfigs() const;
    void setConfig(const QString &section, const AlsaRecConfig &config);

    void applyAllConfigs();
    void sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freq);
    QString getState(int iGateID) ;
    ChatServer *RecorderSocketServer = nullptr;
    //    ChatServer *RecorderSocketServer = nullptr;;

public slots:
    void getAllConfigs(QWebSocket *sender);
    void handleApplyRecSettings(const QJsonObject &obj);
    void updateClientAsIPForAllConfigs(const QString &newIP);
    void updateRtspUriForConfigs(int recID, const QString &newUri);
signals:
    void configChanged(const QString &section);
    void sendMessageToWeb(const QString &jsonMessae);
    void requestSendSquelchStatus(int softPhoneID,
                                  bool pttOn,
                                  bool sqlOn,
                                  bool callState,
                                  QString state,
                                  double freq);


private:
    QMap<int, QTimer*> pauseTimers;
    QMap<QString, AlsaRecConfig> m_configs;
    bool isPortOpen(const QString &ip, int port);
    bool isServiceActive(const QString &serviceName);
    void startService(const QString &serviceName);
    void restartService(const QString &serviceName);
    void stopService(const QString &serviceName);
    double getFrequency(uint8_t iGateID);
    void teardown(int recID);

    QTimer *autoAnnounceTimer;
    QTimer *loopAnnounceTimer;
    QTimer *loopCheckServerAlive;
    QList<int> pendingAnnounceIDs;

private slots:
    void checkServerAlive();
    void autoAnnounce();
    void processNextAnnounce();
    void recLogging(int softPhoneID, int recorderID,QString recState, QString message);


};

#endif // ALSARECDCONFIGMANAGER_H
