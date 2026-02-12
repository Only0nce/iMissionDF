#ifndef NETWORKMNG_H
#define NETWORKMNG_H
#include <QString>
#include <QObject>
#include "QProcess"
#include <QJsonObject>
#include <QJsonDocument>

class NetworkMng : public QObject
{
    Q_OBJECT
public:
    explicit NetworkMng(QObject *parent = 0);
    virtual ~NetworkMng();
    QString getAddress(QString netWorkCard);
//    void getIPAddress(QString netWorkCard);
    QString getTimezone();
    unsigned short toCidr(const char *ipAddress);
    void resetNtp();
    void setStaticIpAddr(QString ipaddr, QString netmask, QString gateway, QString dns1, QString dns2, QString netWorkCard);
    void setDHCPIpAddr(QString netWorkCard);
    void setNTPServer(QString ntpServer);
    QString netWorkCardMac;
    QString netWorkCardMacEth0 = "";
    QString netWorkCardMacWlan0 = "";
    QString netWorkCardAddr;
    QString netWorkCardMask;
    QString netWorkCardGW;
    QString netWorkCardDNS;
    bool eth0Available = false;
    bool wlan0Available = false;
    int SDCardMounted(QString mountPath, QString mmcName);
    QString readLine(QString fileName);
    float getCPUTemp();
    void getIPAddress(QString netWorkCard);
    bool getLinkDetected(QString networkCard);
    float getMemUsage();
    double getCurrentValue();
    float getCPUUsage();
    bool getStorage(QString deviceName);
    int internalStorageTotal;
    int internalStorageUsed;
    int internalStorageAvailable;
    int internalStoragePercentUse;

    int sdStorageTotal;
    int sdStorageUsed;
    int sdStorageAvailable;
    int sdStoragePercentUse;


    unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;

    QString getWiredService(QString macAddress);

    void connmanSetStaticIP(QString ipaddr, QString netmask, QString gateway, QString dns1, QString dns2, QString macAddress);
    void connmanSetDHCP(QString macAddress);
    void checkCard(QString phyNetworkName);
    void initCheckCard(QString phyNetworkName);

    void setDHCPIpAddr3(QString phyNetworkName);
    void setStaticIpAddr3(QString ipaddr, QString netmask, QString gateway, QString dns1, QString dns2, QString phyNetworkName);
    QString getCurrentNetworkName(QString phyNetworkName);
    QString cidrToNetmask(const QString &cidrStr);


signals:
    void restartNetwork();
    void newAddress();
    void lanPlugin(QString networkCard);
    void lanRemove(QString networkCard);
    void networkmsg(QString);
public slots:
    void resetNetwork();
private:
    QProcess* getAddressProcess;
    QProcess* getSystemInfoProcess;
    int last_worktime, last_idletime;
    QString readfile(QString fileName);
    bool checkAddress(QString address);
    int calPrefix(QString mask);
    static int bit_count(uint32_t i);
};

#endif // NETWORKMNG_H
