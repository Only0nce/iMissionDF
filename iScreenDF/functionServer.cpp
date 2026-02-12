#include "iScreenDF.h"

void iScreenDF::newCommandProcess(const QJsonObject &command, QWebSocket *pSender,const QString &message)
{
    QByteArray br = message.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QString menuID = obj["menuID"].toString();
    QString broadcastID =  QJsonValue(obj["broadcastID"]).toString();
    QString objectName = QJsonValue(obj["objectName"]).toString();

    if (!broadcastID.isEmpty())
    {
        handleBroadcastMessage(obj);
    }
    if (objectName == "AddDevice")
    {
        QString name      = obj["Name"].toString();
        QString ip        = obj["ip"].toString();
        QString deviceUid = obj["deviceUniqueId"].toString();

        qDebug() << "[functionServer] AddDevice =>"
                 << "name:" << name
                 << "ip:"   << ip
                 << "deviceUniqueId:" << deviceUid;

        if (!db) {
            qWarning() << "[functionServer] db is null";
            return;
        }

        QTimer::singleShot(0, db, [db = db, name, ip, deviceUid]() {
            db->addNewDevice(name, ip, deviceUid);
        });
    }
    else if (objectName == "UpdateDevice")
    {
        QString name  = obj["Name"].toString();
        QString ip    = obj["ip"].toString();

        QString oldUid = obj["oldDeviceUniqueId"].toString();
        QString newUid = obj["deviceUniqueId"].toString();

        if (oldUid.isEmpty())
            oldUid = newUid;

        qDebug() << "[functionServer] UpdateDevice =>"
                 << "oldUid:" << oldUid
                 << "newUid:" << newUid
                 << "Name:"   << name
                 << "ip:"     << ip;

        QTimer::singleShot(0, db, [db = db, oldUid, newUid, name, ip]() {
            db->updateDeviceByUniqueId(oldUid, newUid, name, ip);
        });
    }
    else if (objectName == "DeleteDevice")
    {
        int id        = obj["id"].toInt();
        QString name  = obj["Name"].toString();
        QString ip    = obj["ip"].toString();
        QString deviceUniqueId = obj["deviceUniqueId"].toString();
        QTimer::singleShot(0, db, [db = db,deviceUniqueId]() {
            db->deleteDeviceByUniqueId(deviceUniqueId);
        });
    }
    else if (objectName == "EditGroup")
    {
        QString action          = obj["action"].toString();
        int     groupID         = obj["groupID"].toInt(-1);
        QString groupName       = obj["groupName"].toString();
        QString uniqueIdInGroup = obj["uniqueIdInGroup"].toString();
        QJsonArray duidArray    = obj["deviceUniqueIds"].toArray();

        QList<QString> deviceUniqueIds;
        deviceUniqueIds.reserve(duidArray.size());
        for (const QJsonValue &v : duidArray) {
            QString duid = v.toString().trimmed();
            if (!duid.isEmpty())
                deviceUniqueIds.append(duid);
        }

        qDebug() << "[functionServer] EditGroup =" << action
                 << "groupID =" << groupID
                 << "groupName =" << groupName
                 << "uniqueIdInGroup =" << uniqueIdInGroup
                 << "deviceUniqueIds =" << deviceUniqueIds;

        if (action == "add") {
            if (uniqueIdInGroup.isEmpty()) {
                db->savegroupSettingNewGroup(groupName,
                                             deviceUniqueIds,
                                             groupID,
                                             uniqueIdInGroup);
            } else {
                qDebug() << "[functionServer] EditGroup add on existing group uid="
                         << uniqueIdInGroup << " not implemented yet.";
            }
        } else if (action == "delete") {
            db->deleteGroupByUID(uniqueIdInGroup);
        }
    }
    else if (objectName == "EditGroupDevices")
    {
        QString action          = obj["action"].toString();
        int     groupID         = obj["groupID"].toInt(-1);
        QString groupName       = obj["groupName"].toString();
        QString uniqueIdInGroup = obj["uniqueIdInGroup"].toString();

        QString deviceUniqueId  = obj["deviceUniqueId"].toString();
        int     roleIndex       = obj["roleIndex"].toInt(-1); // ⭐ DeviceGroups.id เดิม

        QJsonArray devUidArray  = obj["deviceUniqueIds"].toArray();
        QStringList deviceUniqueIds;
        deviceUniqueIds.reserve(devUidArray.size());
        for (const QJsonValue &v : devUidArray) {
            const QString duid = v.toString();
            if (!duid.isEmpty())
                deviceUniqueIds.append(duid);
        }

        qDebug() << "[functionServer] EditGroupDevices ="
                 << "action=" << action
                 << "groupID=" << groupID
                 << "deviceUniqueId=" << deviceUniqueId
                 << "uid=" << uniqueIdInGroup
                 << "roleIndex(id)=" << roleIndex
                 << "deviceUniqueIds=" << deviceUniqueIds;

        if (action == "add") {
            if (deviceUniqueId.isEmpty()) {
                qWarning() << "[functionServer] EditGroupDevices(add): deviceUniqueId is empty, skip";
            } else {
                db->insertDevicesinGroup(groupID,groupName,deviceUniqueId,uniqueIdInGroup);
            }
        } else if (action == "remove") {
            if (deviceUniqueId.isEmpty()) {
                qWarning() << "[functionServer] EditGroupDevices(remove): deviceUniqueId is empty, skip";
            } else {
                db->removeDeviceFromGroup(groupID,deviceUniqueId,uniqueIdInGroup);
            }
        } else if (action == "changedeviceInGroup") {

            if (roleIndex < 0 || deviceUniqueId.isEmpty()) {
                qWarning() << "[functionServer] EditGroupDevices(change): invalid roleIndex or empty duid";
            } else {
                QTimer::singleShot(0, db, [db = db,groupID,groupName,deviceUniqueId,roleIndex,uniqueIdInGroup]() {
                    db->updateDeviceInGroup(groupID,groupName,deviceUniqueId,roleIndex,uniqueIdInGroup);
                });
            }
        }
    }
    else if (objectName == "editName")
    {
        int id = obj["id"].toInt();
        QString GroupsName = obj["GroupsName"].toString();
        QString uniqueIdInGroup = obj["uniqueIdInGroup"].toString();
        QTimer::singleShot(0, db, [db = db, uniqueIdInGroup,GroupsName]() {
            db->editGroupName(uniqueIdInGroup, GroupsName);
        });
        // db->editGroupName(id, GroupsName);
    }
    if(menuID == "getSystem")
    {
        hardwareInfo();
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("menuID", "network");  // Name
        Param.insert("ipaddress", networks->ip_address);
        Param.insert("gateway", networks->ip_gateway);
        Param.insert("subnet", networks->subnet);
        if (networks->dhcpmethod == 0)
            Param.insert("dhcpmethod", "on");
        else
            Param.insert("dhcpmethod", "off");
        Param.insert("pridns", networks->pridns);
        Param.insert("secdns", networks->secdns);
        Param.insert("phyNetworkName","eth0");
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        chatServerDF->broadcastMessage(raw_data);
    }
    else if(menuID == "updateNTPServer")
    {
        //        qDebug() << "updateNTPServer";
        networks->ip_timeserver = obj["ntpServer"].toString();
        //        qDebug() << "updateNTPServer:" << networks->ip_timeserver;
        networking->setNTPServer(networks->ip_timeserver);
        db->setNTPServer(networks->ip_timeserver);
    }
    else if(menuID == "rebootSystem")
    {
        QString reboot = QString("reboot");
        system(reboot.toStdString().c_str());
    }
    else if(menuID == "updateFirmware")
    {
        //        qDebug() << "updateFirmware";
        QThread::msleep(100);
        updateFirmware();
    }
    else if(menuID == "setLocation")
    {
        //        qDebug() << "setLocation";
        networks->location = obj["location"].toString();
        //        qDebug() << "updateNTPServer:" << networks->location;
        QString cmd = QString("sudo timedatectl set-timezone %1").arg(networks->location);
        system(cmd.toUtf8());
        QTimer::singleShot(0, db, [db = db, loc = networks->location]() {
            db->setNTPServerLocation(loc);
        });
        // db->setNTPServerLocation(networks->location);
    }
    else if(menuID == "updateLocalNetwork")
    {
        networks->phyName = obj["phyNetworkName"].toString();
        networks->dhcpmethod = obj["dhcpmethod"].toString();
        networks->ip_address = obj["ipaddress"].toString();
        networks->subnet = obj["subnet"].toString();
        networks->ip_gateway = obj["gateway"].toString();
        networks->pridns = obj["pridns"].toString();
        networks->secdns = obj["secdns"].toString();
        // chartclient->ip_address = networks->ip_address;
        qDebug() << "[functionServer] updateLocalNetwork:" << message;
        networking->setDHCPIpAddr3(networks->phyName);
        networking->setStaticIpAddr3(networks->ip_address,networks->subnet,networks->ip_gateway,networks->pridns,networks->secdns,networks->phyName);
        // emit setNetwork(networks->dhcpmethod,networks->ip_address,networks->subnet,networks->ip_gateway,networks->pridns,networks->secdns,networks->krakenserver);
        // setupNetworktoDisplay(networks->dhcpmethod,networks->ip_address,networks->subnet,networks->ip_gateway, networks->pridns,networks->secdns);
    }
    else if (menuID == "scanDevicesRange")
    {
        QString startIp = obj["startIp"].toString();
        QString endIp = obj["endIp"].toString();
        scanDevicesRange(startIp, endIp);
    }

    // else if (menuID == "GPS_Data") {
    // QString GPS_DateStr  = obj["GPS_Date"].toString();
    // QString GPS_TimeStr  = obj["GPS_Time"].toString();
    // QString latStr    = QString::number(obj["GPS_Lat"].toDouble(), 'f', 6);
    // QString lonStr    = QString::number(obj["GPS_Long"].toDouble(), 'f', 6);
    // QString altStr    = QString::number(obj["GPS_Alt"].toDouble(), 'f', 4);

    // qDebug() << "[functionServer] GPS_Data :" << GPS_Date << GPS_Time << latStr << lonStr << altStr;

    // emit updateLocationLatLongFromGPS(latStr, lonStr, altStr);
    // emit updatecurrentFromGPSTime(GPS_DateStr, GPS_TimeStr);
    // }
    // else if (menuID == "getName"){
    //     qDebug() << "getName";

    //     QJsonObject single;
    //     single["menuID"] = "PPAPDD";
    //     QString sendJson = QString::fromUtf8(QJsonDocument(single).toJson(QJsonDocument::Compact));
    //     chatServerDF->broadcastMessage(sendJson);
    // }
    // else if (menuID == "compass"){
    //     qDebug() << "compass";
    // if(!compassTimer->isActive()) {
    //     compassTimer->start(2000);
    //     qDebug() << "Compass timer started";
    // }
    // QJsonObject single;
    // single["objectName"] = "SetBaudrate";
    // single["baudrate"] = 4;
    // QString sendJson = QString::fromUtf8(QJsonDocument(single).toJson(QJsonDocument::Compact));
    // chatServer->broadcastMessage(sendJson);
    // }
    else if (menuID == "connectGroupSingle")
    {
        handleConnectGroupSingle(obj);
    }
    else if (menuID == "SetConnectionMode")
    {
        QString mode = obj["mode"].toString();
        setMode(mode);
    }
    else if (menuID == "getRolesPage")
    {
        QJsonObject obj;
        obj["menuID"]   = "updateParameterMode";
        obj["mode"]   = RemoteStatus;
        QJsonDocument doc(obj);
        const QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        chatServerDF->broadcastMessage(jsonStr);
        // QJsonDocument jsonDoc2;
        // QJsonObject Param2;
        // QString raw_data2;
        // Param2.insert("menuID", "updateCurrentRole");
        // Param2.insert("uniqueIdInGroup", uniqueIdInGroupSelected);
        // jsonDoc2.setObject(Param2);
        // raw_data2 = QJsonDocument(Param2).toJson(QJsonDocument::Compact).toStdString().c_str();
        // chatServerDF->broadcastMessage(raw_data2);
    }
    else if(menuID == "ChangeActiveRoleID")
    {

        QString roleID = obj["roleID"].toString();
        uniqueIdInGroupSelected = roleID;
        QTimer::singleShot(0, db, [db = db, roleID]() {
            db->getDevicesInGroupJson(roleID);
        });
    }
    else if (menuID == "DFLOG_delete_selected") {
        QJsonArray files = obj.value("files").toArray();

        QJsonArray results;
        int deleted = 0;

        for (const QJsonValue &v : files) {
            const QString rel = v.toString();
            QString reason;
            const bool ok = deleteRel(rel, &reason);
            if (ok) deleted++;

            // results.append(QJsonObject{
            //     {"rel", rel},
            //     {"ok", ok},
            //     {"reason", reason}
            // });
        }

        qDebug() << "[DFLOG selected]" << files.size() << "deleted=" << deleted;

        QJsonObject resp{
                         {"menuID","reloadweb"},
                         };
        sendResult(resp);
        return;
    }
    else if (menuID == "DFLOG_delete_one") {
        const QString rel = obj.value("rel").toString();

        QString reason;
        const bool ok = deleteRel(rel, &reason);

        qDebug() << "[DFLOG one]" << rel << ok << reason;
        QJsonObject resp{
                         {"menuID","reloadweb"},
                         };
        sendResult(resp);
        return;
    }
    else if (menuID == "SHOWIMG_delete_one") {
        const QString rel = obj.value("rel").toString();
        QString reason;
        const bool ok = deleteImageRel(rel, &reason);

        qDebug().noquote() << "[SHOWIMG_delete_one]" << rel << "ok=" << ok << "reason=" << reason;

        // ✅ หน้าเว็บต้องการ reload
        sendReloadWeb();
        return;
    }
    else if (menuID == "SHOWIMG_delete_selected") {
        const QJsonArray files = obj.value("files").toArray();

        int okCount = 0;
        for (const QJsonValue &v : files) {
            const QString rel = v.toString();
            QString reason;
            const bool ok = deleteImageRel(rel, &reason);
            if (ok) okCount++;

            qDebug().noquote() << "  - del" << rel << "ok=" << ok << "reason=" << reason;
        }

        qDebug().noquote() << "[SHOWIMG_delete_selected] total=" << files.size() << "deleted=" << okCount;

        // ✅ หน้าเว็บต้องการ reload
        sendReloadWeb();
        return;
    }
    // else {
    //      qDebug() << "[functionServer] else:" << message;
    // }

}

void iScreenDF::sendToWeb(const QString &data){
    // emit serverSendMessage(data);
}

void iScreenDF::updateFirmware() {
    foundfileupdate = true;
    QStringList fileupdate;
    fileupdate = findFile();
    system("sudo mkdir -p /tmp/update");

    qDebug() << "fileupdate.size()" << fileupdate.size();
    if (fileupdate.size() > 0) {
        qDebug() << "Start update";
        updateStatus = 1;
        QString sendMessage = QString("{\"menuID\":\"update\", \"updateStatus\":%1}").arg(updateStatus);
        QString commandCopyFile = "sudo cp " + QString(fileupdate.at(0)) + " /tmp/update/update.tar";
        system(commandCopyFile.toStdString().c_str());
        system("sudo tar -xf /tmp/update/update.tar -C /tmp/update/");
        system("sudo sh /tmp/update/update.sh");
        updateStatus = 2;
        sendMessage = QString("{\"menuID\":\"update\", \"updateStatus\":%1}").arg(updateStatus);
        qDebug() << "Update complete";
        system("sudo rm /var/www/html/uploads/*");
        exit(0);
    }
    foundfileupdate = false;
}

QStringList iScreenDF::findFile() {
    QStringList listfilename;
    QString ss = "/var/www/html/uploads/";
    const char* sss;
    sss = ss.toStdString().c_str();
    QDir dir1("/var/www/html/uploads/");
    QString filepath;
    QString filename;
    QFileInfoList fi1List(dir1.entryInfoList(QDir::Files, QDir::Name));
    foreach (const QFileInfo& fi1, fi1List) {
        filepath = QString::fromUtf8(fi1.absoluteFilePath().toLocal8Bit());
        filename = QString::fromUtf8(fi1.fileName().toLocal8Bit());
        listfilename << filepath;
        qDebug() << filepath;  // << filepath.toUtf8().toHex();
    }
    return listfilename;
}


void iScreenDF::hardwareInfo() {
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "system");
    // Param.insert("HwName", HwName);
    // Param.insert("HwVersion", HwVersion);
    // Param.insert("SwVersion", SwVersion);
    Param.insert("dateTimeMethod", networks->method_timeserver);
    Param.insert("ntpServer", networks->ip_timeserver);
    Param.insert("location", networks->location);
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    chatServerDF->broadcastMessage(raw_data);
}

void iScreenDF::updateNetworkSlot(const QString &dhcp,const QString &ip,const QString &subnet,const QString &gateway,const QString &primaryDns,const QString &secondaryDns,const QString &krakenserver)
{
    networks->dhcpmethod = dhcp;
    networks->ip_address = ip;
    networks->subnet = subnet;
    networks->ip_gateway = gateway;
    networks->pridns = primaryDns;
    networks->secdns = secondaryDns;
    // networks->printinfo();
    networks->krakenserver =krakenserver;
    capture->Gethost(krakenserver);
    // chartclient->ip_address = ip;

    // setupNetworktoDisplay(networks->dhcpmethod,networks->ip_address,networks->subnet,networks->ip_gateway, networks->pridns,networks->secdns);
    // setUpnetworkraken(networks->krakenserver,networks->ip_address,networks->subnet,networks->ip_gateway);
    // updateServerlogDB(networks->krakenserver);
}
void iScreenDF::updateNTPServerSlot(const QString &ip,const QString &location,const int &method){
    networks->location = location;
    networks->ip_timeserver = ip;
    networks->method_timeserver = method;
}

void iScreenDF::sendResult(const QJsonObject &o)
{
    if (!chatServerDF) return;
    chatServerDF->broadcastMessage(QString::fromUtf8(
        QJsonDocument(o).toJson(QJsonDocument::Compact)
        ));
}

void iScreenDF::cleanupEmptyDirs(const QString &absFilePath)
{
    QDir base(m_txBaseDir);
    const QString baseAbs = QDir(base.absolutePath()).absolutePath();

    QDir d(QFileInfo(absFilePath).absolutePath());
    while (true) {
        const QString dirAbs = QDir(d.absolutePath()).absolutePath();
        if (dirAbs == baseAbs) break;              // หยุดที่ baseDir
        if (!d.exists()) break;

        // มีรายการอื่นอยู่ -> หยุด
        if (!d.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
            break;

        // ลบโฟลเดอร์ว่าง
        const QString name = d.dirName();
        d.cdUp();
        d.rmdir(name);
    }
}

bool iScreenDF::deleteRel(const QString &relIn, QString *reasonOut)
{
    QString rel = relIn.trimmed();
    if (rel.isEmpty() || rel.contains("..")) { if(reasonOut) *reasonOut="invalid_rel"; return false; }

    QDir base(m_txBaseDir);
    const QString abs = base.absoluteFilePath(rel);

    QFileInfo fi(abs);
    if (fi.suffix().toLower() != "csv") { if(reasonOut) *reasonOut="not_allowed"; return false; }
    if (!fi.exists()) { if(reasonOut) *reasonOut="not_found"; return false; }
    if (!fi.isFile()) { if(reasonOut) *reasonOut="not_a_file"; return false; }

    QFile f(abs);
    if (!f.remove()) { if(reasonOut) *reasonOut="remove_failed:"+f.errorString(); return false; }

    cleanupEmptyDirs(abs);
    if (reasonOut) *reasonOut="deleted";
    return true;
}

static inline bool isAllowedImgExt(const QString &absPath)
{
    const QString ext = QFileInfo(absPath).suffix().toLower();
    return (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "webp");
}

bool iScreenDF::deleteImageRel(const QString &relIn, QString *reasonOut)
{
    QString rel = relIn.trimmed();
    rel.replace('\\', '/');
    while (rel.startsWith('/')) rel.remove(0, 1);

    if (rel.isEmpty() || rel.contains("..")) { if(reasonOut) *reasonOut="invalid_rel"; return false; }

    QDir base(m_imgBaseDir);
    const QString abs = base.absoluteFilePath(rel);

    if (!isAllowedImgExt(abs)) { if(reasonOut) *reasonOut="not_allowed"; return false; }

    QFileInfo fi(abs);
    if (!fi.exists()) { if(reasonOut) *reasonOut="not_found"; return false; }
    if (!fi.isFile()) { if(reasonOut) *reasonOut="not_a_file"; return false; }

    QFile f(abs);
    if (!f.remove()) { if(reasonOut) *reasonOut="remove_failed:" + f.errorString(); return false; }

    pruneEmptyDirs(abs);

    if (reasonOut) *reasonOut="deleted";
    return true;
}

void iScreenDF::pruneEmptyDirs(const QString &absFilePath)
{
    QDir base(m_imgBaseDir);
    const QString baseAbs = QDir(base.absolutePath()).absolutePath();

    QDir d(QFileInfo(absFilePath).absolutePath());
    while (true) {
        const QString dirAbs = QDir(d.absolutePath()).absolutePath();
        if (dirAbs == baseAbs) break; // ไม่ลบ baseDir

        // ถ้ามีอะไรอยู่ในโฟลเดอร์ -> หยุด
        if (!d.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
            break;

        const QString name = d.dirName();
        d.cdUp();
        d.rmdir(name);
    }
}

void iScreenDF::sendReloadWeb()
{
    if (!chatServerDF) return;
    QJsonObject resp{{"menuID","reloadweb"}};
    chatServerDF->broadcastMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)
        ));
}

// void iScreenDF::reConnectSlot(){
//     if(chartclient->isConnected == false){
//         chartclient->createConnection("127.0.0.1",8080);
//         qDebug() << "reConnect websocket";
//     }
//     // getUpdateSettingJSON->start();
//     QJsonDocument jsonDoc;
//     QJsonObject Param;
//     Param.insert("objectName", "keepAlive");
//     Param.insert("HwName", HwName);
//     jsonDoc.setObject(Param);
//     QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
//     emit sendToWeb(raw_data);
// }
