
#ifndef RECEIVERRECORDERCONFIGMANAGER_H
#define RECEIVERRECORDERCONFIGMANAGER_H
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QJsonObject>

class ReceiverRecorderConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ReceiverRecorderConfigManager(QObject *parent = nullptr);
    Q_INVOKABLE QVariantMap loadConfig();
    Q_INVOKABLE void getConfig();
    Q_INVOKABLE void updateRecorderConfig(const QString &alsaDev,
                                          const QString &clientIp,
                                          double clientFreq,
                                          const QString &rtspIp,
                                          const QString &rtspUri,
                                          int rtspPort);

signals:
    void onRecorderConfigSaved();
    void configLoaded(const QString &alsaDev,
                      const QString &clientIp,
                      double clientFreq,
                      const QString &rtspIp,
                      const QString &rtspUri,
                      int rtspPort);
};

#endif // RECEIVERRECORDERCONFIGMANAGER_H
