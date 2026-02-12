#include "DatabaseDF.h"
#include <QSqlError>

// ===== Constructor / Destructor =====

DatabaseDF::DatabaseDF(const QString &dbName,
                       const QString &user,
                       const QString &password,
                       const QString &host,
                       QObject *parent)
    : QObject(parent),
    m_dbName(dbName),
    m_dbUser(user),
    m_dbPassword(password),
    m_dbHost(host)
{
    // ไม่สร้าง QSqlDatabase ที่นี่ เพื่อให้สร้างใน thread ปัจจุบันผ่าน ensureDb()
    qDebug() << "[Database] ctor, thread =" << QThread::currentThread();
}

DatabaseDF::~DatabaseDF()
{
    if (db.isValid()) {
        db.close();
    }
}

// ===== Connection Helpers =====

bool DatabaseDF::database_createConnection()
{
    return ensureDb();
}

bool DatabaseDF::ensureDb()
{
    if (!db.isValid()) {
        QString connName =
            QStringLiteral("dbconn_%1").arg(reinterpret_cast<quintptr>(this));
        db = QSqlDatabase::addDatabase("QMYSQL", connName);
        db.setHostName(m_dbHost);
        db.setDatabaseName(m_dbName);
        db.setUserName(m_dbUser);
        db.setPassword(m_dbPassword);
    }

    if (db.isOpen())
        return true;

    if (!db.open()) {
        qWarning() << "[Database] open failed:" << db.lastError().text();
        return false;
    }

    qDebug() << "[Database] DB opened in thread" << QThread::currentThread();
    return true;
}

void DatabaseDF::restartMysql()
{
    system("systemctl stop mysqld");
    system("systemctl start mysqld");
    qDebug() << "[Database] Restart MySQL requested";
}

// ===== Slots for thread lifecycle =====

void DatabaseDF::init()
{
    qDebug() << "[Database::init] thread =" << QThread::currentThread();

    if (!ensureDb()) {
        qWarning() << "[Database::init] ensureDb() failed";
        return;
    }
    ensureColumnsInIScreenparameter();
    createServerKrakenNetworkTable();
    getNetwork();
    getNTPServer();
    // getKrakenServer();  // ถ้าไม่ใช้ก็ไม่ต้อง
    getServerKrakenNetwork();
    getNetworkfromDb();
    GetParameter();
    // getAllClientInDatabase();
    // getActiveClientInDatabase();

    GetrfsocParameter();
    GetIPDFServerFromDB();
    ensureParameterHasMaxDoaLineMeters();
    ensureParameterIPLocalForRemoteGroup();


    qDebug() << "[Database::init] initial queries done";
}


void DatabaseDF::shutdown()
{
    qDebug() << "[Database::shutdown]";
    if (db.isValid() && db.isOpen())
        db.close();
}

void DatabaseDF::getKrakenSetting()
{
    qDebug() << "[Database::getKrakenSetting] not implemented yet";
}

void DatabaseDF::getKrakenServer()
{
    qDebug() << "[Database::getKrakenServer] not implemented (ใช้ getNetwork / iScreenparameter แทน)";
}

// ===== iScreen / Network column ensure =====

void DatabaseDF::ensureColumnsInIScreenparameter()
{
    if (!ensureDb()) {
        qWarning() << "[ensureColumnsInIScreenparameter] DB open failed";
        return;
    }

    QSqlQuery query(db);

    QStringList existingColumns;
    if (!query.exec("SHOW COLUMNS FROM Network")) {
        qWarning() << "Failed to fetch columns:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        existingColumns << query.value(0).toString();
    }

    struct ColumnDef {
        QString name;
        QString typeDef;
        QString defaultValue;
    };

    QList<ColumnDef> requiredColumns = {
        { "krakenserver", "VARCHAR(255) NOT NULL", "192.168.10.26" }
    };

    for (const ColumnDef &col : requiredColumns) {
        if (!existingColumns.contains(col.name)) {
            QString alterSql = QString("ALTER TABLE Network ADD COLUMN %1 %2")
            .arg(col.name, col.typeDef);
            if (!query.exec(alterSql)) {
                qWarning() << "Failed to add column:" << col.name
                           << query.lastError().text();
            } else {
                qDebug() << "Added column:" << col.name;

                QString updateSql =
                    QString("UPDATE Network SET %1 = ?").arg(col.name);
                QSqlQuery updateQuery(db);
                updateQuery.prepare(updateSql);
                updateQuery.addBindValue(col.defaultValue);
                if (!updateQuery.exec()) {
                    qWarning() << "Failed to set initial value for"
                               << col.name << ":"
                               << updateQuery.lastError().text();
                } else {
                    qDebug() << "Set default value for column"
                             << col.name << ":" << col.defaultValue;
                }
            }
        } else {
            qDebug() << "Column already exists:" << col.name;
        }
    }
}

// ===== ServerKrakenNetwork table =====

void DatabaseDF::createServerKrakenNetworkTable()
{
    if (!ensureDb()) {
        qWarning() << "[createServerKrakenNetworkTable] DB open failed";
        return;
    }

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SHOW TABLES LIKE 'ServerKrakenNetwork'");
    if (!checkQuery.exec()) {
        qWarning() << "Failed to check for existing table:"
                   << checkQuery.lastError().text();
        return;
    }

    if (checkQuery.next()) {
        qDebug() << "Table ServerKrakenNetwork already exists. Skipping creation.";
        return;
    }

    QSqlQuery createQuery(db);
    QString createSql = R"(
        CREATE TABLE ServerKrakenNetwork (
            id             INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
            DHCP           VARCHAR(255),
            IP_ADDRESS     VARCHAR(255),
            SUBNETMASK     VARCHAR(255),
            GATEWAY        VARCHAR(255),
            PRIMARY_DNS    VARCHAR(255),
            SECONDARY_DNS  VARCHAR(255),
            phyName        VARCHAR(255)
        )
    )";

    if (!createQuery.exec(createSql)) {
        qWarning() << "Failed to create table ServerKrakenNetwork:"
                   << createQuery.lastError().text();
        return;
    }

    qDebug() << "Table ServerKrakenNetwork created successfully with ID column.";

    QSqlQuery insertQuery(db);
    insertQuery.prepare(R"(
        INSERT INTO ServerKrakenNetwork
        (DHCP, IP_ADDRESS, SUBNETMASK, GATEWAY, PRIMARY_DNS, SECONDARY_DNS, phyName)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    insertQuery.addBindValue("off");
    insertQuery.addBindValue("192.168.1.100");
    insertQuery.addBindValue("255.255.255.0");
    insertQuery.addBindValue("192.168.1.1");
    insertQuery.addBindValue("8.8.8.8");
    insertQuery.addBindValue("8.8.4.4");
    insertQuery.addBindValue("eth0");

    if (!insertQuery.exec()) {
        qWarning() << "Failed to insert initial data:"
                   << insertQuery.lastError().text();
    } else {
        qDebug() << "Initial data inserted into ServerKrakenNetwork.";
    }
}


void DatabaseDF::getIScreenParameter()
{
    if (!ensureDb()) {
        qWarning() << "[getIScreenParameter] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT id, krakenserver, iScreenclient, Subnet, Gateway, phyName
        FROM iScreenparameter
        LIMIT 1
    )");

    if (!query.exec()) {
        qWarning() << "Failed to query iScreenparameter:"
                   << query.lastError().text();
        return;
    }

    if (query.next()) {
        int id            = query.value(0).toInt();
        QString krakenserver  = query.value(1).toString();
        QString iScreenclient = query.value(2).toString();
        QString subnet        = query.value(3).toString();
        QString gateway       = query.value(4).toString();
        QString phyName       = query.value(5).toString();

        qDebug() << "iScreenparameter row:";
        qDebug() << "  id:" << id;
        qDebug() << "  krakenserver:" << krakenserver;
        qDebug() << "  iScreenclient:" << iScreenclient;
        qDebug() << "  Subnet:" << subnet;
        qDebug() << "  Gateway:" << gateway;
        qDebug() << "  phyName:" << phyName;

    } else {
        qDebug() << "No rows found in iScreenparameter.";
    }
}

void DatabaseDF::updateIScreenParameterById(int id,
                                            const QString &krakenserver,
                                            const QString &iScreenclient,
                                            const QString &subnet,
                                            const QString &gateway,
                                            const QString &phyName)
{
    if (!ensureDb()) {
        qWarning() << "[updateIScreenParameterById] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE iScreenparameter
        SET krakenserver = ?, iScreenclient = ?, Subnet = ?, Gateway = ?, phyName = ?
        WHERE id = ?
    )");

    query.addBindValue(krakenserver);
    query.addBindValue(iScreenclient);
    query.addBindValue(subnet);
    query.addBindValue(gateway);
    query.addBindValue(phyName);
    query.addBindValue(id);

    if (!query.exec()) {
        qWarning() << "Failed to update iScreenparameter with id" << id << ":"
                   << query.lastError().text();
    } else {
        qDebug() << "iScreenparameter updated successfully for id:" << id;
    }
}

void DatabaseDF::getServerKrakenNetwork()
{
    if (!ensureDb()) {
        qWarning() << "[getServerKrakenNetwork] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT DHCP, IP_ADDRESS, SUBNETMASK, GATEWAY, PRIMARY_DNS, SECONDARY_DNS, phyName
        FROM ServerKrakenNetwork
        LIMIT 1
    )");

    if (!query.exec()) {
        qWarning() << "Failed to query ServerKrakenNetwork:"
                   << query.lastError().text();
        return;
    }

    if (query.next()) {
        QString dhcp     = query.value(0).toString();
        QString ip       = query.value(1).toString();
        QString subnet   = query.value(2).toString();
        QString gateway  = query.value(3).toString();
        QString dns1     = query.value(4).toString();
        QString dns2     = query.value(5).toString();
        QString phyName  = query.value(6).toString();

        qDebug() << "ServerKrakenNetwork (first row):";
        qDebug() << "  DHCP:         " << dhcp;
        qDebug() << "  IP_ADDRESS:   " << ip;
        qDebug() << "  SUBNETMASK:   " << subnet;
        qDebug() << "  GATEWAY:      " << gateway;
        qDebug() << "  PRIMARY_DNS:  " << dns1;
        qDebug() << "  SECONDARY_DNS:" << dns2;
        qDebug() << "  phyName:      " << phyName;

        emit updateNetworkServerKraken(dhcp, ip, subnet, gateway, dns1, dns2, phyName);
    } else {
        qDebug() << "No rows found in ServerKrakenNetwork table.";
    }
}

void DatabaseDF::updateServerKrakenNetwork(const QString &dhcp,
                                           const QString &ip,
                                           const QString &subnet,
                                           const QString &gateway,
                                           const QString &primaryDns,
                                           const QString &secondaryDns,
                                           const QString &phyName)
{
    if (!ensureDb()) {
        qWarning() << "[updateServerKrakenNetwork] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE ServerKrakenNetwork
        SET DHCP = ?, IP_ADDRESS = ?, SUBNETMASK = ?, GATEWAY = ?, PRIMARY_DNS = ?, SECONDARY_DNS = ?
        WHERE phyName = ?
    )");

    query.addBindValue(dhcp);
    query.addBindValue(ip);
    query.addBindValue(subnet);
    query.addBindValue(gateway);
    query.addBindValue(primaryDns);
    query.addBindValue(secondaryDns);
    query.addBindValue(phyName);

    if (!query.exec()) {
        qWarning() << "Failed to update ServerKrakenNetwork:"
                   << query.lastError().text();
    } else {
        qDebug() << "ServerKrakenNetwork updated successfully for phyName:"
                 << phyName;
    }
}

// ===== Network (single row Network table) =====

void DatabaseDF::getNetwork()
{
    qDebug() << "getNetwork from database";

    if (!ensureDb()) {
        qWarning() << "[getNetwork] DB open failed";
        return;
    }

    QSqlQuery query(db);
    QString queryStr =
        "SELECT DHCP, IP_ADDRESS, SUBNETMASK, GATEWAY, PRIMARY_DNS, "
        "       SECONDARY_DNS, krakenserver FROM Network;";

    if (!query.exec(queryStr)) {
        qWarning() << "[getNetwork] Query failed:"
                   << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString dhcp          = query.value(0).toString();
        QString ip            = query.value(1).toString();
        QString subnet        = query.value(2).toString();
        QString gateway       = query.value(3).toString();
        QString primaryDns    = query.value(4).toString();
        QString secondaryDns  = query.value(5).toString();
        QString krakenserver  = query.value(6).toString();

        qDebug() << "DHCP:" << dhcp
                 << "IP:" << ip
                 << "Subnet:" << subnet
                 << "Gateway:" << gateway
                 << "Primary DNS:" << primaryDns
                 << "Secondary DNS:" << secondaryDns
                 << "Kraken Server:" << krakenserver;

        emit updateNetwork(dhcp, ip, subnet, gateway,
                           primaryDns, secondaryDns, krakenserver);
        emit setConnectToserverKraken(krakenserver);
    }
}

void DatabaseDF::updateKrakenServer(const QString &ip)
{
    qDebug() << "Updating krakenserver to:" << ip;

    if (!ensureDb()) {
        qWarning() << "[updateKrakenServer] DB open failed";
        return;
    }

    QSqlQuery query(db);
    QString queryStr =
        "INSERT INTO iScreenparameter (id, krakenserver) "
        "VALUES (1, :ip) "
        "ON DUPLICATE KEY UPDATE krakenserver = :ip";
    query.prepare(queryStr);
    query.bindValue(":ip", ip);

    if (!query.exec()) {
        qWarning() << "[updateKrakenServer] Query failed:"
                   << query.lastError().text();
    } else {
        qDebug() << "Kraken server IP updated successfully:" << ip;
        // emit setConnectToserverKraken(ip);  // ถ้าต้องการ auto connect
    }
}

void DatabaseDF::setNetworkSlot(const QString &dhcp,
                                const QString &ip,
                                const QString &subnet,
                                const QString &gateway,
                                const QString &primaryDns,
                                const QString &secondaryDns,
                                const QString &krakenserver)
{
    qDebug() << "[setNetworkSlot] update Network";

    if (!ensureDb()) {
        qWarning() << "[setNetworkSlot] DB open failed";
        return;
    }

    QSqlQuery query(db);
    QString queryStr = QString(
                           "UPDATE Network "
                           "SET DHCP='%1',IP_ADDRESS='%2',SUBNETMASK='%3',"
                           "    GATEWAY='%4',PRIMARY_DNS='%5',SECONDARY_DNS='%6',"
                           "    krakenserver='%7';")
                           .arg(dhcp)
                           .arg(ip)
                           .arg(subnet)
                           .arg(gateway)
                           .arg(primaryDns)
                           .arg(secondaryDns)
                           .arg(krakenserver);

    qDebug() << "command set Network:" << queryStr;

    if (!query.exec(queryStr)) {
        qWarning() << "[setNetworkSlot] Query failed:"
                   << query.lastError().text();
        return;
    } else {
        qDebug() << "[setNetworkSlot] Data set successfully";
    }
}

// ===== NTPServer =====

void DatabaseDF::getNTPServer()
{
    qDebug() << "getNTPServer from database";

    if (!ensureDb()) {
        qWarning() << "[getNTPServer] DB open failed";
        return;
    }

    QSqlQuery query(db);
    QString queryStr = "SELECT IP_ADDRESS, LOCATION, Method FROM NTPServer;";

    if (!query.exec(queryStr)) {
        qWarning() << "[getNTPServer] Query failed:"
                   << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString ip       = query.value(0).toString();
        QString location = query.value(1).toString();
        int method       = query.value(2).toInt();

        qDebug() << "IP_ADDRESS:" << ip
                 << "LOCATION:" << location
                 << "Method:" << method;

        emit updateNTPServer(ip, location, method);
    }
}

void DatabaseDF::setNTPServer(const QString &ip)
{
    qDebug() << "setNTPServer from database";

    if (!ensureDb()) {
        qWarning() << "[setNTPServer] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE NTPServer SET IP_ADDRESS=:ip;");
    query.bindValue(":ip", ip);

    if (!query.exec()) {
        qWarning() << "[setNTPServer] Query failed:"
                   << query.lastError().text();
        return;
    } else {
        qDebug() << "[setNTPServer] Data updated successfully";
    }
}

void DatabaseDF::setNTPServerMethod(const int &method)
{
    qDebug() << "setNTPServerMethod from database";

    if (!ensureDb()) {
        qWarning() << "[setNTPServerMethod] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE NTPServer SET Method=:method;");
    query.bindValue(":method", method);

    if (!query.exec()) {
        qWarning() << "[setNTPServerMethod] Query failed:"
                   << query.lastError().text();
        return;
    } else {
        qDebug() << "[setNTPServerMethod] Data updated successfully";
    }
}

void DatabaseDF::setNTPServerLocation(const QString &location)
{
    qDebug() << "setNTPServerLocation from database";

    if (!ensureDb()) {
        qWarning() << "[setNTPServerLocation] DB open failed";
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE NTPServer SET LOCATION=:location;");
    query.bindValue(":location", location);

    if (!query.exec()) {
        qWarning() << "[setNTPServerLocation] Query failed:"
                   << query.lastError().text();
        return;
    } else {
        qDebug() << "[setNTPServerLocation] Data updated successfully";
    }
}

// ===== Network2 (multi NIC) =====

void DatabaseDF::getNetworkfromDb()
{
    if (!ensureDb()) {
        qWarning() << "[getNetworkfromDb] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare(R"(
        SELECT id, DHCP, IP_ADDRESS, SUBNETMASK, GATEWAY,
               PRIMARY_DNS, SECONDARY_DNS, phyName, krakenserver
        FROM Network2
        ORDER BY id ASC
    )");

    if (!qry.exec()) {
        qWarning() << "[getNetworkfromDb] Query failed:"
                   << qry.lastError();
        return;
    }

    while (qry.next()) {
        emit NetworkAppen(
            qry.value("id").toInt(),
            qry.value("DHCP").toString(),
            qry.value("IP_ADDRESS").toString(),
            qry.value("SUBNETMASK").toString(),
            qry.value("GATEWAY").toString(),
            qry.value("PRIMARY_DNS").toString(),
            qry.value("SECONDARY_DNS").toString(),
            qry.value("phyName").toString(),
            qry.value("krakenserver").toString()
            );
    }
}

void DatabaseDF::updateNetworkfromDisplay(int displayIndex,
                                          const QString &dhcp,
                                          const QString &ip,
                                          const QString &mask,
                                          const QString &gw,
                                          const QString &dns1,
                                          const QString &dns2)
{
    if (!ensureDb()) {
        qWarning() << "[updateNetworkfromDisplay] DB open failed";
        return;
    }

    int id = displayIndex + 1;

    QSqlQuery q(db);
    q.prepare(R"(
        UPDATE Network2
           SET DHCP          = :dhcp,
               IP_ADDRESS    = :ip,
               SUBNETMASK    = :mask,
               GATEWAY       = :gw,
               PRIMARY_DNS   = :dns1,
               SECONDARY_DNS = :dns2
         WHERE id = :id
    )");

    q.bindValue(":dhcp", dhcp);
    q.bindValue(":ip", ip);
    q.bindValue(":mask", mask);
    q.bindValue(":gw", gw);
    q.bindValue(":dns1", dns1);
    q.bindValue(":dns2", dns2);
    q.bindValue(":id", id);

    if (!q.exec()) {
        qWarning() << "[updateNetworkfromDisplay] UPDATE Network2 failed:"
                   << q.lastError();
        return;
    }

    qDebug() << "[updateNetworkfromDisplay] SQL prepared ="
             << q.lastQuery();

    qDebug() << "[updateNetworkfromDisplay] Updated Network2 row id =" << id;
    if (id == 3) {
        emit updateNetworkDfDevice("end0",dhcp,ip,mask,gw,dns1,dns2);
    }else if (id == 4) {
        emit updateNetworkDfDevice("end1",dhcp,ip,mask,gw,dns1,dns2);
    }
    getNetworkfromDb();
}

// ===== Groups / Remote devices (QML JSON) =====

void DatabaseDF::editGroupName(const QString &uniqueIdInGroup,
                               const QString &title)
{
    if (!ensureDb()) {
        qWarning() << "[editGroupName] DB open failed";
        return;
    }

    if (uniqueIdInGroup.trimmed().isEmpty()) {
        qWarning() << "[editGroupName] uniqueIdInGroup is empty, skip";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare(QStringLiteral(
        "UPDATE DeviceGroups "
        "SET GroupsName = :name "
        "WHERE uniqueIdInGroup = :uid"
        ));
    qry.bindValue(":name", title);
    qry.bindValue(":uid",  uniqueIdInGroup);

    if (!qry.exec()) {
        qWarning() << "[editGroupName] update failed:"
                   << qry.lastError().text();
    } else {
        qDebug() << "[editGroupName] updated groupName to"
                 << title
                 << "for uniqueIdInGroup =" << uniqueIdInGroup
                 << ", rowsAffected =" << qry.numRowsAffected();
    }

    // reload กลับไปให้ QML
    getRemoteGroups();
    getGroupsInGroupSetting();
}

void DatabaseDF::getRemoteGroups()
{
    if (!ensureDb()) {
        QJsonObject obj({
            { "objectName", "RemoteGroups" },
            { "error", "DB open failed" },
            { "records", QJsonArray() }
        });
        emit remoteGroupsJson(QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        return;
    }

    static const char *SQL =
        "SELECT "
        "  dg.GroupsName, "
        "  dg.GroupID, "
        "  dg.uniqueIdInGroup, "
        "  dl.Name AS DeviceName, "
        "  dl.IPAddress, "
        "  dl.Port, "
        "  dl.deviceUniqueId "         // ⭐ ส่ง deviceUniqueId ของ DeviceList ออกไปด้วย
        "FROM DeviceGroups dg "
        // ❌ เดิม: JOIN DeviceList dl ON dl.id = dg.DeviceID
        // ✅ ใหม่: join ด้วย deviceUniqueId
        "JOIN DeviceList dl ON dl.deviceUniqueId = dg.deviceUniqueId "
        "ORDER BY dg.GroupID, dl.Name";

    QSqlQuery q(db);
    if (!q.exec(QString::fromUtf8(SQL))) {
        qWarning() << "[MySQL] getRemoteGroups query failed:"
                   << q.lastError().text();
        QJsonObject obj({
            { "objectName", "RemoteGroups" },
            { "error", q.lastError().text() },
            { "records", QJsonArray() }
        });
        emit remoteGroupsJson(QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        return;
    }

    auto statusForIp = [](const QString & /*ip*/) -> QString {
        return "Offline";
    };

    QJsonArray records;
    while (q.next()) {
        const QString groupsName      = q.value(0).toString();
        const int     groupsID        = q.value(1).toInt();
        const QString uniqueIdInGroup = q.value(2).toString();
        const QString devName         = q.value(3).toString();
        const QString ip              = q.value(4).toString();
        const int     port            = q.value(5).toInt();
        const QString deviceUid       = q.value(6).toString();   // ⭐ deviceUniqueId

        QJsonObject rec;
        rec["GroupsName"]      = groupsName;
        rec["GroupsID"]        = groupsID;
        rec["uniqueIdInGroup"] = uniqueIdInGroup;

        rec["DeviceName"]      = devName;
        rec["IPAddress"]       = ip;
        rec["Port"]            = port;
        rec["status"]          = statusForIp(ip);

        rec["deviceUniqueId"]  = deviceUid;   // ⭐ ส่งไป QML / UI ใช้ต่อ

        records.push_back(rec);
    }

    QJsonObject payload;
    payload["objectName"] = "RemoteGroups";
    payload["records"]    = records;

    const QString json = QString::fromUtf8(
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    emit remoteGroupsJson(json);
}



void DatabaseDF::getSideRemote()
{
    if (!ensureDb()) {
        QJsonObject obj({
            { "objectName", "DeviceList" },
            { "error", "DB open failed" },
            { "records", QJsonArray() }
        });
        emit remoteSideRemoteJson(QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        return;
    }

    static const char *SQL =
        "SELECT id, Name, IPAddress, Port, groupID, deviceUniqueId "
        "FROM DeviceList "
        "ORDER BY id";

    QSqlQuery q(db);
    if (!q.exec(QString::fromUtf8(SQL))) {
        qWarning() << "[MySQL] getSideRemote query failed:"
                   << q.lastError().text();
        QJsonObject obj({
            { "objectName", "DeviceList" },
            { "error", q.lastError().text() },
            { "records", QJsonArray() }
        });
        emit remoteSideRemoteJson(QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        return;
    }

    auto defaultStatus = [](const QString & /*ip*/) -> QString {
        return "Offline";
    };
    auto defaultRssi = [](int i) -> int {
        return (i % 5);
    };

    QJsonArray records;
    int row = 0;

    while (q.next()) {
        const int     id             = q.value("id").toInt();
        const QString name           = q.value("Name").toString();
        const QString ip             = q.value("IPAddress").toString();
        const int     port           = q.value("Port").toInt();
        const QVariant groupId       = q.value("groupID");           // may be NULL
        const QString deviceUniqueId = q.value("deviceUniqueId").toString();

        QJsonObject rec;
        rec["id"]             = id;
        rec["name"]           = name;
        rec["ip"]             = ip;
        rec["port"]           = port;
        rec["groupID"]        = groupId.isNull() ? QJsonValue() : QJsonValue(groupId.toInt());
        rec["deviceUniqueId"] = deviceUniqueId;
        rec["status"]         = defaultStatus(ip);
        rec["rssi"]           = defaultRssi(row++);

        records.push_back(rec);
    }

    QJsonObject payload;
    payload["objectName"] = "DeviceList";
    payload["records"]    = records;

    const QString json = QString::fromUtf8(
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    emit remoteSideRemoteJson(json);
}

//////////////////////////////////////////get POPUP GROUP////////////////////////////////////////////////////////////
/// \brief Database::getGroupsInGroupSetting
///
void DatabaseDF::getGroupsInGroupSetting()
{
    if (!ensureDb()) {
        QJsonObject obj;
        obj["ok"]      = false;
        obj["error"]   = "DB open failed";
        obj["devices"] = QJsonArray();
        obj["groups"]  = QJsonArray();
        emit sigGroupsInGroupSetting(
            QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return;
    }

    QJsonObject response;
    response["ok"] = true;

    // ========== Devices ==========
    QJsonArray devicesArr;
    {
        QSqlQuery q1(db);
        // ⭐ ดึง deviceUniqueId มาด้วย
        if (q1.exec("SELECT id, Name, IPAddress, Port, deviceUniqueId "
                    "FROM DeviceList ORDER BY id")) {
            while (q1.next()) {
                QJsonObject dev;
                dev["id"]             = q1.value(0).toInt();
                dev["name"]           = q1.value(1).toString();
                dev["ip"]             = q1.value(2).toString();
                dev["port"]           = q1.value(3).toInt();
                dev["deviceUniqueId"] = q1.value(4).toString();  // ⭐ ใส่เพิ่ม

                devicesArr.append(dev);
            }
        } else {
            qWarning() << "[getGroupsInGroupSetting] DeviceList query failed:"
                       << q1.lastError().text();
        }
    }
    response["devices"] = devicesArr;

    // ========== Groups ==========
    QJsonArray groupsArr;
    {
        QSqlQuery q2(db);
        // ❌ เดิม: SELECT GroupsName, GroupID, DeviceID, uniqueIdInGroup
        // ✅ ใหม่: ใช้ deviceUniqueId แทน DeviceID
        if (q2.exec("SELECT GroupsName, GroupID, deviceUniqueId, uniqueIdInGroup "
                    "FROM DeviceGroups ORDER BY id ASC")) {

            // key: "<GroupID>|<uniqueIdInGroup>"
            QMap<QString, QJsonObject> map;

            while (q2.next()) {
                const QString groupName       = q2.value(0).toString();
                const int     groupId         = q2.value(1).toInt();
                const QString deviceUid       = q2.value(2).toString();
                const QString uniqueIdInGroup = q2.value(3).toString();

                if (groupId == 0) {
                    qWarning() << "[getGroupsInGroupSetting] skip row with GroupID = 0";
                    continue;
                }

                const QString key = QString("%1|%2")
                                        .arg(groupId)
                                        .arg(uniqueIdInGroup);

                QJsonObject g;
                if (map.contains(key)) {
                    g = map.value(key);
                } else {
                    g["name"]            = groupName;
                    g["groupID"]         = groupId;
                    g["uniqueIdInGroup"] = uniqueIdInGroup;
                    g["devices"]         = QJsonArray();   // จะเก็บเป็น array ของ deviceUniqueId (string)
                }

                QJsonArray devArr = g["devices"].toArray();
                devArr.append(deviceUid);                 // ⭐ เก็บ deviceUniqueId แทน int ID
                g["devices"] = devArr;

                map.insert(key, g);
            }

            for (auto it = map.begin(); it != map.end(); ++it) {
                QJsonObject g = it.value();
                QJsonArray devArr = g["devices"].toArray();
                g["count"] = devArr.size();
                groupsArr.append(g);
            }

        } else {
            qWarning() << "[getGroupsInGroupSetting] DeviceGroups query failed:"
                       << q2.lastError().text();
        }
    }

    response["groups"] = groupsArr;

    emit sigGroupsInGroupSetting(
        QJsonDocument(response).toJson(QJsonDocument::Compact));
}


void DatabaseDF::getGroupByUid(const QString &uniqueIdInGroup)
{
    if (!ensureDb()) {
        QJsonObject out;
        out["ok"]      = false;
        out["error"]   = "DB open failed";
        out["devices"] = QJsonArray();
        out["groups"]  = QJsonArray();
        emit sigGroupsInGroupSetting(
            QJsonDocument(out).toJson(QJsonDocument::Compact));
        return;
    }

    // ========== Devices ==========
    QJsonArray devicesArr;
    {
        QSqlQuery q(db);
        // ⭐ เพิ่ม deviceUniqueId ใน SELECT
        if (!q.exec("SELECT id, Name, IPAddress, Port, deviceUniqueId "
                    "FROM DeviceList ORDER BY Name ASC")) {
            QJsonObject out;
            out["ok"]      = false;
            out["error"]   = QString("DeviceList query failed: %1")
                               .arg(q.lastError().text());
            out["devices"] = QJsonArray();
            out["groups"]  = QJsonArray();
            emit sigGroupsInGroupSetting(
                QJsonDocument(out).toJson(QJsonDocument::Compact));
            return;
        }

        while (q.next()) {
            QJsonObject d;
            d["id"]             = q.value(0).toInt();
            d["name"]           = q.value(1).toString();
            d["ip"]             = q.value(2).toString();
            d["port"]           = q.value(3).toInt();
            d["deviceUniqueId"] = q.value(4).toString();  // ⭐ ใส่เพิ่ม

            devicesArr.append(d);
        }
    }

    // ========== หา GroupID + GroupName จาก uniqueIdInGroup ==========
    int     groupId   = -1;
    QString groupName = "Unknown";
    {
        QSqlQuery q(db);
        q.prepare("SELECT GroupsName, GroupID "
                  "FROM DeviceGroups "
                  "WHERE uniqueIdInGroup = :uid "
                  "ORDER BY id DESC LIMIT 1");
        q.bindValue(":uid", uniqueIdInGroup);
        if (q.exec() && q.next()) {
            groupName = q.value(0).toString();
            groupId   = q.value(1).toInt();
        } else {
            QJsonObject out;
            out["ok"]      = false;
            out["error"]   = QString("Group not found for uid: %1")
                               .arg(uniqueIdInGroup);
            out["devices"] = devicesArr;
            out["groups"]  = QJsonArray();
            emit sigGroupsInGroupSetting(
                QJsonDocument(out).toJson(QJsonDocument::Compact));
            return;
        }
    }

    // ========== deviceUniqueId ทั้งหมดใน group นี้ ==========
    QJsonArray devUids;
    {
        QSqlQuery q(db);
        // ❌ เดิม: SELECT DISTINCT DeviceID ...
        // ✅ ใหม่: SELECT DISTINCT deviceUniqueId ...
        q.prepare("SELECT DISTINCT deviceUniqueId FROM DeviceGroups "
                  "WHERE uniqueIdInGroup = :uid "
                  "ORDER BY deviceUniqueId ASC");
        q.bindValue(":uid", uniqueIdInGroup);
        if (!q.exec()) {
            QJsonObject out;
            out["ok"]      = false;
            out["error"]   = QString("DeviceIDs query failed: %1")
                               .arg(q.lastError().text());
            out["devices"] = devicesArr;
            out["groups"]  = QJsonArray();
            emit sigGroupsInGroupSetting(
                QJsonDocument(out).toJson(QJsonDocument::Compact));
            return;
        }
        while (q.next())
            devUids.append(q.value(0).toString());   // ⭐ เก็บเป็น string deviceUniqueId
    }

    // ========== groupsArr ==========
    QJsonArray groupsArr;
    {
        QJsonObject g;
        g["groupID"]         = groupId;
        g["name"]            = groupName;
        g["uniqueIdInGroup"] = uniqueIdInGroup;
        g["devices"]         = devUids;             // ⭐ ตอนนี้เป็น array ของ deviceUniqueId
        g["count"]           = devUids.size();
        groupsArr.append(g);
    }

    // ========== response ==========
    QJsonObject resp;
    resp["ok"]      = true;
    resp["devices"] = devicesArr;
    resp["groups"]  = groupsArr;

    emit sigGroupsInGroupSetting(
        QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief Database::getLocalMacLastOctet
/// \return
///

QString DatabaseDF::getLocalMacLastOctet()
{
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &iface : ifaces) {
        // เอาเฉพาะ interface ที่ up และไม่ใช่ loopback
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

        QString mac = iface.hardwareAddress().trimmed();
        if (!mac.isEmpty()) {
            // ตัดเอา 2 octet สุดท้าย เช่น "AA:BB:CC:DD:EE:7F" -> "EE7F"
            return mac.section(':', -2).remove(":").toUpper();
        }
    }

    // fallback ถ้าไม่มี MAC
    return "00";
}
////////////////////////////////////New Group add remove /adddevice /removedevice/////////////////////////////////////////////
void DatabaseDF::savegroupSettingNewGroup(const QString &groupName,
                                          const QList<QString> &deviceUniqueIds,
                                          int &outGroupID,
                                          QString &outUniqueIdInGroup)
{
    qDebug() << "[Database] savegroupSettingNewGroup:"
             << "groupName =" << groupName
             << "deviceUniqueIds =" << deviceUniqueIds;

    outGroupID = 0;
    outUniqueIdInGroup.clear();

    if (!ensureDb()) {
        qWarning() << "[savegroupSettingNewGroup] DB open failed";
        return;
    }

    if (groupName.trimmed().isEmpty()) {
        qWarning() << "[savegroupSettingNewGroup] groupName is empty";
        return;
    }

    if (deviceUniqueIds.isEmpty()) {
        qWarning() << "[savegroupSettingNewGroup] deviceUniqueIds is empty (ต้องมี device อย่างน้อย 1 ตัว)";
        return;
    }

    // ---------- หา GroupID ใหม่ ----------
    int newGroupID = 0;
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT IFNULL(MAX(GroupID), 0) + 1 AS next_gid FROM DeviceGroups")) {
            qWarning() << "[savegroupSettingNewGroup] SELECT MAX(GroupID) failed:"
                       << q.lastError().text();
            return;
        }
        if (q.next()) {
            newGroupID = q.value(0).toInt();
        } else {
            newGroupID = 1;   // fallback
        }
    }

    // ---------- สร้าง uniqueIdInGroup ----------
    QString macLast = getLocalMacLastOctet();      // เช่น "27F2"
    QString uniqueId = macLast + QString::number(newGroupID); // เช่น "27F21"
    // ถ้าอยากได้รูป "27F2-1": macLast + "-" + QString::number(newGroupID)

    qDebug() << "[savegroupSettingNewGroup] newGroupID =" << newGroupID
             << "uniqueIdInGroup =" << uniqueId;

    // ---------- INSERT ลง DeviceGroups ----------
    int insertCount = 0;

    for (const QString &duid : deviceUniqueIds) {
        const QString trimmed = duid.trimmed();
        if (trimmed.isEmpty()) {
            qWarning() << "[savegroupSettingNewGroup] skip empty deviceUniqueId";
            continue;
        }

        QSqlQuery ins(db);
        ins.prepare("INSERT INTO DeviceGroups "
                    "(GroupsName, deviceUniqueId, GroupID, uniqueIdInGroup) "
                    "VALUES (:name, :duid, :gid, :uid)");
        ins.bindValue(":name", groupName);
        ins.bindValue(":duid", trimmed);
        ins.bindValue(":gid",  newGroupID);
        ins.bindValue(":uid",  uniqueId);

        if (!ins.exec()) {
            qWarning() << "[savegroupSettingNewGroup] Insert failed:"
                       << ins.lastError().text()
                       << "deviceUniqueId=" << trimmed;
        } else {
            insertCount++;
        }
    }

    outGroupID         = newGroupID;
    outUniqueIdInGroup = uniqueId;

    qDebug() << "[savegroupSettingNewGroup] Created new groupID" << newGroupID
             << "uniqueIdInGroup =" << uniqueId
             << "with" << insertCount << "devices";

    // reload ข้อมูลให้ QML
    getRemoteGroups();
    getGroupsInGroupSetting();
}

void DatabaseDF::insertDevicesinGroup(int groupID,
                                      const QString &groupName,
                                      const QString &deviceUniqueId,
                                      const QString &uniqueIdInGroup)
{
    if (!ensureDb()) {
        qWarning() << "[insertDevicesinGroup] DB open failed";
        return;
    }

    qDebug() << "[insertDevicesinGroup] INSERT →"
             << "groupID ="        << groupID
             << "groupName ="      << groupName
             << "deviceUniqueId =" << deviceUniqueId
             << "uid ="            << uniqueIdInGroup;

    QSqlQuery q(db);
    q.prepare("INSERT INTO DeviceGroups "
              "(GroupsName, deviceUniqueId, GroupID, uniqueIdInGroup) "
              "VALUES (:name, :duid, :gid, :uid)");

    q.bindValue(":name", groupName);
    q.bindValue(":duid", deviceUniqueId);
    q.bindValue(":gid",  groupID);
    q.bindValue(":uid",  uniqueIdInGroup);

    if (!q.exec()) {
        qWarning() << "[insertDevicesinGroup] Insert failed:"
                   << q.lastError().text();
        return;
    }

    qDebug() << "[insertDevicesinGroup] Insert OK";

    // reload กลับไปให้ QML
    getRemoteGroups();   // หรือ getGroupsInGroupSetting()
}

void DatabaseDF::removeDeviceFromGroup(int groupID,
                                       const QString &deviceUniqueId,
                                       const QString &uniqueIdInGroup)
{
    if (!ensureDb()) {
        qWarning() << "[removeDeviceFromGroup] DB open failed";
        return;
    }

    qDebug() << "[removeDeviceFromGroup] DELETE →"
             << "groupID ="        << groupID
             << "deviceUniqueId =" << deviceUniqueId
             << "uid ="            << uniqueIdInGroup;

    QSqlQuery q(db);
    q.prepare("DELETE FROM DeviceGroups "
              "WHERE GroupID = :gid "
              "  AND deviceUniqueId = :duid "
              "  AND uniqueIdInGroup = :uid");

    q.bindValue(":gid",  groupID);
    q.bindValue(":duid", deviceUniqueId);
    q.bindValue(":uid",  uniqueIdInGroup);

    if (!q.exec()) {
        qWarning() << "[removeDeviceFromGroup] Delete failed:"
                   << q.lastError().text();
        return;
    }

    qDebug() << "[removeDeviceFromGroup] rowsAffected =" << q.numRowsAffected();

    // reload กลับไปให้ QML
    getRemoteGroups();
    getGroupsInGroupSetting();
}

void DatabaseDF::deleteGroupByUID(const QString &uniqueIdInGroup)
{
    if (!ensureDb()) {
        qWarning() << "[deleteGroupByUID] DB open failed";
        return;
    }

    qDebug() << "[deleteGroupByUID] Delete all where uniqueIdInGroup =" << uniqueIdInGroup;

    QSqlQuery q(db);
    q.prepare("DELETE FROM DeviceGroups WHERE uniqueIdInGroup = :uid");
    q.bindValue(":uid", uniqueIdInGroup);

    if (!q.exec()) {
        qWarning() << "[deleteGroupByUID] delete failed:"
                   << q.lastError().text();
        return;
    }

    qDebug() << "[deleteGroupByUID] Delete OK";

    // reload กลับ QML
    getRemoteGroups();
}
void DatabaseDF::updateDeviceInGroup(int groupID,
                                     const QString &groupName,
                                     const QString &deviceUniqueId,
                                     int roleIndex,
                                     const QString &uniqueIdInGroup)
{
    if (!ensureDb()) {
        qWarning() << "[updateDeviceInGroup] DB open failed";
        return;
    }

    // roleIndex = DeviceGroups.id เดิมที่ต้องการแก้ไข
    qDebug() << "[updateDeviceInGroup] UPDATE →"
             << "id ="             << roleIndex
             << "groupID ="        << groupID
             << "groupName ="      << groupName
             << "deviceUniqueId =" << deviceUniqueId
             << "uid ="            << uniqueIdInGroup;

    QSqlQuery q(db);
    q.prepare("UPDATE DeviceGroups "
              "SET GroupsName = :name, "
              "    deviceUniqueId = :duid, "
              "    GroupID = :gid, "
              "    uniqueIdInGroup = :uid "
              "WHERE id = :id");

    q.bindValue(":name", groupName);
    q.bindValue(":duid", deviceUniqueId);
    q.bindValue(":gid",  groupID);
    q.bindValue(":uid",  uniqueIdInGroup);
    q.bindValue(":id",   roleIndex);   // ⭐ ใช้ id เดิม

    if (!q.exec()) {
        qWarning() << "[updateDeviceInGroup] Update failed:"
                   << q.lastError().text();
        return;
    }

    qDebug() << "[updateDeviceInGroup] Update OK";

    // reload กลับไปให้ฝั่ง Web/QML
    getRemoteGroups();   // หรือฟังก์ชันที่คุณใช้ broadcast group list
}

//////////////////////////////////////////////////////////////////////////////////
// void Database::savegroupSettingBygroupID(const QString &json)
// {
//     qDebug() << "[Database] EditGroupbyID:" << json;

//     if (!ensureDb()) {
//         qWarning() << "[savegroupSettingBygroupID] DB open failed";
//         return;
//     }

//     QJsonParseError err;
//     QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
//     if (err.error != QJsonParseError::NoError || !doc.isObject()) {
//         qWarning() << "[savegroupSettingBygroupID] JSON parse failed:"
//                    << err.errorString();
//         return;
//     }

//     QJsonObject root   = doc.object();
//     QJsonArray  groups = root.value("groups").toArray();
//     if (groups.isEmpty()) {
//         qWarning() << "[savegroupSettingBygroupID] No groups to update";
//         return;
//     }

//     QJsonObject g       = groups.first().toObject();
//     int         groupID = g.value("groupID").toInt();
//     QString     groupName = g.value("groupName").toString();
//     QJsonArray  devs      = g.value("devices").toArray();

//     qDebug() << "→ EditGroupbyID:" << groupID
//              << groupName << ", devices:" << devs.size();

//     {
//         QSqlQuery q(db);
//         q.prepare("DELETE FROM DeviceGroups WHERE GroupID = :gid");
//         q.bindValue(":gid", groupID);
//         if (!q.exec()) {
//             qWarning() << "[savegroupSettingBygroupID] Delete failed:"
//                        << q.lastError().text();
//             return;
//         }
//     }

//     QString macLast = getLocalMacLastOctet();
//     QString uniqueIdInGroup = macLast + QString::number(groupID);

//     qDebug() << "Local MAC last2 octets:" << macLast
//              << "→ uniqueIdInGroup =" << uniqueIdInGroup;

//     for (const QJsonValue &v : devs) {
//         int devID = v.toString().toInt();
//         if (devID <= 0)
//             continue;

//         QSqlQuery ins(db);
//         ins.prepare("INSERT INTO DeviceGroups "
//                     "(GroupsName, DeviceID, GroupID, uniqueIdInGroup) "
//                     "VALUES (:name, :did, :gid, :uid)");
//         ins.bindValue(":name", groupName);
//         ins.bindValue(":did", devID);
//         ins.bindValue(":gid", groupID);
//         ins.bindValue(":uid", uniqueIdInGroup);

//         if (!ins.exec()) {
//             qWarning() << "[savegroupSettingBygroupID] Insert failed:"
//                        << ins.lastError().text();
//         }
//     }

//     getRemoteGroups();
//     qDebug() << "Updated groupID" << groupID
//              << "successfully with" << devs.size() << "devices";
// }

// void Database::saveGroupSettingFromJson(const QString &jsonString)
// {
//     qDebug() << "[saveGroupSettingFromJson] Start";

//     QJsonParseError err;
//     QJsonDocument doc =
//         QJsonDocument::fromJson(jsonString.toUtf8(), &err);

//     if (err.error != QJsonParseError::NoError) {
//         qWarning() << "[saveGroupSettingFromJson] JSON ERROR:"
//                    << err.errorString();
//         return;
//     }

//     QJsonObject root   = doc.object();
//     QJsonArray  groups = root["groups"].toArray();

//     if (!ensureDb()) {
//         qWarning() << "[saveGroupSettingFromJson] DB OPEN FAILED";
//         return;
//     }

//     // ใช้ MAC 2 octet สุดท้ายสำหรับเครื่องนี้
//     const QString macLast = getLocalMacLastOctet();
//     const QString macPrefix = macLast + "%";
//     qDebug() << "[saveGroupSettingFromJson] macLast =" << macLast;

//     db.transaction();

//     // ---------- 1) ลบ groups ที่ไม่อยู่ใน JSON (เฉพาะของเครื่องนี้) ----------
//     QStringList jsonGroupNames;
//     for (auto gVal : groups) {
//         QJsonObject gObj = gVal.toObject();
//         jsonGroupNames << gObj["groupName"].toString();
//     }

//     if (!jsonGroupNames.isEmpty()) {
//         QSqlQuery delMissing(db);
//         QString placeholders =
//             QString("?, ").repeated(jsonGroupNames.size()).chopped(2);

//         QString sql = "DELETE FROM DeviceGroups "
//                       "WHERE uniqueIdInGroup LIKE ? "
//                       "AND GroupsName NOT IN (" + placeholders + ")";

//         delMissing.prepare(sql);
//         delMissing.addBindValue(macPrefix);  // เช่น "EE7F%"

//         for (const QString &name : jsonGroupNames)
//             delMissing.addBindValue(name);

//         if (!delMissing.exec()) {
//             qWarning() << "[saveGroupSettingFromJson] "
//                           "Delete Missing Groups Error:"
//                        << delMissing.lastError().text();
//         } else {
//             qDebug() << "[Deleted] Groups not in JSON for this controller";
//         }
//     }

//     QSqlQuery q(db);

//     // ---------- 2) อัปเดต / สร้าง group ตาม JSON ----------
//     for (auto gVal : groups) {
//         QJsonObject gObj = gVal.toObject();
//         QString groupName = gObj["groupName"].toString();
//         QJsonArray devArr = gObj["devices"].toArray();

//         int groupID = -1;

//         // 2.1 หา GroupID เดิมของ group นี้ (เฉพาะของเครื่องนี้)
//         q.prepare("SELECT GroupID FROM DeviceGroups "
//                   "WHERE GroupsName = ? AND uniqueIdInGroup LIKE ? "
//                   "LIMIT 1");
//         q.addBindValue(groupName);
//         q.addBindValue(macPrefix);
//         if (!q.exec()) {
//             qWarning() << "[saveGroupSettingFromJson] SELECT GroupID error:"
//                        << q.lastError().text();
//         }

//         if (q.next()) {
//             groupID = q.value(0).toInt();
//         } else {
//             // 2.2 ถ้าไม่มี group เดิม -> หาค่า MAX(GroupID) ของเครื่องนี้ แล้ว +1
//             QSqlQuery getMax(db);
//             getMax.prepare("SELECT MAX(GroupID) FROM DeviceGroups "
//                            "WHERE uniqueIdInGroup LIKE ?");
//             getMax.addBindValue(macPrefix);
//             if (!getMax.exec()) {
//                 qWarning() << "[saveGroupSettingFromJson] SELECT MAX(GroupID) error:"
//                            << getMax.lastError().text();
//                 groupID = 1;
//             } else if (getMax.next()) {
//                 groupID = getMax.value(0).toInt() + 1;
//                 if (groupID <= 0)
//                     groupID = 1;
//             } else {
//                 groupID = 1;
//             }
//         }

//         // ⭐ คำนวณ uniqueIdInGroup = MAC + GroupID
//         QString uniqueIdInGroup = macLast + QString::number(groupID);

//         // 2.3 ลบ groupName นี้ของเครื่องนี้ก่อน แล้วค่อย insert ใหม่
//         QSqlQuery del(db);
//         del.prepare("DELETE FROM DeviceGroups "
//                     "WHERE GroupsName = ? AND uniqueIdInGroup LIKE ?");
//         del.addBindValue(groupName);
//         del.addBindValue(macPrefix);
//         if (!del.exec()) {
//             qWarning() << "[saveGroupSettingFromJson] Delete group error:"
//                        << del.lastError().text();
//         }

//         // 2.4 INSERT ใหม่พร้อม uniqueIdInGroup
//         QSqlQuery ins(db);
//         ins.prepare("INSERT INTO DeviceGroups "
//                     "(GroupsName, DeviceID, GroupID, uniqueIdInGroup) "
//                     "VALUES (?, ?, ?, ?)");

//         for (auto dVal : devArr) {
//             int devID = dVal.toString().toInt();
//             if (devID <= 0)
//                 continue;

//             ins.addBindValue(groupName);
//             ins.addBindValue(devID);
//             ins.addBindValue(groupID);
//             ins.addBindValue(uniqueIdInGroup);

//             if (!ins.exec()) {
//                 qWarning() << "[saveGroupSettingFromJson] Insert error:"
//                            << ins.lastError().text()
//                            << "groupName=" << groupName
//                            << "devID=" << devID
//                            << "groupID=" << groupID
//                            << "uniqueIdInGroup=" << uniqueIdInGroup;
//             }
//         }

//         qDebug() << "[Updated Group]" << groupName
//                  << "GroupID:" << groupID
//                  << "Device Count:" << devArr.size()
//                  << "uniqueIdInGroup:" << uniqueIdInGroup;
//     }

//     db.commit();
//     getRemoteGroups();
//     qDebug() << "[saveGroupSettingFromJson]  Sync Complete!";
// }


// ===== DeviceList CRUD =====
QString DatabaseDF::generateShortUuid()
{
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    uuid.remove('-');
    return uuid.left(8).toUpper();
}
// เดิม: void Database::addNewDevice(const QString &name, const QString &ip)
void DatabaseDF::addNewDevice(const QString &name,
                              const QString &ip,
                              const QString &deviceUidFromUi)
{
    if (!ensureDb()) {
        qWarning() << "[addNewDevice] DB open failed";
        return;
    }

    // --- เช็คว่าซ้ำ IP หรือไม่ ---
    QSqlQuery check(db);
    check.prepare("SELECT id FROM DeviceList WHERE IPAddress = ?");
    check.addBindValue(ip);

    if (!check.exec()) {
        qWarning() << "[addNewDevice] SELECT Error:"
                   << check.lastError().text();
        return;
    }

    if (check.next()) {
        qDebug() << "[DB] IP already exists — skip insert";
        return;
    }

    // --- ตัดสินใจ deviceUniqueId ---
    QString deviceUniqueId = deviceUidFromUi.trimmed();

    // ถ้า frontend ไม่ได้ส่งมา → สุ่มใหม่
    if (deviceUniqueId.isEmpty()) {
        deviceUniqueId = generateShortUuid();
        qDebug() << "[addNewDevice] No deviceUid from UI, generated:"
                 << deviceUniqueId;
    } else {
        qDebug() << "[addNewDevice] Use deviceUid from UI:"
                 << deviceUniqueId;
    }
    {
        QSqlQuery qCheckUid(db);
        qCheckUid.prepare("SELECT id FROM DeviceList WHERE deviceUniqueId = ?");
        qCheckUid.addBindValue(deviceUniqueId);
        if (qCheckUid.exec() && qCheckUid.next()) {
            QString newUid = generateShortUuid();
            qWarning() << "[addNewDevice] deviceUniqueId duplicated, regenerate:"
                       << deviceUniqueId << "->" << newUid;
            deviceUniqueId = newUid;
        }
    }

    // --- INSERT ---
    QSqlQuery q(db);
    q.prepare("INSERT INTO DeviceList "
              "(Name, IPAddress, Port, deviceUniqueId) "
              "VALUES (?, ?, ?, ?)");

    q.addBindValue(name);
    q.addBindValue(ip);
    q.addBindValue(8000);
    q.addBindValue(deviceUniqueId);

    if (!q.exec()) {
        qWarning() << "[addNewDevice] INSERT Error:"
                   << q.lastError().text();
        return;
    }

    getSideRemote();   // refresh UI / sync

    qDebug() << "[DB] INSERT New Device:"
             << name << ip
             << "deviceUniqueId =" << deviceUniqueId;
}

void DatabaseDF::deleteDeviceByUniqueId(const QString &deviceUniqueId)
{
    if (!ensureDb()) {
        qWarning() << "[deleteDeviceByUniqueId] DB open failed";
        return;
    }

    // 1) หา id, name, ip จาก deviceUniqueId ก่อน
    int id = -1;
    QString name;
    QString ip;

    {
        QSqlQuery sel(db);
        sel.prepare("SELECT id, Name, IPAddress "
                    "FROM DeviceList "
                    "WHERE deviceUniqueId = :uid");
        sel.bindValue(":uid", deviceUniqueId);

        if (!sel.exec()) {
            qWarning() << "[deleteDeviceByUniqueId] SELECT Error:"
                       << sel.lastError().text();
            return;
        }

        if (!sel.next()) {
            qWarning() << "[deleteDeviceByUniqueId] No device with deviceUniqueId ="
                       << deviceUniqueId;
            return;
        }

        id   = sel.value(0).toInt();
        name = sel.value(1).toString();
        ip   = sel.value(2).toString();
    }

    // 2) ลบจาก DeviceList ด้วย deviceUniqueId
    {
        QSqlQuery qry(db);
        qry.prepare("DELETE FROM DeviceList WHERE deviceUniqueId = :uid");
        qry.bindValue(":uid", deviceUniqueId);

        if (!qry.exec()) {
            qWarning() << "[deleteDeviceByUniqueId] DELETE Error:"
                       << qry.lastError().text();
            return;
        }
    }

    qDebug() << "[deleteDeviceByUniqueId] Deleted device:"
             << "id =" << id
             << "name =" << name
             << "ip =" << ip
             << "deviceUniqueId =" << deviceUniqueId;

    // 3) ลบใน DeviceGroups / อื่น ๆ ต่อโดยใช้ id เดิม
    deletDeviceInGroups(id, name, ip);
}


void DatabaseDF::deletDeviceInGroups(int id,const QString &name, const QString &ip)
{
    Q_UNUSED(name)
    Q_UNUSED(ip)

    if (!ensureDb()) {
        qWarning() << "[deletDeviceInGroups] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare("DELETE FROM DeviceGroups WHERE DeviceID=:rid");
    qry.bindValue(":rid", id);
    if (!qry.exec())
        qWarning() << "[deletDeviceInGroups]" << qry.lastError();

    getSideRemote();
}

void DatabaseDF::updateDeviceByUniqueId(const QString &oldUid,
                                        const QString &newUid,
                                        const QString &name,
                                        const QString &ip)
{
    if (!ensureDb()) {
        qWarning() << "[updateDeviceByUniqueId] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare("UPDATE DeviceList "
                "SET Name = :name, "
                "    IPAddress = :ip, "
                "    deviceUniqueId = :newUid "
                "WHERE deviceUniqueId = :oldUid");

    qry.bindValue(":name",  name);
    qry.bindValue(":ip",    ip);
    qry.bindValue(":newUid", newUid);
    qry.bindValue(":oldUid", oldUid);

    if (!qry.exec()) {
        qWarning() << "[updateDeviceByUniqueId] UPDATE FAILED:"
                   << qry.lastError().text();
        return;
    }

    if (qry.numRowsAffected() == 0) {
        qWarning() << "[updateDeviceByUniqueId] No device updated for oldUid:"
                   << oldUid;
    } else {
        qInfo() << "[updateDeviceByUniqueId] Updated device from oldUid="
                << oldUid << "to newUid=" << newUid
                << " name=" << name << " ip=" << ip;
    }
    updateUniqueIdDeviceOngroup(oldUid,newUid);
    getSideRemote();

}
void DatabaseDF::updateUniqueIdDeviceOngroup(const QString &oldUid,
                                             const QString &newUid)
{
    if (!ensureDb()) {
        qWarning() << "[updateUniqueIdDeviceOngroup] DB open failed";
        return;
    }
    QSqlQuery qry(db);
    qry.prepare("UPDATE DeviceGroups "
                "SET deviceUniqueId = :newUid "
                "WHERE deviceUniqueId = :oldUid");

    qry.bindValue(":newUid", newUid);
    qry.bindValue(":oldUid", oldUid);

    if (!qry.exec()) {
        qWarning() << "[updateUniqueIdDeviceOngroup] UPDATE DeviceGroups FAILED:"
                   << qry.lastError().text();
        db.rollback();
        return;
    }

    int affected = qry.numRowsAffected();

    if (!db.commit()) {
        qWarning() << "[updateUniqueIdDeviceOngroup] commit FAILED";
        return;
    }

    qInfo() << "[updateUniqueIdDeviceOngroup] OK:"
            << "oldUid=" << oldUid
            << "newUid=" << newUid
            << "rows="  << affected;

    // refresh group UI/QML ถ้ามีใช้
    getRemoteGroups();
    getGroupsInGroupSetting();
}
// ===== WebSocket server connect helper =====

void DatabaseDF::getAllClientInDatabase()
{
    if (!ensureDb()) {
        qWarning() << "[Database::getAllClientInDatabase] ensureDb() failed";
        return;
    }

    QSqlQuery qry(db);

    qry.prepare(R"( SELECT id, Name, IPAddress, Port FROM DeviceList ORDER BY id ASC)");

    if (!qry.exec()) {
        qWarning() << "[Database::getAllClientInDatabase] Query failed:"
                   << qry.lastError();
        return;
    }

    while (qry.next()) {
        // int      id         = qry.value("id").toInt();
        // QString  name       = qry.value("Name").toString();
        // QString  ip         = qry.value("IPAddress").toString();
        // quint16  socketPort = static_cast<quint16>(qry.value("Port").toInt());

        // qDebug() << "[Database] client row:"
        //          << "id=" << id
        //          << "Name=" << name
        //          << "IP=" << ip
        //          << "socketPort=" << socketPort;

        // emit appendNewClient();
        emit appendNewClient(
            qry.value("id").toInt(),
            qry.value("Name").toString(),
            qry.value("ipaddress").toString(),
            static_cast<uint16_t>(qry.value("Port").toInt())
            );
    }
}
void DatabaseDF::getActiveClientInDatabase()
{
    if (!ensureDb()) {
        qWarning() << "[Database::getActiveClientInDatabase] ensureDb() failed";
        return;
    }

    QSqlQuery qry(db);

    // ใช้ deviceUniqueId + uniqueIdInGroup แทน DeviceList.id / DeviceGroups.id
    qry.prepare(R"(
        SELECT
            DeviceList.deviceUniqueId,
            DeviceList.Name AS DeviceName,
            DeviceList.IPAddress,
            DeviceList.Port,
            DeviceGroups.uniqueIdInGroup,
            DeviceGroups.GroupsName,
            DeviceGroups.GroupID,
            DeviceGroups.deviceUniqueId AS deviceUniqueIdInGroup  -- ถ้าต้องการ
        FROM DeviceList
        INNER JOIN DeviceGroups
            ON DeviceGroups.deviceUniqueId = DeviceList.deviceUniqueId
        ORDER BY DeviceGroups.GroupID
    )");


    if (!qry.exec()) {
        qWarning() << "[Database::getActiveClientInDatabase] Query failed:"
                   << qry.lastError();
        return;
    }

    while (qry.next()) {
        QString deviceUniqueId   = qry.value("deviceUniqueId").toString();
        QString uniqueIdInGroup  = qry.value("uniqueIdInGroup").toString();
        int groupID              = qry.value("GroupID").toInt();
        QString groupName        = qry.value("GroupsName").toString();
        QString deviceName       = qry.value("DeviceName").toString();
        QString ipAddress        = qry.value("IPAddress").toString();
        uint16_t port            = static_cast<uint16_t>(qry.value("Port").toInt());

        qDebug() << "[Database::getActiveClientInDatabase]"
                 << "groupIndex:" << deviceUniqueId
                 << "deviceIndex:" << uniqueIdInGroup
                 << "deviceID:" << groupID
                 << "groupID:" << groupID
                 << "GroupName:" << groupName
                 << "DeviceName:" << deviceName
                 << "IP:" << ipAddress
                 << "Port:" << port;

        emit appendNewActiveClient(
            deviceUniqueId,
            uniqueIdInGroup,
            0,            // deviceID ถ้าไม่ใช้แล้ว ตั้งเป็น 0
            groupID,
            groupName,
            deviceName,
            ipAddress,
            port
            );
    }
}
void DatabaseDF::getActiveClientInDatabase(const QString &uniqueIdInGroupFilter)
{
    if (!ensureDb()) {
        qWarning() << "[Database::getActiveClientInDatabase] ensureDb() failed";
        return;
    }

    QSqlQuery qry(db);

    qry.prepare(R"(
        SELECT
            DeviceList.deviceUniqueId,
            DeviceList.Name AS DeviceName,
            DeviceList.IPAddress,
            DeviceList.Port,
            DeviceGroups.uniqueIdInGroup,
            DeviceGroups.GroupsName,
            DeviceGroups.GroupID,
            DeviceGroups.deviceUniqueId AS deviceUniqueIdInGroup
        FROM DeviceList
        INNER JOIN DeviceGroups
            ON DeviceGroups.deviceUniqueId = DeviceList.deviceUniqueId
        WHERE DeviceGroups.uniqueIdInGroup = :uid
        ORDER BY DeviceGroups.GroupID
    )");

    qry.bindValue(":uid", uniqueIdInGroupFilter);

    if (!qry.exec()) {
        qWarning() << "[Database::getActiveClientInDatabase] Query failed:"
                   << qry.lastError();
        return;
    }

    while (qry.next()) {
        QString deviceUniqueId   = qry.value("deviceUniqueId").toString();
        QString uniqueIdInGroup  = qry.value("uniqueIdInGroup").toString();
        int groupID              = qry.value("GroupID").toInt();
        QString groupName        = qry.value("GroupsName").toString();
        QString deviceName       = qry.value("DeviceName").toString();
        QString ipAddress        = qry.value("IPAddress").toString();
        uint16_t port            = static_cast<uint16_t>(qry.value("Port").toInt());

        qDebug() << "[Database::getActiveClientInDatabase]"
                 << "deviceUniqueId:" << deviceUniqueId
                 << "uniqueIdInGroup:" << uniqueIdInGroup
                 << "groupID:" << groupID
                 << "GroupName:" << groupName
                 << "DeviceName:" << deviceName
                 << "IP:" << ipAddress
                 << "Port:" << port;

        emit appendNewActiveClient(
            deviceUniqueId,
            uniqueIdInGroup,
            0,            // ไม่ใช้ deviceID แล้ว ใส่ 0
            groupID,
            groupName,
            deviceName,
            ipAddress,
            port
            );
    }
}
/////////////////////////////SelectGroup///////////////////////////////////////////
/// \brief Database::getDevicesInGroupJson
/// \param groupUniqueId

void DatabaseDF::getDevicesInGroupJson(const QString &groupUniqueId)
{
    if (!ensureDb()) {
        qWarning() << "[Database] getDevicesInGroupJson: DB not open";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare(
        "SELECT "
        "  dg.id              AS DeviceGroupId, "
        "  dg.GroupsName, "
        "  dg.GroupID, "
        "  dg.uniqueIdInGroup, "
        "  dg.deviceUniqueId, "
        "  dl.Name, "
        "  dl.IPAddress, "
        "  dl.Port "
        "FROM DeviceGroups AS dg "
        "JOIN DeviceList   AS dl ON dl.deviceUniqueId = dg.deviceUniqueId "
        "WHERE dg.uniqueIdInGroup = :uid"
        );
    qry.bindValue(":uid", groupUniqueId);

    if (!qry.exec()) {
        qWarning() << "[Database] getDevicesInGroupJson query failed:"
                   << qry.lastError().text();
        QJsonArray emptyArr;
        emit devicesInGroupJsonReady(-1, QString(), groupUniqueId, emptyArr);
        return;
    }

    QJsonArray devices;
    QString groupName;
    int groupId = -1;

    while (qry.next()) {

        if (groupName.isEmpty())
            groupName = qry.value("GroupsName").toString();
        if (groupId < 0)
            groupId = qry.value("GroupID").toInt();

        QJsonObject dev;
        dev["deviceGroupId"]   = qry.value("DeviceGroupId").toInt();
        dev["groupsName"]      = qry.value("GroupsName").toString();
        dev["groupId"]         = qry.value("GroupID").toInt();
        dev["uniqueIdInGroup"] = qry.value("uniqueIdInGroup").toString();

        // ⭐ ใช้ deviceUniqueId เท่านั้น
        dev["deviceUniqueId"]  = qry.value("deviceUniqueId").toString();

        dev["name"]            = qry.value("Name").toString();
        dev["ip"]              = qry.value("IPAddress").toString();
        dev["port"]            = qry.value("Port").toInt();

        devices.append(dev);
    }

    emit devicesInGroupJsonReady(groupId, groupName, groupUniqueId, devices);
}

void DatabaseDF::getRecorderSettings()
{
    if (!ensureDb()) {
        qWarning() << "[Database] getRecorderSettings: DB not open";
        return;
    }

    QSqlQuery q(db);   // <<< ต้องผูกกับ db
    q.prepare("SELECT * FROM Recorder LIMIT 1");

    if (!q.exec()) {
        qWarning() << "[Database] Query failed:" << q.lastError().text();
        return;
    }

    QString alsaDevice;
    QString clientIp;
    int frequency = 0;
    QString rtspServer;
    QString rtspUrl;
    int rtspPort = 0;

    if (q.next()) {
        alsaDevice  = q.value("ALSA_Device").toString();
        clientIp    = q.value("ClientIP").toString();
        frequency   = q.value("Frequency").toInt();
        rtspServer  = q.value("RTSP_Server").toString();
        rtspUrl     = q.value("RTSP_URL").toString();
        rtspPort    = q.value("RTSP_Port").toInt();
    }

    emit recorderSettingsReady(
        alsaDevice,
        clientIp,
        frequency,
        rtspServer,
        rtspUrl,
        rtspPort
        );

    // qDebug() << "[Recorder] Settings emitted to QML";
}
void DatabaseDF::setRecorderSettingsDB(const QString &alsaDevice,
                                       const QString &clientIp,
                                       int freq,
                                       const QString &rtspServer,
                                       const QString &rtspUrl,
                                       int rtspPort)
{
    if (!ensureDb()) {
        qWarning() << "[Database] setRecorderSettingsDB: DB not open";
        return;
    }

    QSqlQuery q(db);

    // ตรวจสอบว่ามี row อยู่แล้วไหม
    q.prepare("SELECT id FROM Recorder LIMIT 1");

    if (!q.exec()) {
        qWarning() << "[Database] Query failed (SELECT):" << q.lastError().text();
        return;
    }

    if (q.next()) {
        int id = q.value(0).toInt();

        QSqlQuery update(db);
        update.prepare(R"(
            UPDATE Recorder SET
                ALSA_Device = :alsa,
                ClientIP = :ip,
                Frequency = :freq,
                RTSP_Server = :server,
                RTSP_URL = :url,
                RTSP_Port = :port
            WHERE id = :id
        )");
        update.bindValue(":alsa", alsaDevice);
        update.bindValue(":ip", clientIp);
        update.bindValue(":freq", freq);
        update.bindValue(":server", rtspServer);
        update.bindValue(":url", rtspUrl);
        update.bindValue(":port", rtspPort);
        update.bindValue(":id", id);

        if (!update.exec())
            qWarning() << "[Database] Update failed:" << update.lastError().text();

    } else {

        QSqlQuery insert(db);
        insert.prepare(R"(
            INSERT INTO Recorder (
                ALSA_Device,
                ClientIP,
                Frequency,
                RTSP_Server,
                RTSP_URL,
                RTSP_Port
            ) VALUES (
                :alsa,
                :ip,
                :freq,
                :server,
                :url,
                :port
            )
        )");
        insert.bindValue(":alsa", alsaDevice);
        insert.bindValue(":ip", clientIp);
        insert.bindValue(":freq", freq);
        insert.bindValue(":server", rtspServer);
        insert.bindValue(":url", rtspUrl);
        insert.bindValue(":port", rtspPort);

        if (!insert.exec())
            qWarning() << "[Database] Insert failed:" << insert.lastError().text();
    }

    qDebug() << "[Recorder] settings updated";
}

void DatabaseDF::UpdateMode(const QString &mode)
{
    if (!ensureDb()) {
        qWarning() << "[UpdateMode] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare("UPDATE Parameter "
                "SET statusmode = :mode "
                "WHERE id = 1");

    qry.bindValue(":mode", mode);

    if (!qry.exec()) {
        qWarning() << "[UpdateMode] UPDATE Parameter Mode FAILED:"
                   << qry.lastError().text();
        db.rollback();
        return;
    }

    if (!db.commit()) {
        qWarning() << "[UpdateMode] commit FAILED";
        return;
    }
}
void DatabaseDF::UpdateDeviceParameter(const QString &deviceName,const QString &serial)
{
    if (!ensureDb()) {
        qWarning() << "[UpdateDeviceParameter] DB open failed";
        return;
    }

    QSqlQuery q(db);
    q.prepare("UPDATE Parameter "
              "SET deviceName = :d, serialnumber = :s "
              "WHERE id = 1");
    q.bindValue(":d", deviceName);
    q.bindValue(":s", serial);

    if (!q.exec()) {
        qWarning() << "[UpdateDeviceParameter] FAILED:"
                   << q.lastError().text();
        return;
    }

    if (!db.commit()) {
        qWarning() << "[UpdateDeviceParameter] commit FAILED";
        return;
    }
}
void DatabaseDF::GetParameter()
{
    if (!ensureDb()) {
        qWarning() << "[GetParameter] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare("SELECT statusmode, deviceName, serialnumber "
                "FROM Parameter WHERE id = 1");

    if (!qry.exec()) {
        qWarning() << "[GetParameter] SELECT FAILED:"
                   << qry.lastError().text();
        return;
    }

    if (qry.next()) {

        QString mode        = qry.value("statusmode").toString();
        QString deviceName  = qry.value("deviceName").toString();
        QString serial      = qry.value("serialnumber").toString();

        emit parameterReceived(mode, deviceName, serial);
        return;
    }

    qWarning() << "[GetParameter] No rows found";
}

//////////////////////////////////////////////////////////////////// GetrfsocParameter ////////////////////////////////////////////////////////
void DatabaseDF::GetrfsocParameter()
{
    if (!ensureDb()) {
        qWarning() << "[GetParameter] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare(
        "SELECT "
        " setDoaEnable, "
        " spectrumEnabled, "
        " setAdcChannel, "
        " Frequency, "
        " update_en, "
        " TxHz, "
        " TargetOffsetHz, "
        " DoaBwHz, "
        " DoaPowerThresholdDb, "
        " DoaAlgorithm, "
        " uca_radius_m, "
        " rf_agc_target_db, "
        " rf_agc_enabled, "
        " linkstatus, "
        " offset_value, "
        " compass_offset, "
        " maxDoaLineMeters, "
        " IPLocalForRemoteGroup, "
        " setDelayMs, "          // ✅ NEW
        " setDistance "          // ✅ NEW
        "FROM Parameter "
        "WHERE id = 1"
        );

    if (!qry.exec()) {
        qWarning() << "[GetParameter] SELECT FAILED:" << qry.lastError().text();
        return;
    }

    if (!qry.next()) {
        qWarning() << "[GetParameter] No rows found";
        return;
    }

    bool    setDoaEnable        = qry.value("setDoaEnable").toBool();
    bool    spectrumEnabled     = qry.value("spectrumEnabled").toBool();
    int     setAdcChannel       = qry.value("setAdcChannel").toInt();
    int     Frequency           = qry.value("Frequency").toInt();
    int     update_en           = qry.value("update_en").toInt();
    double  TxHz                = qry.value("TxHz").toDouble();
    int     TargetOffsetHz      = qry.value("TargetOffsetHz").toInt();
    int     DoaBwHz             = qry.value("DoaBwHz").toInt();
    double  DoaPowerThresholdDb = qry.value("DoaPowerThresholdDb").toDouble();
    QString DoaAlgorithm        = qry.value("DoaAlgorithm").toString().trimmed();
    double  ucaRadiusM          = qry.value("uca_radius_m").toDouble();
    double  TargetDb            = qry.value("rf_agc_target_db").toDouble();
    bool    rfAgcEnabled        = qry.value("rf_agc_enabled").toBool();
    bool    linkStatus          = qry.value("linkstatus").toInt() == 1;
    double  offsetvalue         = qry.value("offset_value").toDouble();
    double  compassoffset       = qry.value("compass_offset").toDouble();

    int     maxDoaLineMeters      = qry.value("maxDoaLineMeters").toInt();
    QString ipLocalForRemoteGroup = qry.value("IPLocalForRemoteGroup").toString().trimmed();

    // ✅ NEW fields
    int setDelayMs   = qry.value("setDelayMs").toInt();     // ms
    int setDistance  = qry.value("setDistance").toInt();    // meters

    // ✅ clamp กัน DB เก่า/ค่าเพี้ยน
    // if (setDelayMs < 0) setDelayMs = 0;
    // if (setDelayMs > 60000) setDelayMs = 60000;

    // if (setDistance < 0) setDistance = 0;
    // if (setDistance > 200000) setDistance = 200000;

    qDebug() << "[GetParameter]"
             << "DoA=" << setDoaEnable
             << "Spec=" << spectrumEnabled
             << "ADC=" << setAdcChannel
             << "Freq=" << Frequency
             << "TxHz=" << TxHz
             << "BW=" << DoaBwHz
             << "TH=" << DoaPowerThresholdDb
             << "maxDoaLineMeters=" << maxDoaLineMeters
             << "setDelayMs=" << setDelayMs
             << "setDistance=" << setDistance
             << "IPLocalForRemoteGroup=" << ipLocalForRemoteGroup;

    // ✅ ส่งค่าเดิม (ของคุณ)
    emit Getrfsocparameter(setDoaEnable, spectrumEnabled, setAdcChannel, Frequency, update_en,
                           TxHz, TargetOffsetHz, DoaBwHz, DoaPowerThresholdDb, DoaAlgorithm,
                           ucaRadiusM, TargetDb, rfAgcEnabled, linkStatus,
                           offsetvalue, compassoffset, maxDoaLineMeters, ipLocalForRemoteGroup,setDelayMs,setDistance);
}

void DatabaseDF::GetIPDFServerFromDB()
{
    if (!ensureDb()) {
        qWarning() << "[GetIPServer] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    qry.prepare(
        "SELECT "
        " ipdfserver "
        "FROM Parameter "
        "WHERE id = 1"
        );

    if (!qry.exec()) {
        qWarning() << "[GetIPServer] SELECT FAILED:" << qry.lastError().text();
        return;
    }

    if (!qry.next()) {
        qWarning() << "[GetIPServer] No rows found";
        return;
    }
    QString ip        = qry.value("ipdfserver").toString();
    emit GetIPDFServer(ip);

}
void DatabaseDF::UpdateParameterField(const QString &field, const QVariant &value)
{
    static const QSet<QString> allowed = {
        "setDoaEnable",
        "spectrumEnabled",
        "setAdcChannel",
        "Frequency",
        "update_en",
        "TxHz",
        "TargetOffsetHz",
        "DoaBwHz",
        "DoaPowerThresholdDb",
        "DoaAlgorithm",
        "uca_radius_m",
        "rf_agc_target_db",
        "rf_agc_enabled" ,
        "linkstatus" ,
        "ipdfserver" ,
        "compass_offset" ,        // ✅ เพิ่ม
        "maxDoaLineMeters" ,
        "IPLocalForRemoteGroup" ,
        "setDelayMs" ,
        "setDistance"
    };

    if (!allowed.contains(field)) {
        qWarning() << "[UpdateParameterField] rejected field =" << field;
        return;
    }

    if (!ensureDb()) {
        qWarning() << "[UpdateParameterField] DB open failed";
        return;
    }

    QSqlQuery qry(db);
    const QString sql = QString("UPDATE Parameter SET `%1` = :v WHERE id = 1").arg(field);

    qry.prepare(sql);
    qry.bindValue(":v", value);

    if (!qry.exec()) {
        qWarning() << "[UpdateParameterField] UPDATE FAILED:"
                   << qry.lastError().text()
                   << "sql=" << sql
                   << "value=" << value;
        return;
    }

    const qint64 rows = qry.numRowsAffected();
    if (rows <= 0) {
        qWarning() << "[UpdateParameterField] NO ROW UPDATED"
                   << "field=" << field
                   << "value=" << value
                   << "(check id=1 exists or same value)";
    } else {
        qDebug() << "[UpdateParameterField] OK"
                 << "field=" << field
                 << "value=" << value;
    }
}



///////////////////////////////////////////////////REMOT GROUP AND SAVE DATABASES //////////////////////////////////////////////////////////////
/// \brief Database::saveDevicesAndGroupsFromConnectGroupSingle
/// \param obj
/// \param localIp
void DatabaseDF::saveDevicesAndGroupsFromConnectGroupSingle(const QJsonObject &obj,const QString &localIp)
{
    if (!ensureDb()) {
        qWarning() << "[saveDevicesAndGroupsFromConnectGroupSingle] DB open failed";
        return;
    }

    int groupId           = obj.value("groupId").toInt();
    QString groupName     = obj.value("groupName").toString();
    QString groupUniqueId = obj.value("groupUniqueId").toString();
    QJsonArray devices    = obj.value("devices").toArray();

    qDebug() << "[saveDevicesAndGroupsFromConnectGroupSingle]"
             << "groupId=" << groupId
             << "groupName=" << groupName
             << "groupUniqueId=" << groupUniqueId
             << "devices.count=" << devices.size();

    if (!db.transaction()) {
        qWarning() << "[saveDevicesAndGroupsFromConnectGroupSingle] begin transaction failed";
        return;
    }

    // ===== เก็บ UID ของอุปกรณ์ที่ยังอยู่ใน JSON ใหม่ =====
    QSet<QString> keepUids;

    // =======================================================================
    // 1) UPSERT Devices + DeviceGroups
    // =======================================================================
    for (const QJsonValue &v : devices) {
        QJsonObject dv = v.toObject();

        QString ip          = dv.value("ip").toString();
        int     port        = dv.value("port").toInt();
        QString name        = dv.value("name").toString();
        QString deviceUid   = dv.value("deviceUniqueId").toString();
        QString uidGroup    = dv.value("uniqueIdInGroup").toString();
        bool    isController = dv.value("isController").toBool();

        if (!localIp.isEmpty() && ip == localIp)
            continue;

        if (ip.isEmpty() || deviceUid.isEmpty())
            continue;

        if (uidGroup.isEmpty())
            uidGroup = groupUniqueId;

        keepUids.insert(deviceUid);

        // ------ UPSERT DeviceList ------
        {
            QSqlQuery check(db);
            check.prepare("SELECT id FROM DeviceList WHERE deviceUniqueId = :uid");
            check.bindValue(":uid", deviceUid);
            check.exec();

            if (check.next()) {
                int did = check.value(0).toInt();
                QSqlQuery upd(db);
                upd.prepare(
                    "UPDATE DeviceList SET Name=:name, IPAddress=:ip, Port=:port "
                    "WHERE id=:id"
                    );
                upd.bindValue(":name", name);
                upd.bindValue(":ip", ip);
                upd.bindValue(":port", port > 0 ? port : 8000);
                upd.bindValue(":id", did);
                upd.exec();
            } else {
                QSqlQuery ins(db);
                ins.prepare(
                    "INSERT INTO DeviceList (Name, IPAddress, Port, deviceUniqueId) "
                    "VALUES (:name,:ip,:port,:uid)"
                    );
                ins.bindValue(":name", name);
                ins.bindValue(":ip", ip);
                ins.bindValue(":port", port > 0 ? port : 8000);
                ins.bindValue(":uid", deviceUid);
                ins.exec();
            }
        }

        // ------ UPSERT DeviceGroups ------
        {
            QSqlQuery check2(db);
            check2.prepare(
                "SELECT id FROM DeviceGroups "
                "WHERE deviceUniqueId=:uid AND uniqueIdInGroup=:gid"
                );
            check2.bindValue(":uid", deviceUid);
            check2.bindValue(":gid", uidGroup);
            check2.exec();

            if (check2.next()) {
                int dgid = check2.value(0).toInt();
                QSqlQuery upd2(db);
                upd2.prepare(
                    "UPDATE DeviceGroups "
                    "SET GroupsName=:gname, GroupID=:gid "
                    "WHERE id=:id"
                    );
                upd2.bindValue(":gname", groupName);
                upd2.bindValue(":gid", groupId);
                upd2.bindValue(":id", dgid);
                upd2.exec();
            } else {
                QSqlQuery ins2(db);
                ins2.prepare(
                    "INSERT INTO DeviceGroups "
                    "(GroupsName, deviceUniqueId, GroupID, uniqueIdInGroup) "
                    "VALUES (:gname,:uid,:gid,:ugid)"
                    );
                ins2.bindValue(":gname", groupName);
                ins2.bindValue(":uid", deviceUid);
                ins2.bindValue(":gid", groupId);
                ins2.bindValue(":ugid", uidGroup);
                ins2.exec();
            }
        }
    }

    // =======================================================================
    // 2) ลบเฉพาะอุปกรณ์ที่ไม่อยู่ใน JSON ใหม่สำหรับ group นี้
    // =======================================================================
    {
        QSqlQuery qSel(db);
        qSel.prepare(
            "SELECT deviceUniqueId FROM DeviceGroups WHERE uniqueIdInGroup = :ugid"
            );
        qSel.bindValue(":ugid", groupUniqueId);
        qSel.exec();

        QList<QString> removeUids;

        while (qSel.next()) {
            QString uidDb = qSel.value(0).toString();

            if (!keepUids.contains(uidDb))
                removeUids.append(uidDb);
        }

        for (const QString &uid : removeUids) {

            // ลบออกจาก DeviceGroups ใน group นี้ก่อน
            {
                QSqlQuery qDel(db);
                qDel.prepare(
                    "DELETE FROM DeviceGroups "
                    "WHERE deviceUniqueId=:uid AND uniqueIdInGroup=:ugid"
                    );
                qDel.bindValue(":uid", uid);
                qDel.bindValue(":ugid", groupUniqueId);
                qDel.exec();
            }

            // ตรวจว่า UID ยังอยู่ใน group อื่นหรือไม่
            QSqlQuery qCheck(db);
            qCheck.prepare(
                "SELECT COUNT(*) FROM DeviceGroups WHERE deviceUniqueId=:uid"
                );
            qCheck.bindValue(":uid", uid);
            qCheck.exec();

            bool stillUsed = false;
            if (qCheck.next())
                stillUsed = (qCheck.value(0).toInt() > 0);

            // ไม่อยู่ใน group อื่นแล้ว → ลบ DeviceList
            if (!stillUsed) {
                QSqlQuery qDel2(db);
                qDel2.prepare(
                    "DELETE FROM DeviceList WHERE deviceUniqueId=:uid"
                    );
                qDel2.bindValue(":uid", uid);
                qDel2.exec();
            }
        }
    }

    // =======================================================================

    if (!db.commit()) {
        qWarning() << "[saveDevicesAndGroupsFromConnectGroupSingle] commit failed";
        return;
    }

    qDebug() << "[saveDevicesAndGroupsFromConnectGroupSingle] COMMIT OK";

    getSideRemote();
    getRemoteGroups();
    getGroupsInGroupSetting();
    // getActiveClientInDatabase(groupUniqueId);
    emit setupServerClientForDevices(groupUniqueId);
}

static bool columnExists(QSqlDatabase &db, const QString &table, const QString &column)
{
    QSqlQuery q(db);
    q.prepare(QString("SHOW COLUMNS FROM %1 LIKE ?").arg(table));
    q.addBindValue(column);
    if (!q.exec()) {
        qWarning() << "[columnExists] Failed:" << q.lastError().text();
        return false;
    }
    return q.next();
}
void DatabaseDF::ensureParameterHasMaxDoaLineMeters()
{
    if (!ensureDb()) {
        qWarning() << "[ensureParameterHasMaxDoaLineMeters] DB open failed";
        return;
    }

    // defaults (ปรับได้ตามที่คุณใช้จริง)
    const int DEFAULT_MAX_DOA_LINE_METERS = 15000; // 15 km
    const int DEFAULT_SET_DELAY_MS        = 2000;  // 2 s
    const int DEFAULT_SET_DISTANCE_M      = 1000;  // 1000 m

    bool needAlter = false;

    const bool hasMaxDoaLineMeters = columnExists(db, "Parameter", "maxDoaLineMeters");
    const bool hasSetDelayMs       = columnExists(db, "Parameter", "setDelayMs");
    const bool hasSetDistance      = columnExists(db, "Parameter", "setDistance");

    if (!hasMaxDoaLineMeters) needAlter = true;
    if (!hasSetDelayMs)       needAlter = true;
    if (!hasSetDistance)      needAlter = true;

    if (!needAlter) {
        qDebug() << "[ensureParameterHasMaxDoaLineMeters] Columns already exist:"
                 << "maxDoaLineMeters=" << hasMaxDoaLineMeters
                 << "setDelayMs=" << hasSetDelayMs
                 << "setDistance=" << hasSetDistance;
        return;
    }

    // ทำให้ปลอดภัย: ทำเป็น transaction
    if (!db.transaction()) {
        qWarning() << "[ensureParameterHasMaxDoaLineMeters] transaction() failed:"
                   << db.lastError().text();
        // ไม่ return ก็ได้ แต่โดยทั่วไปควรหยุด
        return;
    }

    auto alterAddColumn = [&](const QString &sql, const char *tag) -> bool {
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            qWarning() << tag << "ALTER failed:" << q.lastError().text()
            << "SQL=" << sql;
            return false;
        }
        qDebug() << tag << "OK";
        return true;
    };

    bool ok = true;

    // 1) Add columns if missing
    if (!hasMaxDoaLineMeters) {
        ok = ok && alterAddColumn(
                 QString("ALTER TABLE Parameter ADD COLUMN maxDoaLineMeters INT NOT NULL DEFAULT %1")
                     .arg(DEFAULT_MAX_DOA_LINE_METERS),
                 "[ensureParameter] add maxDoaLineMeters");
    }

    if (!hasSetDelayMs) {
        ok = ok && alterAddColumn(
                 QString("ALTER TABLE Parameter ADD COLUMN setDelayMs INT NOT NULL DEFAULT %1")
                     .arg(DEFAULT_SET_DELAY_MS),
                 "[ensureParameter] add setDelayMs");
    }

    if (!hasSetDistance) {
        ok = ok && alterAddColumn(
                 QString("ALTER TABLE Parameter ADD COLUMN setDistance INT NOT NULL DEFAULT %1")
                     .arg(DEFAULT_SET_DISTANCE_M),
                 "[ensureParameter] add setDistance");
    }

    // ถ้า ALTER fail -> rollback
    if (!ok) {
        db.rollback();
        qWarning() << "[ensureParameterHasMaxDoaLineMeters] rollback due to ALTER failure";
        return;
    }

    // 2) Optional: normalize existing rows
    // maxDoaLineMeters
    if (columnExists(db, "Parameter", "maxDoaLineMeters")) {
        QSqlQuery upd(db);
        const QString sql =
            QString("UPDATE Parameter SET maxDoaLineMeters=%1 WHERE maxDoaLineMeters IS NULL OR maxDoaLineMeters<=0")
                .arg(DEFAULT_MAX_DOA_LINE_METERS);
        if (!upd.exec(sql)) {
            qWarning() << "[ensureParameter] UPDATE maxDoaLineMeters failed:" << upd.lastError().text();
        }
    }

    // setDelayMs
    if (columnExists(db, "Parameter", "setDelayMs")) {
        QSqlQuery upd(db);
        const QString sql =
            QString("UPDATE Parameter SET setDelayMs=%1 WHERE setDelayMs IS NULL OR setDelayMs<0")
                .arg(DEFAULT_SET_DELAY_MS);
        if (!upd.exec(sql)) {
            qWarning() << "[ensureParameter] UPDATE setDelayMs failed:" << upd.lastError().text();
        }
    }

    // setDistance
    if (columnExists(db, "Parameter", "setDistance")) {
        QSqlQuery upd(db);
        const QString sql =
            QString("UPDATE Parameter SET setDistance=%1 WHERE setDistance IS NULL OR setDistance<=0")
                .arg(DEFAULT_SET_DISTANCE_M);
        if (!upd.exec(sql)) {
            qWarning() << "[ensureParameter] UPDATE setDistance failed:" << upd.lastError().text();
        }
    }

    if (!db.commit()) {
        qWarning() << "[ensureParameterHasMaxDoaLineMeters] commit() failed:" << db.lastError().text();
        db.rollback();
        return;
    }

    qDebug() << "[ensureParameterHasMaxDoaLineMeters] ensure OK:"
             << "maxDoaLineMeters" << (hasMaxDoaLineMeters ? "exists" : "added")
             << "setDelayMs"       << (hasSetDelayMs ? "exists" : "added")
             << "setDistance"      << (hasSetDistance ? "exists" : "added");
}


void DatabaseDF::ensureParameterIPLocalForRemoteGroup()
{
    if (!ensureDb()) {
        qWarning() << "[ensureParameterIPLocalForRemoteGroup] DB open failed";
        return;
    }

    // ตรวจว่าคอลัมน์มีแล้วหรือยัง
    if (columnExists(db, "Parameter", "IPLocalForRemoteGroup")) {
        qDebug() << "[ensureParameterIPLocalForRemoteGroup] Column already exists";
        return;
    }

    // เพิ่มคอลัมน์เป็น VARCHAR เก็บ IP
    QSqlQuery alter(db);
    const QString alterSql =
        "ALTER TABLE Parameter "
        "ADD COLUMN IPLocalForRemoteGroup VARCHAR(64) NOT NULL DEFAULT '10.10.0.20'";

    if (!alter.exec(alterSql)) {
        qWarning() << "[ensureParameterIPLocalForRemoteGroup] ALTER failed:"
                   << alter.lastError().text();
        return;
    }

    qDebug() << "[ensureParameterIPLocalForRemoteGroup] Added column IPLocalForRemoteGroup";

    // ตั้งค่า default ให้ row เก่าที่เป็น NULL หรือว่าง
    QSqlQuery upd(db);
    const QString updSql =
        "UPDATE Parameter "
        "SET IPLocalForRemoteGroup = '10.10.0.20' "
        "WHERE IPLocalForRemoteGroup IS NULL OR IPLocalForRemoteGroup = ''";

    if (!upd.exec(updSql)) {
        qWarning() << "[ensureParameterIPLocalForRemoteGroup] UPDATE default failed:"
                   << upd.lastError().text();
    }
}
