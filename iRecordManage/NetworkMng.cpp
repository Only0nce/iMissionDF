#include "NetworkMng.h"
#include "QDebug"
#include <QFile>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;
NetworkMng::NetworkMng(QObject *parent) : QObject(parent)
{
    getAddressProcess = new QProcess(this);
    getSystemInfoProcess = new QProcess(this);

}
NetworkMng::~NetworkMng(){
    delete getAddressProcess;
    delete getSystemInfoProcess;
}

int NetworkMng::SDCardMounted(QString mountPath,QString mmcName)
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QString lsblkOut;
    int mounted = -1; // No SD Card
    arguments << "-c" << QString("lsblk | grep ") + mmcName;
    getSystemInfoProcess->start(prog , arguments);
    getSystemInfoProcess->waitForFinished();
    lsblkOut = QString(getSystemInfoProcess->readAll()).trimmed();
    arguments.clear();
    if(lsblkOut.contains(mountPath))
    {
        mounted = 1; // Mounted
    }
    else if(lsblkOut.contains(mmcName)) {
        mounted = 0; // No Mount
        // Mount SD Card
        arguments << "-c" << QString("mount /dev/") + mmcName + " " + mountPath;
        getSystemInfoProcess->start(prog , arguments);
        getSystemInfoProcess->waitForFinished();
        lsblkOut = QString(getSystemInfoProcess->readAll()).trimmed();
        arguments.clear();
        arguments << "-c" << QString("lsblk | grep ") + mmcName;
        getSystemInfoProcess->start(prog , arguments);
        getSystemInfoProcess->waitForFinished();
        lsblkOut = QString(getSystemInfoProcess->readAll()).trimmed();
        arguments.clear();
        if(lsblkOut.contains(mountPath))
        {
            mounted = 1; // Mounted
        }
        else
        {
            mounted = -2; // Can't Mount
        }
    }
    return mounted;
}


QString NetworkMng::readLine(QString fileName)
{
    QFile inputFile(fileName);
    inputFile.open(QIODevice::ReadOnly);
    if (!inputFile.isOpen())
        return "";

    QTextStream stream(&inputFile);
    QString line = stream.readLine();
    inputFile.close();
//    qDebug() << line;
    return line.trimmed();
}
void NetworkMng::getIPAddress(QString netWorkCard){
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    arguments << "-c" << QString("ifconfig %1 | grep 'inet ' | awk '{print $2}' | sed 's/addr://'").arg(netWorkCard);
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished(100);
    netWorkCardAddr = QString(getAddressProcess->readAll()).trimmed();
    qDebug() << "netWorkCardAddr:" << netWorkCardAddr;
    networkmsg(netWorkCardAddr);
    arguments.clear();
}
bool NetworkMng::getLinkDetected(QString networkCard)
{
    bool linkDetected = false;
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    arguments << "-c" << QString("ethtool %1 | grep \"Link detected\"").arg(networkCard);
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished(100);
    linkDetected = QString(getAddressProcess->readAll()).contains("Link detected: yes");
    arguments.clear();
    return  linkDetected;
}
float NetworkMng::getMemUsage(){
    QString memInfo;
    int totalMem;
    int useMem;
    QString arguments = QString("free | grep Mem | awk '{print $2,$3}' > /opt/iPatchSoftPhone/sysinfo/memInfo");
    system(arguments.toStdString().c_str());
    memInfo = readLine("/opt/iPatchSoftPhone/sysinfo/memInfo");
    arguments.clear();
    if(memInfo.split(" ").size() >= 2)
    {
        totalMem = QString(memInfo.split(" ").at(0)).toInt();
        useMem = QString(memInfo.split(" ").at(1)).toInt();
    }
    if(totalMem > useMem){
        return float((useMem*100.0)/totalMem);
    }
    return 0.0;
}

bool NetworkMng::getStorage(QString deviceName)
{
    QString arguments = QString("df | grep " + deviceName + " | awk '{print $2,$3,$4,$5}' | sed 's/%//' > /opt/iPatchSoftPhone/mmcinfo/" + deviceName);
    system(arguments.toStdString().c_str());
    QString memInfo = readLine("/opt/iPatchSoftPhone/mmcinfo/" + deviceName);
    if (deviceName.contains("ubuntu--vg-ubuntu--lv"))
    {
        if(memInfo.split(" ").size() >= 4){
            internalStorageTotal = QString(memInfo.split(" ").at(0)).toInt();
            internalStorageUsed = QString(memInfo.split(" ").at(1)).toInt();
            internalStorageAvailable  = QString(memInfo.split(" ").at(2)).toInt();
            internalStoragePercentUse  = QString(memInfo.split(" ").at(3)).toInt();
            return true;
        }
        internalStorageTotal = 0;
        internalStorageUsed = 0;
        internalStorageAvailable = 0;
        internalStoragePercentUse = 0;
    }
    else {
        if(memInfo.split(" ").size() >= 4){
            sdStorageTotal = QString(memInfo.split(" ").at(0)).toInt();
            sdStorageUsed = QString(memInfo.split(" ").at(1)).toInt();
            sdStorageAvailable  = QString(memInfo.split(" ").at(2)).toInt();
            sdStoragePercentUse  = QString(memInfo.split(" ").at(3)).toInt();
            return true;
        }
        sdStorageTotal = 0;
        sdStorageUsed = 0;
        sdStorageAvailable = 0;
        sdStoragePercentUse  = 0;
    }
    return false;
}



float NetworkMng::getCPUTemp()
{
    string val;
    string getval_str = "/sys/class/thermal/thermal_zone0/temp";
    ifstream getval(getval_str.c_str());
    if (getval.fail()){
        cout << " OPERATION FAILED: Unable to getCPUTemp."<< endl;
        return 0.0;
            }

    getval >> val ;  //read gpio value
    getval.close(); //close the value file
    return float((QString::fromStdString(val).toFloat())/1000.0);
}

double NetworkMng::getCurrentValue(){
    double percent;
    FILE* file;
    unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

    file = fopen("/proc/stat", "r");
    fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow,
        &totalSys, &totalIdle);
    fclose(file);

    if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
        totalSys < lastTotalSys || totalIdle < lastTotalIdle){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else{
        total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
            (totalSys - lastTotalSys);
        percent = total;
        total += (totalIdle - lastTotalIdle);
        percent /= total;
        percent *= 100;
    }

    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;

    return percent;
}
float NetworkMng::getCPUUsage(){
    float rate;
    string val0;
    long val1, val2, val3, val4, val5;
    string getval_str = "/proc/stat";
    ifstream getval(getval_str.c_str());
    if (getval.fail()){
        cout << " OPERATION FAILED: Unable to getCPUUsage." << endl;
        return 0.0;
            }

    getval >> val0 >> val1 >> val2 >> val3 >> val4 >> val5 ;
    getval.close(); //close the value file
    if (QString::fromStdString(val0) == "cpu"){
        long worktime, idletime;
        worktime = val1+val2+val3;
        idletime = val4;
        float dworktime = (worktime-last_worktime);
        float didletime = (idletime-last_idletime);
        rate = float(dworktime)/(didletime+dworktime);
        last_worktime=worktime;
        last_idletime=idletime;
        if(last_worktime==0)
            return 0;
    }
    else
    {
        return 0.0;
    }
    if (rate < 0) rate = rate*(-1);
    return rate*100.0;
}

QString NetworkMng::cidrToNetmask(const QString &cidrStr) {
    bool ok;
    int cidr = cidrStr.toInt(&ok);
    if (!ok || cidr < 0 || cidr > 32)
        return "0.0.0.0";

    quint32 mask = cidr == 0 ? 0 : (0xFFFFFFFF << (32 - cidr));
    return QString("%1.%2.%3.%4")
        .arg((mask >> 24) & 0xFF)
        .arg((mask >> 16) & 0xFF)
        .arg((mask >> 8) & 0xFF)
        .arg(mask & 0xFF);
}

QString NetworkMng::getAddress(QString netWorkCard) {
    QString networkInfo;
    QString prog = "/bin/bash";
    QStringList arguments;

    qDebug() << "getAddress:" << netWorkCard;

    // --- Get IP Address ---
    arguments << "-c" << QString("ip -4 addr show %1 | grep -oP '(?<=inet\\s)\\d+(\\.\\d+){3}'").arg(netWorkCard);
    getAddressProcess->start(prog, arguments);
    getAddressProcess->waitForFinished(1000);
    netWorkCardAddr = QString(getAddressProcess->readAll()).trimmed();
    qDebug() << "ip address:" << netWorkCardAddr;

    // --- Get MAC Address ---
    arguments.clear();
    arguments << "-c" << QString("cat /sys/class/net/%1/address").arg(netWorkCard);
    getAddressProcess->start(prog, arguments);
    getAddressProcess->waitForFinished(1000);
    netWorkCardMac = QString(getAddressProcess->readAll()).trimmed();
    qDebug() << "mac address:" << netWorkCardMac;

    if (netWorkCard == "eth0")
        netWorkCardMacEth0 = netWorkCardMac;
    else if (netWorkCard == "wlan0")
        netWorkCardMacWlan0 = netWorkCardMac;

    // --- Get Netmask ---
    arguments.clear();
    arguments << "-c" << QString("ip -4 addr show %1 | grep -oP '(?<=inet\\s)\\d+(\\.\\d+){3}/\\d+' | cut -d'/' -f2").arg(netWorkCard);
    getAddressProcess->start(prog, arguments);
    getAddressProcess->waitForFinished(1000);
    QString cidr = QString(getAddressProcess->readAll()).trimmed();
    netWorkCardMask = cidrToNetmask(cidr);
    qDebug() << "netmask address:" << netWorkCardMask;

    // --- Get Default Gateway ---
    arguments.clear();
    arguments << "-c" << QString("ip route show default 0.0.0.0/0 | grep -oP '(?<=via\\s)\\d+(\\.\\d+){3}'");
    getAddressProcess->start(prog, arguments);
    getAddressProcess->waitForFinished(1000);
    netWorkCardGW = QString(getAddressProcess->readAll()).trimmed();
    qDebug() << "gateway:" << netWorkCardGW;

    // --- Get DNS Servers ---
    arguments.clear();
    arguments << "-c" << "grep nameserver /etc/resolv.conf | awk '{print $2}'";
    getAddressProcess->start(prog, arguments);
    getAddressProcess->waitForFinished(1000);
    netWorkCardDNS = QString(getAddressProcess->readAll()).trimmed().replace("\n", " ");
    qDebug() << "DNS server(s):" << netWorkCardDNS;

    QJsonObject mainObject;
    mainObject.insert("objectName", "getAddress");
    mainObject.insert("IPAddress", netWorkCardAddr);
    mainObject.insert("Netmask", netWorkCardMask);
    mainObject.insert("Gateway", netWorkCardGW);
    mainObject.insert("Server", netWorkCardDNS);
    mainObject.insert("MacAddress", netWorkCardMac);
    QJsonDocument jsonDoc(mainObject);
    networkInfo = jsonDoc.toJson(QJsonDocument::Compact);

    qDebug() << networkInfo;
    emit networkmsg(networkInfo);
    return networkInfo;
}


void NetworkMng::resetNetwork(){
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    qDebug() << "networking restarting";
    arguments << "-c" << QString("ifconfig eth0 down");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();

    arguments.clear();
    arguments << "-c" << QString("ifconfig eth0 up");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();

    arguments.clear();
    arguments << "-c" << QString("netplan apply");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();

    qDebug() << "networking restarted";

    getAddress("eth0");
}

void NetworkMng::resetNtp(){
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    arguments << "-c" << QString("systemctl daemon-reload");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();

    arguments.clear();
    arguments << "-c" << QString("systemctl restart systemd-timesyncd");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();
    arguments.clear();

    for(int j=0; j < 10000; j++){
        for(int i=0; i < 10000; i++){}
    }

    arguments.clear();
    arguments << "-c" << QString("systemctl restart systemd-timesyncd");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();
    arguments.clear();
    qDebug() << "systemctl restart systemd-timesyncd";

}

void NetworkMng::setNTPServer(QString ntpServer){
    QString filename = "/etc/systemd/timesyncd.conf";
    QString data;
    if (ntpServer != "0.0.0.0")
        data = QString("[Time]\n"
                   "NTP=%1\n"
                   "FallbackNTP=0.debian.pool.ntp.org 1.debian.pool.ntp.org 2.debian.pool.ntp.org 3.debian.pool.ntp.org\n").arg(ntpServer);
    else
        data = QString("[Time]\n"
                   "#NTP=%1\n"
                   "#FallbackNTP=0.debian.pool.ntp.org 1.debian.pool.ntp.org 2.debian.pool.ntp.org 3.debian.pool.ntp.org\n").arg(ntpServer);

    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    qDebug() << ntpServer;
    file.close();
    resetNtp();
}
QString NetworkMng::getTimezone()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QString Timezone;
    arguments << "-c" << QString("ls -la /etc/localtime | grep '/usr/share/zoneinfo/' | awk '{print $11}'");
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();
    Timezone = QString(getAddressProcess->readAll()).trimmed();
    arguments.clear();
    qDebug() << "Timezone" << Timezone;
    return Timezone.replace("/usr/share/zoneinfo/","");
}
unsigned short NetworkMng::toCidr(const char* ipAddress)
{
    unsigned short netmask_cidr;
    int ipbytes[4];

    netmask_cidr=0;
    sscanf(ipAddress, "%d.%d.%d.%d", &ipbytes[0], &ipbytes[1], &ipbytes[2], &ipbytes[3]);

    for (int i=0; i<4; i++)
    {
        switch(ipbytes[i])
        {
            case 0x80:
                netmask_cidr+=1;
                break;

            case 0xC0:
                netmask_cidr+=2;
                break;

            case 0xE0:
                netmask_cidr+=3;
                break;

            case 0xF0:
                netmask_cidr+=4;
                break;

            case 0xF8:
                netmask_cidr+=5;
                break;

            case 0xFC:
                netmask_cidr+=6;
                break;

            case 0xFE:
                netmask_cidr+=7;
                break;

            case 0xFF:
                netmask_cidr+=8;
                break;

            default:
                return netmask_cidr;
                break;
        }
    }

    return netmask_cidr;
}

void NetworkMng::setStaticIpAddr(QString ipaddr,QString netmask,QString gateway,QString dns1,QString dns2, QString netWorkCard){
    QString filename = "/etc/netplan/00-installer-config.yaml";
    if (netWorkCard == "wlan0")
        filename = "/etc/netplan/01-installer-config.yaml";
    QString data;
    unsigned short netmask_cidr = toCidr(netmask.toStdString().c_str());
    if (netmask_cidr <= 0) netmask_cidr = 24;
    if(((ipaddr!="0.0.0.0") && (netmask_cidr > 0) && (gateway!="0.0.0.0") && (dns1!="0.0.0.0") && (dns2!="0.0.0.0"))){
        data = QString("network:\n"
                       "  ethernets:\n"
                       "    %1:\n"
                       "      addresses: [%2/%3]\n"
                       "      dhcp4: no\n"
                       "      gateway4: %4\n"
                       "      nameservers:\n"
                       "        addresses:\n"
                       "          - %5\n"
                       "          - %6\n"
                       "  version: 2"
                       "")
                .arg(netWorkCard).arg(ipaddr).arg(netmask_cidr).arg(gateway).arg(dns1).arg(dns2);
    }
    else if(((ipaddr!="0.0.0.0") && (netmask_cidr > 0) && (gateway!="0.0.0.0") && (dns1=="0.0.0.0") && (dns2!="0.0.0.0")) ||
            ((ipaddr!="0.0.0.0") && (netmask_cidr > 0) && (gateway!="0.0.0.0") && (dns1!="0.0.0.0") && (dns2=="0.0.0.0")))
    {
        if (dns1=="0.0.0.0") dns1 = dns2;
        data = QString("network:\n"
                       "  ethernets:\n"
                       "    %1:\n"
                       "      addresses: [%2/%3]\n"
                       "      dhcp4: no\n"
                       "      gateway4: %4\n"
                       "      nameservers:\n"
                       "        addresses:\n"
                       "          - %5\n"
                       "  version: 2"
                       "")
                .arg(netWorkCard).arg(ipaddr).arg(netmask_cidr).arg(gateway).arg(dns1);
    }
    else if((ipaddr!="0.0.0.0") && (netmask_cidr > 0) && (gateway!="0.0.0.0") && (dns1=="0.0.0.0") && (dns2=="0.0.0.0")){
        data = QString("network:\n"
                       "  ethernets:\n"
                       "    %1:\n"
                       "      addresses: [%2/%3]\n"
                       "      dhcp4: no\n"
                       "      gateway4: %4\n"
                       "  version: 2"
                       "")
                .arg(netWorkCard).arg(ipaddr).arg(netmask_cidr).arg(gateway);
    }
    else if((ipaddr!="0.0.0.0") && (netmask_cidr > 0) && (gateway=="0.0.0.0")){
        data = QString("network:\n"
                       "  ethernets:\n"
                       "    %1:\n"
                       "      addresses: [%2/%3]\n"
                       "      dhcp4: no\n"
                       "  version: 2"
                       "")
                .arg(netWorkCard).arg(ipaddr).arg(netmask_cidr);
    }
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    system("netplan apply");
}

void NetworkMng::setDHCPIpAddr(QString netWorkCard)
{
    QString filename = "/etc/netplan/00-installer-config.yaml";
    QString data = QString("network:\n"
                   "  ethernets:\n"
                   "    %1:\n"
                   "      dhcp4: yes\n"
                   "  version: 2"
                   "")
            .arg(netWorkCard);

    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    system("netplan apply");
}


QString NetworkMng::getWiredService(QString macAddress)
{
    QString service = "";
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    macAddress = macAddress.replace(":","");
    arguments << "-c" << QString("connmanctl services | grep -i %1 | awk '{print $3}'").arg(macAddress);
//    qDebug() << arguments;
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();
    service = QString(getAddressProcess->readAll()).trimmed();
    arguments.clear();
//    qDebug() << macAddress << "service" << service;
    return service;
}

QString NetworkMng::readfile(QString fileName)
{
    string val;
    string getval_str = fileName.toStdString();
    ifstream getvalgpio(getval_str.c_str());
    if (getvalgpio.fail()){
        return "";
            }

    getvalgpio >> val ;
    getvalgpio.close();
//    qDebug() << QString::fromStdString(val);
    return (QString::fromStdString(val));
}
void NetworkMng::initCheckCard(QString phyNetworkName)
{
    if (phyNetworkName == "eth0"){
        if (readfile("/sys/class/net/eth0/operstate").contains("up") == false)
        {
            eth0Available = false;
            emit lanRemove(phyNetworkName);
        }
        else if  (readfile("/sys/class/net/eth0/operstate").contains("up"))
        {
            eth0Available = true;
            emit lanPlugin(phyNetworkName);
        }
    }
    else if (phyNetworkName == "wlan0"){
        if (readfile("/sys/class/net/wlan0/operstate").contains("up") == false)
        {
            wlan0Available = false;
            emit lanRemove(phyNetworkName);
        }
        else if (readfile("/sys/class/net/wlan0/operstate").contains("up"))
        {
            wlan0Available = true;
            emit lanPlugin(phyNetworkName);
        }
    }

}
void NetworkMng::checkCard(QString phyNetworkName)
{
    if (phyNetworkName == "eth0"){
        if ((eth0Available) & (readfile("/sys/class/net/eth0/operstate").contains("up") == false))
        {
            eth0Available = false;
            emit lanRemove(phyNetworkName);
        }
        else if ((eth0Available == false) & (readfile("/sys/class/net/eth0/operstate").contains("up")))
        {
            eth0Available = true;
            emit lanPlugin(phyNetworkName);
        }
    }
    else if (phyNetworkName == "wlan0"){
        if ((wlan0Available) & (readfile("/sys/class/net/wlan0/operstate").contains("up") == false))
        {
            wlan0Available = false;
            emit lanRemove(phyNetworkName);
        }
        else if ((wlan0Available == false) & (readfile("/sys/class/net/wlan0/operstate").contains("up")))
        {
            wlan0Available = true;
            emit lanPlugin(phyNetworkName);
        }
    }

}
void NetworkMng::setDHCPIpAddr3(QString phyNetworkName)
{
    QString connectionName = getCurrentNetworkName(phyNetworkName);

    if(connectionName == "") return;

    QString prog = "/bin/bash";//shell
    QStringList arguments;
    arguments << "-c" << QString("sudo nmcli connection modify \"%1\" ipv4.method auto ipv4.addresses '' ipv4.gateway ''").arg(connectionName);
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();

    arguments.clear();
    arguments << "-c" << QString("sudo nmcli dev connect %1").arg(phyNetworkName);
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();
    arguments.clear();
}

void NetworkMng::connmanSetDHCP(QString phyNetworkName)
{
    QString command;
    QString macAddress;
    if ((phyNetworkName == "eth0") & (netWorkCardMacEth0 != "")){
        netWorkCardMac = netWorkCardMacEth0;
    }
    else if ((phyNetworkName == "wlan0") & (netWorkCardMacWlan0 != "")){
        netWorkCardMac = netWorkCardMacWlan0;
    }
    else
    {
        getAddress(phyNetworkName);
    }

    macAddress = netWorkCardMac;
    QString service = getWiredService(macAddress);

    QString prog = "/bin/bash";//shell
    QStringList arguments;

    if (service == "") return;
    command = QString("connmanctl config %1 ipv4 dhcp").arg(service);
    system(command.toStdString().c_str());
}
void NetworkMng::connmanSetStaticIP(QString ipaddr,QString netmask,QString gateway,QString dns1,QString dns2, QString phyNetworkName)
{
    QString command;
    QString macAddress;
    if ((phyNetworkName == "eth0") & (netWorkCardMacEth0 != "")){
        netWorkCardMac = netWorkCardMacEth0;
    }
    else if ((phyNetworkName == "wlan0") & (netWorkCardMacWlan0 != "")){
        netWorkCardMac = netWorkCardMacWlan0;
    }
    else
    {
        getAddress(phyNetworkName);
    }

    macAddress = netWorkCardMac;
    QString service = getWiredService(macAddress);

    QString prog = "/bin/bash";//shell
    QStringList arguments;

    getAddressProcess->kill();

    if (service == "") return;
    if (netmask == "0.0.0.0") netmask = "255.255.255.0";
    if (gateway == "0.0.0.0") gateway = "";
    if (dns1 == "0.0.0.0") dns1 = "";
    if (dns2 == "0.0.0.0") dns2 = "";
    if(ipaddr!="0.0.0.0")
    {
        command = QString("connmanctl config %1 --ipv4 manual %2 %3 %4").arg(service).arg(ipaddr).arg(netmask).arg(gateway);
        system(command.toStdString().c_str());

        command = QString("connmanctl config %1 --nameservers %2 %3").arg(service).arg(dns1).arg(dns2);
        system(command.toStdString().c_str());

        QString ifconfigCommand = QString("ifconfig %1 %2").arg(phyNetworkName).arg(ipaddr);
        system(ifconfigCommand.toStdString().c_str());
    }
    else
    {
        command = QString("connmanctl config %1 ipv4 dhcp").arg(service);
        system(command.toStdString().c_str());
    }
}
int NetworkMng::bit_count(uint32_t i)
{
    QString valueInHex= QString("%1").arg(i , 0, 16);

    int c = 0;
    unsigned int seen_one = 0;

    while (i > 0) {
        if (i & 1) {
            seen_one = 1;
            c++;
        } else {
            if (seen_one) {
                return -1;
            }
        }
        i >>= 1;
    }

    qDebug() << "value = " << valueInHex << c;
    return c;
}
int NetworkMng::calPrefix(QString mask)
{
    uint32_t longMask = 0;
    int prefix = 24;
    QStringList sListMask = mask.split(".");
    uint32_t listMask[4] = {0,0,0,0};
    for (int i=0; i<sListMask.length();i++)
    {
        listMask[i] = sListMask.at(i).toInt();
    }
    longMask = listMask[0] << 24 | listMask[1] << 16 | listMask[2] << 8 | listMask[3] << 0;
    qDebug() << "sListMask" << sListMask << listMask[0] << listMask[1] << listMask[2] << listMask[3] << longMask;
    return bit_count(longMask);
}
bool NetworkMng::checkAddress(QString address)
{
    QStringList _address = address.split(".");
    if (_address.size() != 4) return false;
    if ((_address.at(0).toInt() > 255) || (_address.at(1).toInt() > 255)  || (_address.at(2).toInt() > 255) || (_address.at(3).toInt() > 255))  return false;
    return true;
}
QString NetworkMng::getCurrentNetworkName(QString phyNetworkName)
{
    QString connectionName = "";

    QString prog = "/bin/bash";//shell
    QStringList arguments;
    arguments << "-c" << QString("sudo nmcli -t -f NAME,DEVICE connection  | grep %1").arg(phyNetworkName);
    getAddressProcess->start(prog , arguments);
    getAddressProcess->waitForFinished();
    connectionName = QString(getAddressProcess->readAll()).trimmed();
    arguments.clear();
    return connectionName.split(":").at(0);
}
void NetworkMng::setStaticIpAddr3(QString ipaddr,QString netmask,QString gateway,QString dns1,QString dns2, QString phyNetworkName)
{
    QString connectionName = getCurrentNetworkName(phyNetworkName);
    qDebug() << "setStaticIpAddr3:" << connectionName;

    if(connectionName == "") return;
    qDebug() << "netmask" << netmask;
    int netMaskPrefix = calPrefix(netmask);
    if (netmask == "0.0.0.0") netMaskPrefix = 24;
    if ((ipaddr == "0.0.0.0") || (ipaddr == ""))  return;
    if (gateway == "") gateway = "0.0.0.0";
    if (dns1 == "") dns1 = "0.0.0.0";
    if (dns2 == "") dns2 = "0.0.0.0";

    if (checkAddress(ipaddr) == false) return;
    if (checkAddress(netmask) == false) return;
    if (checkAddress(gateway) == false) return;
    if (checkAddress(dns1) == false) return;
    if (checkAddress(dns2) == false) return;
    if(((ipaddr!="0.0.0.0") &&  (gateway!="0.0.0.0") && (dns1!="0.0.0.0") && (dns2!="0.0.0.0")))
    {
        QString nmcliCmd = QString("sudo nmcli connection modify \"%1\" ipv4.method manual ipv4.addresses %2/%3 ipv4.gateway %4 ipv4.dns %5,%6").arg(connectionName).arg(ipaddr).arg(netMaskPrefix).arg(gateway).arg(dns1).arg(dns2);
        system(nmcliCmd.toStdString().c_str());

        nmcliCmd = QString("sudo nmcli dev connect %1").arg(phyNetworkName);
        system(nmcliCmd.toStdString().c_str());
    }
    else if(((ipaddr!="0.0.0.0") &&  (gateway!="0.0.0.0") && (dns1!="0.0.0.0") && (dns2=="0.0.0.0")))
    {
        QString nmcliCmd = QString("sudo nmcli connection modify \"%1\" ipv4.method manual ipv4.addresses %2/%3 ipv4.gateway %4 ipv4.dns %5").arg(connectionName).arg(ipaddr).arg(netMaskPrefix).arg(gateway).arg(dns1);
        system(nmcliCmd.toStdString().c_str());

        nmcliCmd = QString("sudo nmcli dev connect %1").arg(phyNetworkName);
        system(nmcliCmd.toStdString().c_str());
    }
    else if(((ipaddr!="0.0.0.0") &&  (gateway!="0.0.0.0") && (dns1=="0.0.0.0") && (dns2!="0.0.0.0")))
    {
        QString nmcliCmd = QString("sudo nmcli connection modify \"%1\" ipv4.method manual ipv4.addresses %2/%3 ipv4.gateway %4 ipv4.dns %5").arg(connectionName).arg(ipaddr).arg(netMaskPrefix).arg(gateway).arg(dns2);
        system(nmcliCmd.toStdString().c_str());

        nmcliCmd = QString("sudo nmcli dev connect %1").arg(phyNetworkName);
        system(nmcliCmd.toStdString().c_str());
    }
    else if((ipaddr!="0.0.0.0") && (gateway!="0.0.0.0") && (dns1=="0.0.0.0") && (dns2=="0.0.0.0")){
        QString nmcliCmd = QString("sudo nmcli connection modify \"%1\" ipv4.method manual ipv4.addresses %2/%3 ipv4.gateway %4").arg(connectionName).arg(ipaddr).arg(netMaskPrefix).arg(gateway);
        system(nmcliCmd.toStdString().c_str());

        nmcliCmd = QString("sudo nmcli dev connect %1").arg(phyNetworkName);
        system(nmcliCmd.toStdString().c_str());
    }
    else if((ipaddr!="0.0.0.0") && (gateway=="0.0.0.0"))
    {
        QString nmcliCmd = QString("sudo nmcli connection modify \"%1\" ipv4.method manual ipv4.addresses %2/%3").arg(connectionName).arg(ipaddr).arg(netMaskPrefix);
        system(nmcliCmd.toStdString().c_str());

        nmcliCmd = QString("sudo nmcli dev connect %1").arg(phyNetworkName);
        system(nmcliCmd.toStdString().c_str());
    }
    else if(ipaddr!="0.0.0.0")
    {
        QString nmcliCmd = QString("sudo nmcli connection modify \"%1\" ipv4.method manual ipv4.addresses %2/24").arg(connectionName).arg(ipaddr);
        system(nmcliCmd.toStdString().c_str());

        nmcliCmd = QString("sudo nmcli dev connect %1").arg(phyNetworkName);
        system(nmcliCmd.toStdString().c_str());
    }
}
