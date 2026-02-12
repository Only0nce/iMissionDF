#ifndef NETWORKMNG_H
#define NETWORKMNG_H
#include <QString>
#include <QObject>
#include "QProcess"

class NetworkMng : public QObject
{
    Q_OBJECT
public:
    explicit NetworkMng(QObject *parent = 0);
    virtual ~NetworkMng();
    QString getAddress(const QString &netWorkCard);
    //    void getIPAddress(QString netWorkCard);
    QString getTimezone();
    unsigned short toCidr(const char *ipAddress);
    void resetNtp();
    void setStaticIpAddr(QString ipaddr,QString netmask,QString gateway,QString dns1,QString dns2,QString netWorkCard);
    void setDHCPIpAddr(const QString &netWorkCard);
    void setNTPServer(const QString &ntpServer);
    QString netWorkCardMac;
    QString netWorkCardMacEth0 = "";
    QString netWorkCardMacWlan0 = "";
    QString netWorkCardAddr;
    QString netWorkCardMask;
    QString netWorkCardGW;
    QString netWorkCardDNS;
    bool eth0Available = false;
    bool wlan0Available = false;
    int SDCardMounted(QString mountPath,QString mmcName);
    QString readLine(const QString &fileName);
    float getCPUTemp();
    void getIPAddress(const QString &netWorkCard);
    bool getLinkDetected(const QString &networkCard);
    float getMemUsage();
    double getCurrentValue();
    float getCPUUsage();
    bool getStorage(const QString &deviceName);
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

    void connmanSetStaticIP(QString ipaddr, QString netmask,QString gateway,QString dns1,QString dns2,QString macAddress);
    void connmanSetDHCP(const QString &macAddress);
    void checkCard(const QString &phyNetworkName);
    void initCheckCard(const QString &phyNetworkName);

    void setDHCPIpAddr3(const QString &phyNetworkName);
    void setStaticIpAddr3(QString ipaddr,QString netmask,QString gateway,QString dns1,QString dns2,QString phyNetworkName);
    QString getCurrentNetworkName(const QString &phyNetworkName);


signals:
    void restartNetwork();
    void newAddress();
    void lanPlugin(const QString &networkCard);
    void lanRemove(const QString &networkCard);

public slots:
    void resetNetwork();
    void resetNetworknmcli();
private:
    QString phyName = "eth0";
    QProcess* getAddressProcess;
    QProcess* getSystemInfoProcess;
    int last_worktime, last_idletime;
    QString readfile(QString fileName);
    bool checkAddress(const QString &address);
    int calPrefix(const QString &mask);
    static int bit_count(uint32_t i);
};

#endif // NETWORKMNG_H
