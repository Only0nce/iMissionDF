#include "iScreenDF.h"

int iScreenDF::ClientIndexCheck(const QString &deviceUniqueId)
{
    for (int i = 0; i < client_list.size(); ++i) {
        clientNode *client = client_list.at(i);
        if (client && client->deviceUniqueId == deviceUniqueId) {
            return i;
        }
    }
    return client_list.size();
}

int iScreenDF::ClientActiveIDCheck(const QString &uniqueIdInGroup, groupActive *group)
{
    int i = 0;
    Q_FOREACH (clientNode *client, group->client_active_list) {
        if (client && client->uniqueIdInGroup == uniqueIdInGroup) {
            return i;
        }
        i++;
    }
    return group->client_active_list.size();
}

int iScreenDF::GroupIndexCheck(const QString &uniqueIdInGroup)
{
    int i = 0;
    Q_FOREACH (groupActive *group, group_active_list) {
        if (group && group->uniqueIdInGroup == uniqueIdInGroup) {
            return i;
        }
        i++;
    }
    return group_active_list.size();
}


// void iScreenDF::appendNewClient(int id,QString name, QString ipAddress, uint16_t socketPort)
// {
//     // qDebug() << "[iScreenDF::appendNewClient] id =" ;
//     int i = ClientIndexCheck(id);

//     qDebug() << "[iScreenDF::appendNewClient] id =" << id << "name =" << name << "ip =" << ipAddress << "port =" << socketPort << "index =" << i << "current size =" << client_list.size();

//     if (i > client_list.size()) {
//         qWarning() << "[iScreenDF::appendNewClient] index out of range, ignore. i ="
//                    << i << " size =" << client_list.size();
//         return;
//     }
//     if (i == client_list.size()) {
//         clientNode *node = new clientNode;
//         client_list.append(node);
//     }

//     clientNode *node = client_list.at(i);
//     if (!node) {
//         node = new clientNode;
//         client_list[i] = node;
//     }

//     node->deviceIndex   = id;
//     node->devicename       = name;
//     node->ipAddress  = ipAddress;
//     node->socketPort = socketPort;
//     node->chatclient = nullptr;
// }
quint32 iScreenDF::ipToHex(const QString &ip) const
{
    const QStringList parts = ip.split('.');
    if (parts.size() != 4)
        return 0;

    bool ok = false;
    quint32 a = parts[0].toUInt(&ok); if (!ok) return 0;
    quint32 b = parts[1].toUInt(&ok); if (!ok) return 0;
    quint32 c = parts[2].toUInt(&ok); if (!ok) return 0;
    quint32 d = parts[3].toUInt(&ok); if (!ok) return 0;

    return (a << 24) | (b << 16) | (c << 8) | d;
}

void iScreenDF::appendNewActiveClient(const QString &deviceUniqueId,const QString &uniqueIdInGroup,int deviceID,int groupID,const QString &groupName,const QString &deviceName,const QString &deviceIPAddress,uint16_t socketPort)
{
    qDebug() << "[iScreenDF::appendNewActiveClient]"
             << "deviceUniqueId:" << deviceUniqueId
             << "uniqueIdInGroup:" << uniqueIdInGroup
             << "deviceID:" << deviceID
             << "groupID:" << groupID
             << "GroupName:" << groupName
             << "DeviceName:" << deviceName
             << "IP:" << deviceIPAddress
             << "Port:" << socketPort;
    // closeAllGroupClients();
    groupActive *currentGroup = nullptr;
    for (groupActive *g : group_active_list) {
        if (g && g->uniqueIdInGroup == uniqueIdInGroup) {
            currentGroup = g;
            break;
        }
    }

    if (!currentGroup)  {
        currentGroup = new groupActive;
        currentGroup->groupID        = groupID;
        currentGroup->groupName      = groupName;
        currentGroup->uniqueIdInGroup = uniqueIdInGroup;
        group_active_list.append(currentGroup);
        qDebug() << "[appendNewActiveClient] New groupActive created for uniqueIdInGroup"
                 << uniqueIdInGroup;
    } else {
        currentGroup->groupID   = groupID;
        currentGroup->groupName = groupName;
    }

    clientNode *node = nullptr;
    for (clientNode *c : currentGroup->client_active_list) {
        if (c && c->deviceUniqueId == deviceUniqueId) {
            node = c;
            break;
        }
    }
    if (!node) {
        node = new clientNode;
        node->chatclient = nullptr;
        currentGroup->client_active_list.append(node);
        qDebug() << "[appendNewActiveClient] New clientNode created for deviceUniqueId"
                 << deviceUniqueId;
    }

    node->deviceUniqueId = deviceUniqueId;
    node->uniqueIdInGroup = uniqueIdInGroup;
    node->deviceID       = deviceID;
    node->groupID        = groupID;
    node->devicename     = deviceName;
    node->ipAddress      = deviceIPAddress;
    node->socketPort     = socketPort;

    QString selfIP;
    if (!m_network2List.isEmpty() && m_network2List.at(1)) {
        selfIP = m_network2List.at(1)->ip_address;
    }

    quint32 selfIPValue   = ipToHex(selfIP);
    quint32 clientIPValue = ipToHex(node->ipAddress);

    if (clientIPValue >= selfIPValue && clientIPValue > 0) {
        if (!node->chatclient) {
            node->chatclient = new ChatClientDF(this);
            node->chatclient->setUniqueIdInGroup(uniqueIdInGroup);
            connect(node->chatclient, SIGNAL(TextMessageReceived(QString)),
                    this, SLOT(TextMessageReceivedFromClient(QString)));
            connect(node->chatclient, SIGNAL(closed(int,QString)),
                    this, SLOT(socketClientClosed(int,QString)));
            connect(node->chatclient, &ChatClientDF::onDeviceConnected,
                    this, &iScreenDF::onDeviceConnected);

            node->chatclient->createConnection(node->ipAddress, node->socketPort);

            qDebug() << "[appendNewActiveClient] ChatClient created for"
                     << node->ipAddress << ":" << node->socketPort;
        } else {
            qDebug() << "[appendNewActiveClient] Reusing existing ChatClient for deviceUniqueId"
                     << deviceUniqueId;
        }
    } else {
        qDebug() << "[appendNewActiveClient] Skipping connect, client IP lower than self IP"
                 << node->ipAddress << "selfIP:" << selfIP;
    }
}


void iScreenDF::onDeviceConnected(const QString &uniqueIdInGroup, const QString &ipaddress)
{
    qDebug() << "[iScreenDF::onDeviceConnected]"
             << "uniqueIdInGroup:" << uniqueIdInGroup
             << "ipaddress:" << ipaddress;

    Q_FOREACH (groupActive *group, group_active_list)
    {
        if (!group)
            continue;

        Q_FOREACH (clientNode *client, group->client_active_list)
        {
            if (!client || !client->chatclient)
                continue;

            bool currentConnected = client->chatclient->isConnected;
            if (currentConnected &&
                client->uniqueIdInGroup == uniqueIdInGroup &&
                client->ipAddress == ipaddress)
            {
                qDebug() << "onDeviceConnected"
                         << client->devicename
                         << "isConnected:" << currentConnected;

                QJsonObject DeviceConnected;
                DeviceConnected.insert("menuID", "DeviceConnected");
                DeviceConnected.insert("uniqueIdInGroup", uniqueIdInGroup);
                DeviceConnected.insert("groupID", group->groupID);

                chatServerDF->broadcastMessage(
                    QJsonDocument(DeviceConnected).toJson(QJsonDocument::Compact));

                client->descriptions = "DeviceConnected";
                client->status = 1;

                // ส่งคำสั่ง getName
                QJsonObject recordObject;
                recordObject.insert("menuID", "getName");
                client->chatclient->m_webSocket->sendTextMessage(
                    QJsonDocument(recordObject).toJson(QJsonDocument::Compact));
            }

            if (client->Connected != currentConnected) {
                client->Connected = currentConnected;
                qDebug() << "Success in savedata"
                         << client->uniqueIdInGroup
                         << client->descriptions
                         << client->status;
            }
        }
    }
}


void iScreenDF::TextMessageReceivedFromClient(const QString &message)
{
    // qDebug() << "[iScreenDF::TextMessageReceivedFromClient]" << message;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[TextMessageReceivedFromClient] JSON parse error:"
                   << err.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString menuID  = obj.value("menuID").toString();
    QString objectName = obj.value("objectName").toString();
    QString broadcastID = obj.value("broadcastID").toString();

    if (!broadcastID.isEmpty()) {
        handleBroadcastMessage(obj);
    }

    if (objectName == "StopConnecting") {
        qDebug() << "[iScreenDF::TextMessageReceivedFromClient]" << message;
        QString ip = obj["ip"].toString();
        closeGroupClientsByIp(ip);
        GroupSelected = "";
        emit setSelectedGroupByUniqueId(GroupSelected);
    }
    // qDebug() << "[iScreenDF::TextMessageReceivedFromClient]" << message;
}

static QString normalizeIp(const QString &s)
{
    QString t = s.trimmed();
    if (t.isEmpty())
        return t;

    // ws://ip:port หรือ http://ip:port
    QUrl url(t);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (!url.host().isEmpty())
            return url.host().trimmed();
    }

    // ip:port (IPv4 case)
    int colon = t.indexOf(':');
    if (colon > 0) {
        QString maybeIp = t.left(colon).trimmed();
        if (!maybeIp.isEmpty())
            return maybeIp;
    }

    return t;
}

void iScreenDF::closeGroupClientsByIp(const QString &targetIp)
{
    const QString ipNeed = normalizeIp(targetIp);

    qDebug() << "[iScreenDF::closeGroupClientsByIp] closing ALL clients with ip =" << ipNeed;

    if (ipNeed.isEmpty()) {
        qWarning() << "[iScreenDF::closeGroupClientsByIp] targetIp is empty, skip";
        return;
    }

    int killed = 0;

    for (groupActive *group : group_active_list) {
        if (!group)
            continue;

        for (int i = group->client_active_list.size() - 1; i >= 0; --i) {
            clientNode *node = group->client_active_list.at(i);
            if (!node)
                continue;

            ChatClientDF *cl = node->chatclient;
            if (!cl)
                continue;

            // --- ดึง IP จากหลาย field แล้ว normalize ---
            QString raw;
            if (!cl->ip_address.isEmpty())
                raw = cl->ip_address;
            else if (!cl->m_ipaddress.isEmpty())
                raw = cl->m_ipaddress;

            const QString clientIp = normalizeIp(raw);

            qDebug() << "   [closeGroupClientsByIp] candidate raw =" << raw
                     << "-> normalized =" << clientIp;

            if (clientIp != ipNeed)
                continue;

            qDebug() << "[iScreenDF::closeGroupClientsByIp] DISCONNECT client ip =" << clientIp
                     << "socketID =" << cl->m_socketID;

            // 1) กัน callback ยิงกลับมาหลังจากเราลบ node/ลบ client
            cl->disconnect();

            // 2) ปิดการเชื่อมต่อ (สุภาพ)
            cl->disconnectFromServer();

            // 3) ทำลาย object
            cl->deleteLater();
            node->chatclient = nullptr;

            // 4) ลบ node ออกจาก list
            group->client_active_list.removeAt(i);
            delete node;

            killed++;
        }
    }

    // เร่งให้ deleteLater ถูก process ในรอบนี้ (ช่วยกัน reconnect ซ้ำเร็วๆ)
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    qDebug() << "[iScreenDF::closeGroupClientsByIp] done. killed =" << killed
             << "clients for ip =" << ipNeed;
}




void iScreenDF::closeAllGroupClients()
{
    qDebug() << "[iScreenDF::closeAllGroupClients] closing all clients";

    for (groupActive* group : group_active_list) {
        if (!group) continue;

        for (clientNode* client : group->client_active_list) {
            if (!client) continue;

            if (client->chatclient) {
                // WebSocket
                client->chatclient->disconnectFromServer();

                // signal-slot connections
                client->chatclient->disconnect();

                // object
                client->chatclient->deleteLater();
                client->chatclient = nullptr;
            }
        }
        qDeleteAll(group->client_active_list);
        group->client_active_list.clear();
    }

    // เคลียร์ list ของ group ถ้าต้องการลบทั้งหมด
    // qDeleteAll(group_active_list);
    // group_active_list.clear();
}

void iScreenDF::DevicesInGroupJsonReady(int groupId,
                                          const QString &groupName,
                                          const QString &groupUniqueId,
                                          const QJsonArray &devices)
{
    setMode("REMOTE");

    qDebug() << "[iScreenDF] DevicesInGroupJsonReady:"
             << "groupId =" << groupId
             << "groupName =" << groupName
             << "groupUniqueId =" << groupUniqueId
             << "devices =" << devices;

    if (devices.isEmpty()) {
        qWarning() << "[iScreenDF] DevicesInGroupJsonReady: devices array is empty";
        return;
    }

    int controllerNetId = -1;
    QString controllerIp;

    if (!m_network2List.isEmpty() && m_network2List.at(1)) {
        controllerNetId = m_network2List.at(1)->id;
        controllerIp    = m_network2List.at(1)->ip_address;
    }

    // ---------------------------------------------------------
    //  Loop ทุก Device และ Connect ใหม่ทันที โดยไม่เช็คของเก่า
    // ---------------------------------------------------------
    for (const QJsonValue &v : devices) {
        QJsonObject devObj = v.toObject();

        QString ip   = devObj.value("ip").toString();
        int port     = devObj.value("port").toInt();
        QString name = devObj.value("name").toString();

        QString uniqueIdInGroup = devObj.value("uniqueIdInGroup").toString();
        if (uniqueIdInGroup.isEmpty())
            uniqueIdInGroup = groupUniqueId;

        QString deviceUid = devObj.value("deviceUniqueId").toString();
        if (deviceUid.isEmpty()) {
            deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            devObj["deviceUniqueId"] = deviceUid;
        }

        if (ip.isEmpty() || port <= 0) {
            qWarning() << "[iScreenDF] skip device, invalid ip/port";
            continue;
        }

        // -------------------- อัด devices ใหม่ --------------------
        QJsonArray filteredDevices;
        for (const QJsonValue &dv : devices) {
            QJsonObject other = dv.toObject();

            if (other.value("deviceUniqueId").toString().isEmpty())
                other["deviceUniqueId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);

            filteredDevices.append(other);
        }

        // -------------------- เพิ่ม controller --------------------
        if (controllerNetId != -1 && !controllerIp.isEmpty()) {
            QJsonObject controllerObj;
            controllerObj["deviceGroupId"]   = 0;
            controllerObj["groupId"]         = groupId;
            controllerObj["groupsName"]      = groupName;
            controllerObj["ip"]              = controllerIp;
            controllerObj["name"]            = controllerName;
            controllerObj["port"]            = 8000;
            controllerObj["isController"]    = true;
            controllerObj["uniqueIdInGroup"] = uniqueIdInGroup;
            controllerObj["deviceUniqueId"]  = Serialnumber;
            filteredDevices.append(controllerObj);
        }

        // -------------------- JSON Payload --------------------
        QJsonObject single;
        single["menuID"]          = "connectGroupSingle";
        single["groupId"]         = groupId;
        single["groupName"]       = groupName;
        single["groupUniqueId"]   = groupUniqueId;
        single["uniqueIdInGroup"] = uniqueIdInGroup;
        single["devices"]         = filteredDevices;

        QString sendJson =
            QString::fromUtf8(QJsonDocument(single).toJson(QJsonDocument::Compact));

        qDebug() << "[iScreenDF] FORCE CONNECT ->" << ip << "send:" << sendJson;

        // ============================================================
        //  ไม่เช็ค Node เดิมแล้ว! สร้าง ChatClient ใหม่ทุกรอบ
        // ============================================================
        ChatClientDF *client = new ChatClientDF(this);
        client->setUniqueIdInGroup(uniqueIdInGroup);

        connect(client, &ChatClientDF::onDeviceConnected,
                this,
                [client, sendJson](QString, QString ipAddr){
                    qDebug() << "[DevicesInGroupJsonReady] Connected -> sending to" << ipAddr;
                    client->sendTextMessage(sendJson);
                    client->disconnectFromServer();
                });

        connect(client, &ChatClientDF::closed,
                client, &QObject::deleteLater);

        client->createConnection(ip, static_cast<quint16>(port));
    }
    // db->getActiveClientInDatabase(groupUniqueId);
    setupServerClientForDevicesRemote(groupUniqueId);
}

void iScreenDF::setupServerClientForDevices(const QString &groupUniqueId){
    qDebug() << "[iScreenDF] setupServerClientForDevices :" << groupUniqueId  ;
    // QJsonDocument jsonDoc;
    // QJsonObject Param;
    // QString raw_data;
    // Param.insert("objectName", "StopConnecting");
    // Param.insert("ip", m_network2List.at(0)->ip_address);
    // jsonDoc.setObject(Param);
    // raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    // chatServerDF->broadcastMessage(raw_data);
    // qDebug() << "[iScreenDF] broadcastMessage :" << raw_data  ;

    closeAllGroupClients();

    QTimer::singleShot(0, db, [db = db, groupUniqueId]() {
            db->getActiveClientInDatabase(groupUniqueId);
    });
}
void iScreenDF::setupServerClientForDevicesRemote(const QString &uniqueIdInGroup){
    qDebug() << "[iScreenDF] setupServerClientForDevices :" << uniqueIdInGroup  ;

    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("objectName", "StopConnecting");
    Param.insert("ip", m_network2List.at(1)->ip_address);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    chatServerDF->broadcastMessage(raw_data);
    qDebug() << "[iScreenDF] broadcastMessage :" << raw_data  ;

    closeAllGroupClients();
    QTimer::singleShot(0, db, [db = db, uniqueIdInGroup]() {
        db->getActiveClientInDatabase(uniqueIdInGroup);
    });
    // db->getActiveClientInDatabase(uniqueIdInGroup);
}

void iScreenDF::handleBroadcastMessage(const QJsonObject &obj)
{
    Parameter *p = new Parameter();
    const QString broadcastID = obj.value("broadcastID").toString();

    if (broadcastID == "UpdateGPSMarker") {
        const QString serial = obj.value("Serialnumber").toString();
        const QString name   = obj.value("name").toString();

        const double lat = obj.value("lat").toDouble();
        const double lon = obj.value("lon").toDouble();
        const double alt = obj.value("alt").toDouble();

        const QString dateStr = obj.value("date").toString();
        const QString timeStr = obj.value("time").toString();

        emit updateGpsMarker(serial, name, lat, lon, alt, dateStr, timeStr);
        return;
    }
    if (broadcastID == "doaFrameUpdated") {
        // ===== identity ต้องให้ตรงกับ QML key: serial|name =====
        const QString serial = obj.value("Serialnumber").toString();

        // เอา name เป็นหลัก (ให้ตรงกับ GPS)
        QString name = obj.value("name").toString();
        if (name.isEmpty())
            name = obj.value("controllerName").toString();

        // arrays
        QVariantList thetaList;
        QVariantList specList;

        const QJsonArray thetaArr = obj.value("thetaList").toArray();
        const QJsonArray specArr  = obj.value("specList").toArray();

        const int n = qMin(thetaArr.size(), specArr.size());
        thetaList.reserve(n);
        specList.reserve(n);

        for (int i = 0; i < n; ++i) {
            thetaList.append(thetaArr.at(i).toDouble());
            specList.append(specArr.at(i).toDouble());
        }

        // scalars
        const double doaDeg = obj.value("doaDeg").toDouble();
        const double conf   = obj.value("confidence").toDouble();

        emit doaFrameUpdated(serial, name, thetaList, specList, doaDeg, conf);
        return;
    }
    if (broadcastID == "Compass")
    {
        const QString serial = obj.value("Serialnumber").toString();

        QString name = obj.value("name").toString();
        if (name.isEmpty())
            name = obj.value("controllerName").toString();
        const double heading = obj.value("heading").toDouble();

        emit updateDegree(serial,name,heading);
        return;
    }

    if (broadcastID == "updateReceiverFreqandbw")
    {
        int  Freq = obj.value("Freq").toDouble();
        int BW   = obj.value("BW").toDouble();
        bool link = obj.value("linkstatus").toBool();
        p->m_Frequency = Freq;
        p->m_doaBwHz = BW;
        p->m_linkStatus = link;
        updateReceiverFreqandbw(Freq,BW,link);

        qDebug() << "[functionWebsocketMng] updateReceiverFreqandbw:" << Freq << BW << link;
        return;
    }

}
void iScreenDF::updateReceiverFreqandbw(int Freq, int BW,bool link)
{
    emit rfsocParameterUpdated(Freq, BW);
    emit updatelinkStatus(link);
    const double offsetHz = BW * 0.51;

    updateReceiverParametersFreqOffsetBw((qint64)Freq, offsetHz, (double)BW);
    // emit updateReceiverFreqandbw
    emit updateReceiverParametersFreqandbw(Freq, BW , link);
}

void iScreenDF::broadcastMessageServerandClient(const QJsonObject &obj)
{
    QJsonObject out = obj;
    if (out.contains("menuID")) {
        out["broadcastID"] = out.value("menuID");
        out.remove("menuID");
    }

    out["name"] = controllerName;
    out["Serialnumber"] = Serialnumber;

    const QString msg = QString::fromUtf8(
        QJsonDocument(out).toJson(QJsonDocument::Compact)
        );

    if (chatServerDF)
        chatServerDF->broadcastMessage(msg);

    for (groupActive *group : group_active_list) {
        if (!group) continue;

        for (clientNode *client : group->client_active_list) {
            if (!client || !client->chatclient || !client->chatclient->m_webSocket)
                continue;

            auto *ws = client->chatclient->m_webSocket;
            if (client->chatclient->isConnected &&
                ws->state() == QAbstractSocket::ConnectedState) {

                ws->sendTextMessage(msg);
            }
        }
    }
}
// void iScreenDF::DevicesInGroupJsonReady(int groupId,const QString &groupName,const QString &groupUniqueId,const QJsonArray &devices)
// {
//     qDebug() << "[iScreenDF] DevicesInGroupJsonReady:"
//              << "groupId =" << groupId
//              << "groupName =" << groupName
//              << "groupUniqueId =" << groupUniqueId
//              << "devices =" << devices;

//     if (devices.isEmpty()) {
//         qWarning() << "[iScreenDF] DevicesInGroupJsonReady: devices array is empty";
//         return;
//     }

//     int controllerNetId = -1;
//     QString controllerIp;

//     if (!m_network2List.isEmpty() && m_network2List.at(0)) {
//         controllerNetId = m_network2List.at(0)->id;
//         controllerIp    = m_network2List.at(0)->ip_address;
//     }

//     for (const QJsonValue &v : devices) {
//         QJsonObject devObj = v.toObject();

//         QString ip   = devObj.value("ip").toString();
//         int port     = devObj.value("port").toInt();
//         QString name = devObj.value("name").toString();

//         QString uniqueIdInGroup = devObj.value("uniqueIdInGroup").toString();
//         if (uniqueIdInGroup.isEmpty())
//             uniqueIdInGroup = groupUniqueId;

//         QString deviceUid = devObj.value("deviceUniqueId").toString();
//         if (deviceUid.isEmpty()) {
//             deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
//             devObj["deviceUniqueId"] = deviceUid;
//         }

//         if (ip.isEmpty() || port <= 0) {
//             qWarning() << "[iScreenDF] skip device, invalid ip/port";
//             continue;
//         }

//         // --------- สร้าง JSON ส่งไปยัง device ---------
//         QJsonArray filteredDevices;
//         for (const QJsonValue &dv : devices) {
//             QJsonObject other = dv.toObject();
//             if (other.value("deviceUniqueId").toString().isEmpty())
//                 other["deviceUniqueId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
//             filteredDevices.append(other);
//         }

//         if (controllerNetId != -1 && !controllerIp.isEmpty()) {
//             QJsonObject controllerObj;
//             controllerObj["deviceGroupId"]   = 0;
//             controllerObj["groupId"]         = groupId;
//             controllerObj["groupsName"]      = groupName;
//             controllerObj["ip"]              = controllerIp;
//             controllerObj["name"]            = controllerName;
//             controllerObj["port"]            = 8000;
//             controllerObj["isController"]    = true;
//             controllerObj["uniqueIdInGroup"] = uniqueIdInGroup;
//             controllerObj["deviceUniqueId"]  = Serialnumber;
//             filteredDevices.append(controllerObj);
//         }

//         QJsonObject single;
//         single["menuID"]          = "connectGroupSingle";
//         single["groupId"]         = groupId;
//         single["groupName"]       = groupName;
//         single["groupUniqueId"]   = groupUniqueId;
//         single["uniqueIdInGroup"] = uniqueIdInGroup;
//         single["devices"]         = filteredDevices;

//         QString sendJson = QString::fromUtf8(QJsonDocument(single).toJson(QJsonDocument::Compact));
//         qDebug() << "[iScreenDF] send to" << ip << "=>" << sendJson;

//         // --------- inline search clientNode ตาม IP ---------
//         clientNode* node = nullptr;
//         for (groupActive* group : group_active_list) {
//             if (!group) continue;
//             for (clientNode* c : group->client_active_list) {
//                 if (!c) continue;
//                 if (c->ipAddress == ip) {
//                     node = c;
//                     break;
//                 }
//             }
//             if (node) break;
//         }

//         if (node && node->chatclient && node->chatclient->isConnected) {
//             node->chatclient->sendTextMessage(sendJson);
//             qDebug() << "[DevicesInGroupJsonReady] Reuse connected ChatClient for" << ip;
//         } else {
//             ChatClient *client = new ChatClient(this);
//             client->setUniqueIdInGroup(uniqueIdInGroup);

//             connect(client, &ChatClient::onDeviceConnected,
//                     this,
//                     [client, sendJson](QString, QString ipAddr){
//                         qDebug() << "[DevicesInGroupJsonReady] onDeviceConnected -> send to" << ipAddr;
//                         client->sendTextMessage(sendJson);
//                         client->disconnectFromServer();
//                     });

//             connect(client, &ChatClient::closed,
//                     this,
//                     [this, client]{
//                         m_groupClients.removeAll(client);
//                         client->deleteLater();
//                     });

//             m_groupClients.append(client);  // เก็บ client ไว้ไม่ให้ถูก delete
//             client->createConnection(ip, static_cast<quint16>(port));
//         }
//     }
//     // db->getActiveClientInDatabase(groupUniqueId);
// }







