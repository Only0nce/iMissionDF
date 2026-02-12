#ifndef DATABASEDF_H
#define DATABASEDF_H

#include <QObject>
#include <QSqlDatabase>
#include <QtSql>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QThread>

#include <QWebSocketServer>
#include <QWebSocket>
#include <QNetworkInterface>

class DatabaseDF : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseDF(const QString &dbName,
                        const QString &user,
                        const QString &password,
                        const QString &host,
                        QObject *parent = nullptr);
    ~DatabaseDF();

    // DB connection
    bool database_createConnection();   // optional, ใช้ ensureDb แทนก็ได้

    void restartMysql();

    // Network / Kraken / NTP
    void getKrakenSetting();   // stub ไว้ให้ (ยังไม่ได้ implement จริง)
    void getNetwork();
    void getNTPServer();
    void setNTPServer(const QString &ip);
    void setNTPServerMethod(const int &method);
    void setNTPServerLocation(const QString &location);
    void getKrakenServer();    // stub ไว้ให้ (ยังไม่ได้ implementจริง)
    void updateKrakenServer(const QString &ip);

    // iScreen / ServerKraken
    void ensureColumnsInIScreenparameter();
    void createServerKrakenNetworkTable();
    void getIScreenParameter();
    void updateIScreenParameterById(int id,
                                    const QString &krakenserver,
                                    const QString &iScreenclient,
                                    const QString &subnet,
                                    const QString &gateway,
                                    const QString &phyName);
    void getServerKrakenNetwork();
    void updateServerKrakenNetwork(const QString &dhcp,
                                   const QString &ip,
                                   const QString &subnet,
                                   const QString &gateway,
                                   const QString &primaryDns,
                                   const QString &secondaryDns,
                                   const QString &phyName);

    // Network2 (multi NIC) + QML
    void getNetworkfromDb();
    void updateNetworkfromDisplay(int index,
                                  const QString &dhcp,
                                  const QString &ip,
                                  const QString &mask,
                                  const QString &gw,
                                  const QString &dns1,
                                  const QString &dns2);

    // Groups / DeviceList (QML pages)
    void getRemoteGroups();
    void getSideRemote();
    void getGroupsInGroupSetting();
    void editGroupName(const QString &uniqueIdInGroup,
                       const QString &title);
    void saveGroupSettingFromJson(const QString &jsonString);
    QString generateShortUuid();
    void addNewDevice(const QString &name,
                      const QString &ip,
                      const QString &deviceUidFromUi);
    void deleteDeviceByUniqueId(const QString &deviceUniqueId);

    void deletDeviceInGroups(int id,const QString &name, const QString &ip);
    void updateDeviceByUniqueId(const QString &oldUid,
                                const QString &newUid,
                                const QString &name,
                                const QString &ip);
    void updateUniqueIdDeviceOngroup(const QString &oldUid,
                                     const QString &newUid);
    void getGroupByUid(const QString &uniqueIdInGroup);
    void savegroupSettingBygroupID(const QString &json);

    void getDevicesInGroupJson(const QString &groupUniqueId);

    // WebSocket server connect
    void getAllClientInDatabase();
    void getActiveClientInDatabase();
    void getActiveClientInDatabase(const QString &uniqueIdInGroupFilter);

    void saveDeviceList(const QJsonObject &obj);
    void saveDeviceGroupsFromConnectGroupSingle(const QJsonObject &obj);
    QString getLocalMacLastOctet();
    void savegroupSettingNewGroup(const QString &groupName,
                                  const QList<QString> &deviceUniqueIds,
                                  int &outGroupID,
                                  QString &outUniqueIdInGroup);
    void insertDevicesinGroup(int groupID,
                              const QString &groupName,
                              const QString &deviceUniqueId,
                              const QString &uniqueIdInGroup);
    void removeDeviceFromGroup(int groupID,
                               const QString &deviceUniqueId,
                               const QString &uniqueIdInGroup);
    void deleteGroupByUID(const QString &uniqueIdInGroup);
    void saveDevicesAndGroupsFromConnectGroupSingle(const QJsonObject &obj,
                                                    const QString &localIp);
    //////////// Recorder////////////////////
    void setRecorderSettingsDB(const QString &alsaDevice,const QString &clientIp,int freq,const QString &rtspServer,const QString &rtspUrl,int rtspPort);
    void getRecorderSettings();
    ///////////// setmode //////////////////
    void UpdateMode(const QString &mode);
    void GetParameter();

    void UpdateDeviceParameter(const QString &deviceName,const QString &serial);
    void updateDeviceInGroup(int groupID,const QString &groupName,const QString &deviceUniqueId,int roleIndex,const QString &uniqueIdInGroup);

    void GetrfsocParameter();
    void GetIPDFServerFromDB();
    void UpdateParameterField(const QString &field, const QVariant &value);
    void ensureParameterHasMaxDoaLineMeters();
    void ensureParameterIPLocalForRemoteGroup();


signals:
    // Network / NTP
    void updateNetwork(const QString &dhcp,
                       const QString &ip,
                       const QString &subnet,
                       const QString &gateway,
                       const QString &primaryDns,
                       const QString &secondaryDns,
                       const QString &krakenserver);

    void updateNTPServer(const QString &ip,const QString &location,const int &Method);
    void setConnectToserverKraken(const QString &ip);
    void updateNetworkServerKraken(const QString &dhcp,const QString &ip,const QString &subnet,const QString &gateway,const QString &primaryDns,const QString &secondaryDns,const QString &phyName);

    // QML json payloads
    void remoteGroupsJson(const QString &json);
    void remoteSideRemoteJson(const QString &json);
    void sigGroupsInGroupSetting(const QString &json);

    // Network2 item
    void NetworkAppen(int id,const QString &dhcp,const QString &ip,const QString &subnet,const QString &gateway,const QString &primaryDns,const QString &secondaryDns,const QString &phyName,const QString &krakenserver);

    void appendNewClient(int id = 0, QString name = "", QString ipAddress = "", uint16_t socketPort = 0);
    void appendNewActiveClient(const QString &deviceUniqueId,const QString &uniqueIdInGroup,int deviceID,int groupID,const QString &groupName,const QString &deviceName,const QString &deviceIPAddress,uint16_t socketPort);
    void devicesInGroupJsonReady(int groupId,const QString &groupName,const QString &groupUniqueId,const QJsonArray &devices);
    void setupServerClientForDevices(const QString &uniqueIdInGroup);
    //////////// Recorder////////////////////
    void recorderSettingsReady(QString alsaDevice,QString clientIp,int frequency,QString rtspServer,QString rtspUrl,int rtspPort);
    ///////////// setmode //////////////////
    void parameterReceived(const QString &mode,const QString &deviceName,const QString &serialnumber);

    void Getrfsocparameter(bool  setDoaEnable, bool spectrumEnabled, int setAdcChannel, int Frequency, int update_en, double TxHz,
                           int TargetOffsetHz, int DoaBwHz, double DoaPowerThresholdDb,const QString &DoaAlgorithm, double ucaRadiusM,double TargetDb,bool rfAgcEnabled,bool linkStatus,double offsetvalue,double compassoffset
                           , int maxDoaLineMeters,const QString &ipLocalForRemoteGroup, int setDelayMs, int setDistance);
    void GetIPDFServer(const QString &ip);
    void updateNetworkDfDevice(const QString &iface,const QString &mode,const QString &ip,const QString &subnet,const QString &gateway,const QString &primaryDns,const QString &secondaryDns);

public slots:
    void init();
    void shutdown();

    // slot จาก UI
    void setNetworkSlot(const QString &dhcp,
                        const QString &ip,
                        const QString &subnet,
                        const QString &gateway,
                        const QString &primaryDns,
                        const QString &secondaryDns,
                        const QString &krakenserver);
    // void appendNewClient(int id = 0, QString name = "", QString ipAddress ="", uint16_t socketPort = 0);

private:
    QSqlDatabase db;
    bool ensureDb();

    // config สำหรับสร้าง connection ใน thread ปัจจุบัน
    QString m_dbName;
    QString m_dbUser;
    QString m_dbPassword;
    QString m_dbHost;
};

#endif // DATABASEDF_H
