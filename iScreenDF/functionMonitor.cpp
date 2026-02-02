#include "iScreenDF.h"
#include "WorkerScan.h"

// GET FROM qml /sidepanels/SideGroup.qml
void iScreenDF::openPopupSetting(const QString &msg) {
    qDebug() << "[functionMonitor] :" <<  msg;
    emit  openPopupSettingRequested(msg);
    if (msg == "Group Management") {
        QTimer::singleShot(0, db, &DatabaseDF::getGroupsInGroupSetting);
        // db->getGroupsInGroupSetting();
        // emit  openPopupSettingRequested(msg);
    } else if (msg == "Add Device") {
        QTimer::singleShot(0, db, &DatabaseDF::getSideRemote);
        // db->getSideRemote();
        // emit  openPopupSettingRequested(msg);
    }
}

void iScreenDF::getdatabaseToSideSettingDrawer(const QString &msg) {
    qDebug() << "[functionMonitor] getdatabaseToSideSettingDrawer :" <<  msg;
    if (msg == "SideGroup") {
        QTimer::singleShot(0, db, &DatabaseDF::getRemoteGroups);
        // db->getRemoteGroups();
    }
    else if (msg == "SideRemote") {
        QTimer::singleShot(0, db, &DatabaseDF::getSideRemote);
        // db->getSideRemote();
    }
    else if (msg == "SideLocal")
    {
        QTimer::singleShot(0, db, &DatabaseDF::GetParameter);
        QTimer::singleShot(0, db, &DatabaseDF::GetrfsocParameter);
    }
}

void iScreenDF::groupSetting(const QString &title, int id, const QString &json)
{
    qDebug() << "[functionMonitor] GroupSetting :" << title << "ID :" << id << "Title :" << json;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON Error:" << err.errorString();
        return;
    }
    QJsonObject obj = doc.object();

    // if (title == "ALL_Group") {
    //     QTimer::singleShot(0, db, [db = db, json]() {
    //         db->saveGroupSettingFromJson(json);
    //     });

        // db->saveGroupSettingFromJson(json);
    //     return;
    // }
    // if (title == "EditGroupbyID") {

    //     QTimer::singleShot(0, db, [db = db, json]() {
    //         db->savegroupSettingBygroupID(json);
    //     });
    //     // db->savegroupSettingBygroupID(json);
    //     return;
    // }

    QJsonArray payload = obj.value("payload").toArray();
    if (payload.isEmpty()) {
        qWarning() << "No payload found!";
        return;
    }
    QJsonObject devObj = payload.first().toObject();

    if (title == "EditGroup"){
        QString action          = devObj.value("action").toString();
        int     groupID         = devObj.value("groupID").toInt(-1);
        QString groupName       = devObj.value("groupName").toString();
        QString uniqueIdInGroup = devObj.value("uniqueIdInGroup").toString();
        QJsonArray duidArray    = devObj.value("deviceUniqueIds").toArray();

        QList<QString> deviceUniqueIds;
        deviceUniqueIds.reserve(duidArray.size());
        for (const QJsonValue &v : duidArray) {
            QString duid = v.toString().trimmed();
            if (!duid.isEmpty())
                deviceUniqueIds.append(duid);
        }

        qDebug() << "[functionMonitor] EditGroup =" << action
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
                qDebug() << "[functionMonitor] EditGroup add on existing group uid="
                         << uniqueIdInGroup << " not implemented yet.";
            }
        } else if (action == "delete") {
            db->deleteGroupByUID(uniqueIdInGroup);
        }
    }
    else if (title == "EditGroupDevices"){
        QString action          = devObj.value("action").toString();
        int     groupID         = devObj.value("groupID").toInt(-1);
        QString groupName       = devObj.value("groupName").toString();
        QString uniqueIdInGroup = devObj.value("uniqueIdInGroup").toString();

        QString deviceUniqueId  = devObj.value("deviceUniqueId").toString();

        QJsonArray devUidArray  = devObj.value("deviceUniqueIds").toArray();
        QStringList deviceUniqueIds;
        deviceUniqueIds.reserve(devUidArray.size());
        for (const QJsonValue &v : devUidArray) {
            const QString duid = v.toString();
            if (!duid.isEmpty())
                deviceUniqueIds.append(duid);
        }

        qDebug() << "[functionMonitor] EditGroupDevices ="
                 << "action=" << action
                 << "groupID=" << groupID
                 << "deviceUniqueId=" << deviceUniqueId
                 << "uid=" << uniqueIdInGroup
                 << "deviceUniqueIds=" << deviceUniqueIds;

        if (action == "add") {
            if (deviceUniqueId.isEmpty()) {
                qWarning() << "[functionMonitor] EditGroupDevices(add): deviceUniqueId is empty, skip";
            } else {
                db->insertDevicesinGroup(groupID,
                                         groupName,
                                         deviceUniqueId,
                                         uniqueIdInGroup);
            }
        } else if (action == "remove") {
            if (deviceUniqueId.isEmpty()) {
                qWarning() << "[functionMonitor] EditGroupDevices(remove): deviceUniqueId is empty, skip";
            } else {
                db->removeDeviceFromGroup(groupID,
                                          deviceUniqueId,
                                          uniqueIdInGroup);
            }
        }
    }
    else if (title == "editName") {
        int id = devObj.value("id").toInt();
        QString GroupsName = devObj.value("GroupsName").toString();
        QString uniqueIdInGroup = devObj.value("uniqueIdInGroup").toString();
        QTimer::singleShot(0, db, [db = db, uniqueIdInGroup,GroupsName]() {
            db->editGroupName(uniqueIdInGroup, GroupsName);
        });
        // db->editGroupName(id, GroupsName);
    }
    else if (title == "AddDevice") {
        QString name = devObj.value("Name").toString();
        QString ip   = devObj.value("ip").toString();
        QString deviceUid = devObj.value("deviceUniqueId").toString();

        qDebug() << "[functionMonitor] AddDevice =>"
                 << "name:" << name
                 << "ip:" << ip
                 << "deviceUniqueId:" << deviceUid;

        QTimer::singleShot(0, db, [db = db, name, ip, deviceUid]() {
            db->addNewDevice(name, ip, deviceUid);
        });
    }
    else if (title == "UpdateDevice") {
        QString name  = devObj.value("Name").toString();
        QString ip    = devObj.value("ip").toString();

        QString oldUid = devObj.value("oldDeviceUniqueId").toString();
        QString newUid = devObj.value("deviceUniqueId").toString();

        if (oldUid.isEmpty())
            oldUid = newUid;

        qDebug() << "[functionMonitor] UpdateDevice =>"
                 << "oldUid:" << oldUid
                 << "newUid:" << newUid
                 << "Name:"   << name
                 << "ip:"     << ip;

        QTimer::singleShot(0, db, [db = db, oldUid, newUid, name, ip]() {
            db->updateDeviceByUniqueId(oldUid, newUid, name, ip);
        });
    }
    else if (title == "DeleteDevice") {
        int id        = devObj.value("id").toInt();
        QString name  = devObj.value("Name").toString();
        QString ip    = devObj.value("ip").toString();
        QString deviceUniqueId = devObj.value("deviceUniqueId").toString();
        qDebug() << "[functionMonitor] DeleteDevice => id:" << id << "Name:" << name << "ip:" << ip << "deviceUniqueId" << deviceUniqueId;
        QTimer::singleShot(0, db, [db = db,deviceUniqueId]() {
            db->deleteDeviceByUniqueId(deviceUniqueId);;
        });
        // db->deleteDeviceID(id, name, ip);
    }
    else if (title == "settingbyGroup") {
        int gid = devObj.value("id").toInt();
        QString GroupsName = devObj.value("GroupsName").toString();
        QString uniqueIdInGroup  = devObj.value("uniqueIdInGroup").toString();

        emit openPopupSettingRequested("Group Management");
        QTimer::singleShot(0, db, [db = db, uniqueIdInGroup]() {
            db->getGroupByUid(uniqueIdInGroup);
        });
        // db->getGroupById(gid);
    }
    else if (title == "SelectGroup")
    {
        int id       = devObj.value("id").toInt();
        QString Groupsname  = devObj.value("GroupsName").toString();
        QString uniqueIdInGroup = devObj.value("uniqueIdInGroup").toString();
        uniqueIdInGroupSelected  = uniqueIdInGroup;
        QTimer::singleShot(0, db, [db = db, uniqueIdInGroup]() {
            db->getDevicesInGroupJson(uniqueIdInGroup);
        });
        qDebug() << "[functionMonitor] SelectGroup => id:" << id << "Groupsname:" << Groupsname;
    }
    else {
        qWarning() << "Unknown command title:" << title;
    }
}

void iScreenDF::groupSettingconfig(const QString &title, int id , const QString &name, const QString &devicelist)
{
    qDebug() << "[functionMonitor] GroupSetting :" << title << "ID :" << id << "namegroup :" << name << "device :" << devicelist;
}

// SEND TO QML /sidepanels/SideGroup.qml
void iScreenDF::remoteGroupsJson(const QString &json){
    qDebug() << "[functionMonitor] NavBarGroup:" << json;
    emit setremoteGroupsJson(json);
    emit setSelectedGroupByUniqueId(GroupSelected);
    // GroupSelected = "";
}

void iScreenDF::remoteSideRemoteJson(const QString &json){
    qDebug() << "[functionMonitor]  SideRemote:" << json;
    emit setremoteDeviceListJson(json);
}

void iScreenDF::sigGroupsInGroupSetting(const QString &json) {
       qDebug() << "[functionMonitor]  sigGroupsInGroupSetting:" << json;
    emit setsigGroupsInGroupSetting(json);
}

void iScreenDF::cancelScan()
{
    m_scanning.storeRelease(false);
}

void iScreenDF::scanDevices()
{
    if (m_network2List.isEmpty()) {
        qWarning() << "[functionMonitor] m_network2List is empty";
        return;
    }
    QString ip = m_network2List.at(1)->ip_address;
    QStringList parts = ip.split('.');

    if (parts.size() != 4) {
        qWarning() << "[functionMonitor] invalid ip:" << ip;
        return;
    }
    QString baseIp = parts[0] + "." + parts[1] + "." + parts[2] + ".";

    QThread *thread = new QThread(this);
    WorkerScan *worker = new WorkerScan();
    worker->baseIp    = baseIp;
    worker->selfIp    = ip;
    worker->start     = 1;
    worker->end       = 254;
    worker->port      = 9000;
    worker->timeoutMs = 200;
    qDebug() << "[functionMonitor]  scanDevices:"  << baseIp;

    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &WorkerScan::process);

    connect(worker, &WorkerScan::deviceFound,this,&iScreenDF::ondeviceFound,Qt::QueuedConnection);
    connect(worker, &WorkerScan::scanFinished,
            this, [=]() {
                emit scanFinished();
                thread->quit();
            });
    connect(this, &iScreenDF::scanFinished,
            this, &iScreenDF::onScanFinishedBroadcast);
    connect(thread, &QThread::finished,worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,thread, &QObject::deleteLater);

    thread->start();
}

void iScreenDF::onScanFinishedBroadcast()
{
    if (!chatServerDF) {
        qWarning() << "[functionMonitor] chatServerDF is null, cannot broadcast scanDevicesFinished";
        return;
    }

    QJsonObject Param;
    Param.insert("menuID", "scanDevicesFinished");

    QJsonDocument jsonDoc(Param);
    QString raw_data = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    chatServerDF->broadcastMessage(raw_data);
}

void iScreenDF::ondeviceFound(QString name, QString serial, QString ip, int ping)
{
    qDebug() << "[functionMonitor] deviceFound:"
             << name << serial << ip << ping;

    // ถ้ายังอยากให้ signal ภายใน Qt ใช้งานต่อ ก็ emit ไว้เหมือนเดิม
    emit deviceFound(name, serial, ip, ping);

    // สร้าง JSON ส่งออก WebSocket
    if (!chatServerDF) {
        qWarning() << "[functionMonitor] chatServerDF is null, cannot broadcast scanDevicesFound";
        return;
    }

    QJsonObject Param;
    Param.insert("menuID", "scanDevicesFound");   // ให้ตรงกับ dfdevice.js
    Param.insert("name",   name);
    Param.insert("serial", serial);
    Param.insert("ip",     ip);
    Param.insert("ping",   ping);

    QJsonDocument jsonDoc(Param);

    // ให้ได้ QString แบบ compact JSON
    QString raw_data = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    // ส่งออกไปทุก client
    chatServerDF->broadcastMessage(raw_data);
}


int iScreenDF::NetworkIDCheck(int id)
{
    for (int i = 0; i < m_network2List.size(); ++i) {
        if (m_network2List.at(i)->id == id)
            return i;
    }
    return m_network2List.size();
}

void iScreenDF::NetworkAppen(int id, const QString &dhcp, const QString &ip,const QString &subnet,const QString &gateway,
                             const QString &primaryDns,const QString &secondaryDns,const QString &phyName,const QString &krakenserver)
{
    qDebug() << "[functionMonitor] NetWork :" << id << dhcp << ip << subnet << gateway
             << primaryDns << secondaryDns << phyName << krakenserver;

    int i = NetworkIDCheck(id);

    if (i == m_network2List.size())
        m_network2List.append(new Network2);
    else if (i > m_network2List.size())
        return;

    m_network2List.at(i)->id           = id;
    m_network2List.at(i)->dhcpmethod   = dhcp;
    m_network2List.at(i)->ip_address   = ip;
    m_network2List.at(i)->subnet       = subnet;
    m_network2List.at(i)->ip_gateway   = gateway;
    m_network2List.at(i)->pridns       = primaryDns;
    m_network2List.at(i)->secdns       = secondaryDns;
    m_network2List.at(i)->phyName      = phyName;
    m_network2List.at(i)->krakenserver = krakenserver;
}

void iScreenDF::getNetworkfromDb(int id)
{
    // updateIPServerDF();
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }
    Parameter *p = m_parameter.first();
    emit updateGlobalOffsets( p->m_offset_value, p->m_compass_offset);

    QJsonArray allRows;

    for (int i = 0; i < m_network2List.size(); ++i) {
        Network2 *n = m_network2List.at(i);

        QJsonObject obj;
        obj["id"]            = n->id;
        obj["DHCP"]          = n->dhcpmethod;
        obj["IP_ADDRESS"]    = n->ip_address;
        obj["SUBNETMASK"]    = n->subnet;
        obj["GATEWAY"]       = n->ip_gateway;
        obj["PRIMARY_DNS"]   = n->pridns;
        obj["SECONDARY_DNS"] = n->secdns;
        obj["phyName"]       = n->phyName;
        obj["krakenserver"]  = n->krakenserver;

        allRows.append(obj);
    }

    QJsonObject root;
    root["rows"] = allRows;

    QJsonDocument doc(root);
    const QString allJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    for (int i = 0; i < m_network2List.size(); ++i) {
        Network2 *n = m_network2List.at(i);
        if (n->id != id)
            continue;

        QVariantMap row;
        row["id"]            = n->id;
        row["DHCP"]          = n->dhcpmethod;
        row["IP_ADDRESS"]    = n->ip_address;
        row["SUBNETMASK"]    = n->subnet;
        row["GATEWAY"]       = n->ip_gateway;
        row["PRIMARY_DNS"]   = n->pridns;
        row["SECONDARY_DNS"] = n->secdns;
        row["phyName"]       = n->phyName;
        row["krakenserver"]  = n->krakenserver;
        row["all"] = allJson;

        emit networkRowUpdated(row);
        break;
    }
}

void iScreenDF::updateNetworkfromDisplayIndex(int index,const QString &dhcp,const QString &ip,const QString &mask,const QString &gw,const QString &dns1,const QString &dns2)
{
    // qDebug() << "updateNetworkfromDisplayIndex:" << index << dhcp << ip << mask << gw << dns1 << dns2;
    QTimer::singleShot(0, db, [db = db, index , dhcp ,ip,mask,gw,dns1,dns2]() {
        db->updateNetworkfromDisplay(index,dhcp,ip,mask,gw,dns1,dns2);
    });
    // db->updateNetworkfromDisplay(index,dhcp,ip,mask,gw,dns1,dns2);
}

void iScreenDF::restartNetworkIndex(int index)
{
    int id = index + 1;
    qDebug() << "[functionMonitor] restartNetworkIndex index=" << index << "id=" << id;
}

void iScreenDF::getRecorderSettings(){
    qDebug() << "[functionMonitor] getRecorderSettings";
    db->getRecorderSettings();
}
void iScreenDF::setRecorderSettings(const QString &alsaDevice,const QString &clientIp,int freq,const QString &rtspServer,const QString &rtspUrl,int rtspPort)
{
    db->setRecorderSettingsDB(alsaDevice,clientIp,freq,rtspServer,rtspUrl,rtspPort);
}
void iScreenDF::recorderSettingsReady(QString alsaDevice,QString clientIp,int frequency,QString rtspServer,QString rtspUrl,int rtspPort)
{
    qDebug() << "[functionMonitor] recorderSettingsReady" << alsaDevice << clientIp << frequency << rtspServer  << rtspUrl << rtspPort;
    emit recorderSettings(alsaDevice,clientIp,frequency,rtspServer,rtspUrl,rtspPort);
}


void iScreenDF::setMode(const QString &mode)
{
    qDebug() << "[functionMonitor::setMode] mode =" << mode;

    if (!db) {
        qWarning() << "[functionMonitor::setMode] db is nullptr!";
    } else {
        closeAllGroupClients();
        db->UpdateMode(mode);
    }

    RemoteStatus = mode;
    emit updateParameterMode(mode);
    qDebug() << "[functionMonitor] setMode :" << mode;
    QJsonObject obj;
    obj["menuID"]   = "updateParameterMode";
    obj["mode"]   = mode;
    QJsonDocument doc(obj);
    const QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    chatServerDF->broadcastMessage(jsonStr);

    // ถ้าเปลี่ยนมาเป็น REMOTE และมี connectGroupSingle ค้างอยู่ → ประมวลผลทันที
    if (RemoteStatus == "REMOTE" && m_pendingConnectGroup) {
        qDebug() << "[functionMonitor::setMode] Remote is REMOTE → process pending connectGroupSingle";
        m_pendingConnectGroup = false;
        processConnectGroupSingleInternal(m_pendingConnectObj);
        m_pendingConnectObj = QJsonObject();  // clear
    } else if (RemoteStatus == "LOCAL")
    {
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        Param.insert("objectName", "StopConnecting");
        Param.insert("ip", m_network2List.at(1)->ip_address);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        chatServerDF->broadcastMessage(raw_data);
        qDebug() << "[functionMonitor] broadcastMessage :" << raw_data;
    }
}


void iScreenDF::parameterReceived(const QString &mode,const QString &deviceName,const QString &serial)
{
    qDebug() << "[iScreenDF::parameterReceived] mode =" << mode
             << "deviceName =" << deviceName
             << "serial =" << serial;

    RemoteStatus   = mode;
    controllerName = deviceName;
    Serialnumber   = serial;

    emit updateParameterMode(mode);
    emit updateParameter(deviceName, serial);

    QJsonObject obj;
    obj["menuID"]   = "updateParameterMode";
    obj["mode"]   = mode;
    QJsonDocument doc(obj);
    const QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    chatServerDF->broadcastMessage(jsonStr);

    if (RemoteStatus == "REMOTE" && m_pendingConnectGroup) {
        qDebug() << "[iScreenDF::parameterReceived] Remote is REMOTE → process pending connectGroupSingle";
        m_pendingConnectGroup = false;
        processConnectGroupSingleInternal(m_pendingConnectObj);
        m_pendingConnectObj = QJsonObject();
    }

}

void iScreenDF::handleConnectGroupSingle(const QJsonObject &obj)
{
    QString groupUniqueId = obj["groupUniqueId"].toString();
    uniqueIdInGroupSelected = groupUniqueId;
    qDebug() << "[connectGroupSingle] RemoteStatus =" << RemoteStatus;

    if (RemoteStatus == "LOCAL") {
        qDebug() << "[connectGroupSingle] System in LOCAL mode → pending & request popup";
        m_pendingConnectGroup = true;
        m_pendingConnectObj   = obj;   // เก็บทั้ง obj ไว้ใช้ทีหลัง

        emit requestRemotePopup();
        return;
    }
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("objectName", "getstatus");
    Param.insert("Status", RemoteStatus);
    Param.insert("ip", m_network2List.at(1)->ip_address);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    chatServerDF->broadcastMessage(raw_data);

    // QJsonDocument jsonDoc2;
    // QJsonObject Param2;
    // QString raw_data2;
    // Param2.insert("menuID", "updateCurrentRole");
    // Param2.insert("uniqueIdInGroup", uniqueIdInGroupSelected);
    // jsonDoc2.setObject(Param2);
    // raw_data2 = QJsonDocument(Param2).toJson(QJsonDocument::Compact).toStdString().c_str();
    // chatServerDF->broadcastMessage(raw_data2);

    processConnectGroupSingleInternal(obj);
}

// ------------------------------------------------------------------
// processConnectGroupSingleInternal: logic
// ------------------------------------------------------------------
void iScreenDF::processConnectGroupSingleInternal(const QJsonObject &obj)
{
    int groupId           = obj["groupId"].toInt();
    QString groupName     = obj["groupName"].toString();
    QString groupUniqueId = obj["groupUniqueId"].toString();
    QJsonArray devices    = obj["devices"].toArray();

    qDebug() << "groupId           =" << groupId;
    qDebug() << "groupName         =" << groupName;
    qDebug() << "groupUniqueId     =" << groupUniqueId;
    qDebug() << "----------------------------------------";
    qDebug() << "devices in same group =" << devices.size();

    QString localIp;
    if (!m_network2List.isEmpty() && m_network2List.at(1)) {
        localIp = m_network2List.at(1)->ip_address;
    }
    qDebug() << "[connectGroupSingle] localIp =" << localIp;

    bool hasOtherDevice = false;

    // --- วนลูปดู devices ทั้งกลุ่ม ---
    for (int i = 0; i < devices.size(); ++i) {
        QJsonObject dv   = devices.at(i).toObject();
        int deviceId     = dv.value("deviceId").toInt();
        int deviceGroupId= dv.value("deviceGroupId").toInt();
        int gIdInDevice  = dv.value("groupId").toInt();
        QString groupsName = dv.value("groupsName").toString();
        QString uidGroup   = dv.value("uniqueIdInGroup").toString();
        QString name       = dv.value("name").toString();
        QString ip         = dv.value("ip").toString();
        int port           = dv.value("port").toInt();
        QString deviceUid  = dv.value("deviceUniqueId").toString();
        bool isController  = dv.value("isController").toBool();

        qDebug() << "   [" << i << "]"
                 << "deviceId ="        << deviceId
                 << "deviceGroupId ="   << deviceGroupId
                 << "groupId ="         << gIdInDevice
                 << "groupsName ="      << groupsName
                 << "uniqueIdInGroup =" << uidGroup
                 << "deviceUniqueId ="  << deviceUid
                 << "isController ="    << isController
                 << "name ="            << name
                 << "ip ="              << ip
                 << "port ="            << port;

        // เก็บชื่อ/serial ของตัวเรา (เครื่อง local)
        if (!localIp.isEmpty() && ip == localIp) {
            controllerName = name;
            Serialnumber   = deviceUid;
        }

        // ถ้ามี device อื่นใน group (ip ไม่ตรง local) ให้ mark ไว้
        if (!localIp.isEmpty() && ip != localIp) {
            hasOtherDevice = true;
        }
    }

    qDebug() << "[connectGroupSingle] controllerName =" << controllerName
             << "Serialnumber =" << Serialnumber;
    qDebug() << "========================================";

    GroupSelected = groupUniqueId;

    if (hasOtherDevice) {
        qDebug() << "[connectGroupSingle] has other device in group, save to DB";

        if (!db) {
            qWarning() << "[connectGroupSingle] db is nullptr, cannot save!";
        } else {
            // ใช้ QTimer::singleShot เพื่อให้ทำใน main thread ถ้า db เป็น QObject
            QTimer::singleShot(0, db, [db = db, obj, localIp]() {
                db->saveDevicesAndGroupsFromConnectGroupSingle(obj, localIp);
            });
        }
    } else {
        qDebug() << "[connectGroupSingle] only local device in group, skip DB save";
    }

    // ถ้าคุณต้องการ setup connection จริง ๆ ต่อจากนี้
    // setupServerClientForDevices(groupId, groupName, devices);
}

void iScreenDF::setParameterdevice(const QString &deviceName, const QString &serial)
{
    qDebug()  << "[functionMonitor] setParameterdevice" << deviceName << serial;
    controllerName = deviceName;
    Serialnumber = serial;

    QTimer::singleShot(0, db, [db = db, deviceName, serial]() {
        db->UpdateDeviceParameter(deviceName,serial);
    });
}
