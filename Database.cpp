#include "Database.h"

Database::Database(QString dbName, QString user, QString password, QString host, QObject *parent) : QObject(parent) {
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(host);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);
    towerAndDistance = new TowerAndDistance;
    FTP_Param_ = new FTP_Param;
    SetupEquipment = new SetupParameterEquipment;
    if (!ensureEventAlarmHistoryTimeColumn()) {
        qWarning() << "[insertEventAlarmHistory] skip: cannot ensure TIME(6) column";
        return;
    }
}

Database::~Database() {}
void Database::restartMysql() {
    system("systemctl stop mysqld");
    system("systemctl start mysqld");

    qDebug() << "Restart MySQL";
}

bool Database::database_createConnection() {
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //        emit mysqlError();
        restartMysql();
        return false;
    }
    db.close();
    qDebug() << "Database connected";

    //    upgradeDatabase();
    return true;
}

void Database::updateSettingNetwork(QString ip,
                                    QString gateway,
                                    QString snmp,
                                    QString ntp,
                                    bool PlcDoError,
                                    bool PlcDiError,
                                    bool ModuleHispeedPhaseAError,
                                    bool ModuleHispeedPhaseBError,
                                    bool ModuleHispeedPhaseCError,
                                    bool modbusPhaseAError,
                                    bool modbusPhaseBError,
                                    bool modbusPhaseCError,
                                    bool GpsModuleFail,
                                    bool SystemInital,
                                    bool CommunicationError,
                                    bool RelayStartEvent,
                                    bool surgeStartEvent,
                                    bool PeriodicStartEvent,
                                    bool ManualTestEvent,
                                    bool LFLFail,
                                    bool LFLOperate)
{
    qDebug() << "updateSettingNetwork update database";

    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        restartMysql();
        return;
    }

    QSqlQuery qry(db);
    qry.prepare(R"(
        UPDATE SettingNetwork
        SET ipaddress                = :ipaddress,
            ipgateway                = :ipgateway,
            snmpserver               = :snmpserver,
            sychonizationserver      = :sychonizationserver,
            PlcDoError               = :PlcDoError,
            PlcDiError               = :PlcDiError,
            ModuleHispeedPhaseAError = :ModuleHispeedPhaseAError,
            ModuleHispeedPhaseBError = :ModuleHispeedPhaseBError,
            ModuleHispeedPhaseCError = :ModuleHispeedPhaseCError,
            modbusPhaseAError        = :modbusPhaseAError,
            modbusPhaseBError        = :modbusPhaseBError,
            modbusPhaseCError        = :modbusPhaseCError,
            GpsModuleFail            = :GpsModuleFail,
            SystemInital             = :SystemInital,
            CommunicationError       = :CommunicationError,
            RelayStartEvent          = :RelayStartEvent,
            surgeStartEvent          = :surgeStartEvent,
            PeriodicStartEvent       = :PeriodicStartEvent,
            ManualTestEvent          = :ManualTestEvent,
            LFLFail                  = :LFLFail,
            LFLOperate               = :LFLOperate
        WHERE id = 1
    )");

    qry.bindValue(":ipaddress", ip);
    qry.bindValue(":ipgateway", gateway);
    qry.bindValue(":snmpserver", snmp);
    qry.bindValue(":sychonizationserver", ntp);

    qry.bindValue(":PlcDoError", PlcDoError ? 1 : 0);
    qry.bindValue(":PlcDiError", PlcDiError ? 1 : 0);
    qry.bindValue(":ModuleHispeedPhaseAError", ModuleHispeedPhaseAError ? 1 : 0);
    qry.bindValue(":ModuleHispeedPhaseBError", ModuleHispeedPhaseBError ? 1 : 0);
    qry.bindValue(":ModuleHispeedPhaseCError", ModuleHispeedPhaseCError ? 1 : 0);
    qry.bindValue(":modbusPhaseAError", modbusPhaseAError ? 1 : 0);
    qry.bindValue(":modbusPhaseBError", modbusPhaseBError ? 1 : 0);
    qry.bindValue(":modbusPhaseCError", modbusPhaseCError ? 1 : 0);
    qry.bindValue(":GpsModuleFail", GpsModuleFail ? 1 : 0);
    qry.bindValue(":SystemInital", SystemInital ? 1 : 0);
    qry.bindValue(":CommunicationError", CommunicationError ? 1 : 0);
    qry.bindValue(":RelayStartEvent", RelayStartEvent ? 1 : 0);
    qry.bindValue(":surgeStartEvent", surgeStartEvent ? 1 : 0);
    qry.bindValue(":PeriodicStartEvent", PeriodicStartEvent ? 1 : 0);
    qry.bindValue(":ManualTestEvent", ManualTestEvent ? 1 : 0);
    qry.bindValue(":LFLFail", LFLFail ? 1 : 0);
    qry.bindValue(":LFLOperate", LFLOperate ? 1 : 0);

    if (!qry.exec()) {
        qDebug() << "SQL Error:" << qry.lastError().text();
        qDebug() << "Executed Query:" << qry.lastQuery();
    }

    db.close();
}

// Fetch settings from database
void Database::getSettingNetwork() {
    QString ip;
    QString gateway;
    QString snmp;
    QString ntp;
    bool PlcDoError;
    bool PlcDiError;
    bool ModuleHispeedPhaseAError;
    bool ModuleHispeedPhaseBError;
    bool ModuleHispeedPhaseCError;
    bool modbusPhaseAError;
    bool modbusPhaseBError;
    bool modbusPhaseCError;
    bool GpsModuleFail;
    bool SystemInital;
    bool CommunicationError;
    bool RelayStartEvent;
    bool surgeStartEvent;
    bool ReriodicStartEvent;
    bool ManualTestEvent;
    bool LFLFail;
    bool LFLOperate;
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        restartMysql();
        return;
    }

    QSqlQuery qry("SELECT * FROM SettingNetwork WHERE id=1;");
    if (qry.next()) {
        ip = qry.value("ipaddress").toString();
        gateway = qry.value("ipgateway").toString();
        snmp = qry.value("snmpserver").toString();
        ntp = qry.value("sychonizationserver").toString();
        PlcDoError = qry.value("PlcDoError").toBool();
        PlcDiError = qry.value("PlcDiError").toBool();
        ModuleHispeedPhaseAError = qry.value("ModuleHispeedPhaseAError").toBool();
        ModuleHispeedPhaseBError = qry.value("ModuleHispeedPhaseBError").toBool();
        ModuleHispeedPhaseCError = qry.value("ModuleHispeedPhaseCError").toBool();
        modbusPhaseAError = qry.value("modbusPhaseAError").toBool();
        modbusPhaseBError = qry.value("modbusPhaseBError").toBool();
        modbusPhaseCError = qry.value("modbusPhaseCError").toBool();
        GpsModuleFail = qry.value("GpsModuleFail").toBool();
        SystemInital = qry.value("SystemInital").toBool();
        CommunicationError = qry.value("CommunicationError").toBool();
        RelayStartEvent = qry.value("RelayStartEvent").toBool();
        surgeStartEvent = qry.value("surgeStartEvent").toBool();
        ReriodicStartEvent = qry.value("PeriodicStartEvent").toBool();
        ManualTestEvent = qry.value("ManualTestEvent").toBool();
        LFLFail = qry.value("LFLFail").toBool();
        LFLOperate = qry.value("LFLOperate").toBool();

        qDebug() << "Fetched settings:";
        qDebug() << "IP:" << ip << "Gateway:" << gateway << "SNMP:" << snmp << "NTP:" << ntp;
        qDebug() << "PLC Errors:" << PlcDoError << PlcDiError;
        qDebug() << "Module Errors:" << ModuleHispeedPhaseAError << ModuleHispeedPhaseBError << ModuleHispeedPhaseCError;
        qDebug() << "Modbus Errors:" << modbusPhaseAError << modbusPhaseBError << modbusPhaseCError;
        qDebug() << "GPS Fail:" << GpsModuleFail << "System Init:" << SystemInital;
        qDebug() << "Comm Error:" << CommunicationError << "Relay Event:" << RelayStartEvent;
        qDebug() << "Surge Event:" << surgeStartEvent << "Periodic Event:" << ReriodicStartEvent;
        qDebug() << "Manual Test Event:" << ManualTestEvent << "LFL Fail:" << LFLFail << "LFL Operate:" << LFLOperate;
    } else {
        qDebug() << "No settings found in the database.";
    }
    db.close();
    emit getSettingNetworks(ip, gateway, snmp, ntp, PlcDoError, PlcDiError, ModuleHispeedPhaseAError, ModuleHispeedPhaseBError, ModuleHispeedPhaseCError, modbusPhaseAError, modbusPhaseBError, modbusPhaseCError, GpsModuleFail, SystemInital, CommunicationError, RelayStartEvent, surgeStartEvent, ReriodicStartEvent, ManualTestEvent, LFLFail, LFLOperate);
}

bool Database::updateSnmpipserver(QString ip) {
    QString query = QString("UPDATE SettingNetwork SET snmpserver = '%1'").arg(ip);
    //        qDebug() << query;
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //            emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
        printf("***********************SQL Error*****************\n%s\n", query.toStdString().c_str());
    }
    db.close();
    return true;
}
bool Database::updateParameterEquipment(QString SubstationName, int Voltage, QString TransmissionLineName, QString Distance, QString IPaddress, QString Brand, QString Model, QString SerialNo, QString ContractNumber, QString Date, QString LFLSerialNo) {
    //        UPDATE EquipmentAndInstallation SET SubstationName="สถานไฟฟาแรงสง สระบร 2",Voltage=1000,TransmissionLineName="A",Distance=1000,IPaddress='192.168.10.35',Brand='ifz',Model='v1',SerialNo='ifz2024v1',ContractNumber='526452854',Date='2025-01-23',LFLSerialNo="2";
    QString query = QString(
                        "UPDATE EquipmentAndInstallation SET SubstationName='%1',Voltage=%2,TransmissionLineName='%3',Distance='%4',IPaddress='%5',Brand='%6',"
                        "Model='%7',SerialNo='%8',ContractNumber='%9',Date='%10',LFLSerialNo='%11'")
                        .arg(SubstationName)
                        .arg(Voltage)
                        .arg(TransmissionLineName)
                        .arg(Distance)
                        .arg(IPaddress)
                        .arg(Brand)
                        .arg(Model)
                        .arg(SerialNo)
                        .arg(ContractNumber)
                        .arg(Date)
                        .arg(LFLSerialNo);
    //        qDebug() << query;
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //            emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
        printf("***********************SQL Error*****************\n%s\n", query.toStdString().c_str());
    }
    db.close();
    return true;
}
bool Database::updateEmail_Config(QString senderEmail, QString senderName, QString password, QString recipientEmail, QString recipientName, QString Smtpserver, int SmtpPort) {
    //         qDebug() << "updateEmail_Config[8]" <<"senderEmail" <<senderEmail << "NamesenderEmail" << senderName << "Password" << password << "recipientEmail" << recipientEmail
    //                     << "recipientName" <<  recipientName << "SmtpServer" << Smtpserver << "SmtpPort" << SmtpPort;

    QString query = QString("UPDATE EmailSenderDatabase SET senderEmail='%1',senderName='%2',password='%3',recipientEmail='%4',recipientName='%5',smtpServer='%6',smtpPort=%7").arg(senderEmail).arg(senderName).arg(password).arg(recipientEmail).arg(recipientName).arg(Smtpserver).arg(SmtpPort);
    //        qDebug() << query;
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //            emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
        printf("***********************SQL Error*****************\n%s\n", query.toStdString().c_str());
    }
    db.close();
    return true;
}
bool Database::updateSnmpSelectionState(bool plcDO, bool plcDI, bool HiSpeedPhaseA, bool HiSpeedPhaseB, bool HiSpeedPhaseC, bool modbusPhaseA, bool modbusPhaseB, bool modbusPhaseC, bool gpsModule, bool systeminit, bool communication, bool relayStart, bool surgeStart, bool periodic, bool manualTest, bool lflFail, bool lflOperate) {
    QString query = QString(
                        "UPDATE SettingNetwork SET PlcDoError=%1, PlcDiError=%2, ModuleHispeedPhaseAError=%3, ModuleHispeedPhaseBError=%4, ModuleHispeedPhaseCError=%5, modbusPhaseAError=%6,"
                        "modbusPhaseBError=%7,modbusPhaseCError=%8,GpsModuleFail=%9, SystemInital =%10, CommunicationError=%11 , RelayStartEvent=%12,surgeStartEvent=%13,PeriodicStartEvent=%14,"
                        "ManualTestEvent=%15,LFLFail=%16,LFLOperate=%17")
                        .arg(plcDO)
                        .arg(plcDI)
                        .arg(HiSpeedPhaseA)
                        .arg(HiSpeedPhaseB)
                        .arg(HiSpeedPhaseC)
                        .arg(modbusPhaseA)
                        .arg(modbusPhaseB)
                        .arg(modbusPhaseC)
                        .arg(gpsModule)
                        .arg(systeminit)
                        .arg(communication)
                        .arg(relayStart)
                        .arg(surgeStart)
                        .arg(periodic)
                        .arg(manualTest)
                        .arg(lflFail)
                        .arg(lflOperate);
    //        qDebug() << query;
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //            emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
        printf("***********************SQL Error*****************\n%s\n", query.toStdString().c_str());
    }
    db.close();
    return true;
}

bool Database::insertIntoSnmpTrepDataLogger(const QString &alertTrap, const QString &alertState) {
    QString query = QString(
                        "INSERT INTO snmptrepdatalogger (time, Alert_trape, Alert_state) "
                        "VALUES (CURRENT_TIMESTAMP, '%1', '%2')")
                        .arg(alertTrap)
                        .arg(alertState);
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //            emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
        printf("***********************SQL Error*****************\n%s\n", query.toStdString().c_str());
    }
    db.close();
    return true;
}
bool Database::getParameterEquipment() {
    QString SubstationName = "";
    int Voltage = 0;
    QString TransmissionLineName = "";
    QString Distance = 0;
    QString IPaddress = "";
    QString Brand = "";
    QString Model = "";
    QString SerialNo = "";
    QString ContractNumber = "";
    QString Date = "";
    QString LFLSerialNo = "";
    QString query = QString(
        "SELECT SubstationName,Voltage,TransmissionLineName,Distance,IPaddress,Brand,Model,SerialNo,ContractNumber,Date,"
        "LFLSerialNo FROM EquipmentAndInstallation ORDER BY id ASC LIMIT 8;");
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //        emit mysqlError();
        restartMysql();
        return false;
    }

    QSqlQuery checkColumnQuery;
    QString checkQuery = R"(
        SELECT DATA_TYPE, CHARACTER_MAXIMUM_LENGTH
        FROM INFORMATION_SCHEMA.COLUMNS
        WHERE table_name = 'EquipmentAndInstallation'
        AND COLUMN_NAME = 'Distance';
    )";
    if (!checkColumnQuery.exec(checkQuery)) {
        qDebug() << "Failed to query column info:" << checkColumnQuery.lastError();
    } else if (checkColumnQuery.next()) {
        QString dataType = checkColumnQuery.value(0).toString().toUpper();
        QVariant maxLengthVar = checkColumnQuery.value(1);
        int maxLength = maxLengthVar.isNull() ? -1 : maxLengthVar.toInt();

        qDebug() << "Distance column type:" << dataType << "Length:" << maxLength;

        if (dataType != "VARCHAR" || maxLength != 255) {
            QSqlQuery alterQuery;
            QString alter = "ALTER TABLE EquipmentAndInstallation MODIFY Distance VARCHAR(255);";
            if (!alterQuery.exec(alter)) {
                qDebug() << "Failed to alter column Distance:" << alterQuery.lastError();
            } else {
                qDebug() << "Column Distance altered to VARCHAR(255)";
            }
        }
    }

    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
    }

    else {
        while (qry.next()) {
            SubstationName = qry.value(0).toString();
            Voltage = qry.value(1).toInt();
            TransmissionLineName = qry.value(2).toString();
            Distance = qry.value(3).toString();
            IPaddress = qry.value(4).toString();
            Brand = qry.value(5).toString();
            Model = qry.value(6).toString();
            SerialNo = qry.value(7).toString();
            ContractNumber = qry.value(8).toString();
            Date = qry.value(9).toString();
            LFLSerialNo = qry.value(10).toString();

            qDebug() << "Sub SetupEquipment[get from data base]" << "SubstationName" << SubstationName << "Voltage" << Voltage << "TransmissionLineName" << TransmissionLineName << "Distance" << Distance << "IPaddress" << IPaddress << "Brand " << Brand << "Model" << Model << "SerialNo" << SerialNo << "ContractNumber" << ContractNumber << "Date" << Date << "LFLSerialNo" << LFLSerialNo;
            SetupEquipment->SubstationName = SubstationName;
            SetupEquipment->Voltage = Voltage;
            SetupEquipment->TransmissionLineName = TransmissionLineName;
            SetupEquipment->Distance = Distance;
            SetupEquipment->IPaddress = IPaddress;
            SetupEquipment->Brand = Brand;
            SetupEquipment->Model = Model;
            SetupEquipment->SerialNo = SerialNo;
            SetupEquipment->ContractNumber = ContractNumber;
            SetupEquipment->Date = Date;
            SetupEquipment->LFLSerialNo = LFLSerialNo;
            // qWarning() << "getParameterEquipment SetupEquipment->SubstationName" << SetupEquipment->SubstationName
            //            << " SetupEquipment->Voltage" << SetupEquipment->Voltage
            //            << " SetupEquipment->TransmissionLineName" << SetupEquipment->TransmissionLineName
            //            << " SetupEquipment->Distance" << SetupEquipment->Distance
            //            << " SetupEquipment->IPaddress" << SetupEquipment->IPaddress
            //            << " SetupEquipment->Brand" << SetupEquipment->Brand
            //            << " SetupEquipment->Model" << SetupEquipment->Model
            //            << " SetupEquipment->SerialNo" << SetupEquipment->SerialNo
            //            << " SetupEquipment->ContractNumber" << SetupEquipment->ContractNumber
            //            << " SetupEquipment->Date" << SetupEquipment->Date
            //            << " SetupEquipment->LFLSerialNo" << SetupEquipment->LFLSerialNo;
            emit newdataParameterEquipment(SubstationName, Voltage, TransmissionLineName, Distance, IPaddress, Brand, Model, SerialNo, ContractNumber, Date, LFLSerialNo);
        }
    }
    db.close();
    return true;
}
bool Database::getSmtpParameter() {
    QString senderEmail = "";
    QString NamesenderEmail = "";
    QString Password = "";
    QString recipientEmail = "";
    QString recipientName = "";
    QString SmtpServer = "";
    int SmtpPort = 0;
    QString query = QString(
        "SELECT senderEmail, senderName, Password, recipientEmail, recipientName, smtpServer, smtpPort"
        " FROM EmailSenderDatabase ORDER BY id ASC LIMIT 8;");
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //        emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
    }

    else {
        while (qry.next()) {
            senderEmail = qry.value(0).toString();
            NamesenderEmail = qry.value(1).toString();
            Password = qry.value(2).toString();
            recipientEmail = qry.value(3).toString();
            recipientName = qry.value(4).toString();
            SmtpServer = qry.value(5).toString();
            SmtpPort = qry.value(6).toInt();

            qDebug() << "senderEmail" << senderEmail << "NamesenderEmail" << NamesenderEmail << "Password" << Password << "recipientEmail" << recipientEmail << "recipientName" << recipientName << "SmtpServer" << SmtpServer << "SmtpPort" << SmtpPort;
            emit newdataSmtpParameter(senderEmail, NamesenderEmail, Password, recipientEmail, recipientName, SmtpServer, SmtpPort);
        }
    }
    db.close();
    return true;
}

bool Database::getdataMysql() {
    QString serverip = " ";
    bool plcDO = 0;
    bool plcDI = 0;
    bool HiSpeedA = 0;
    bool HiSpeedB = 0;
    bool HiSpeedC = 0;
    bool modbusA = 0;
    bool modbusB = 0;
    bool modbusC = 0;
    bool Gps = 0;
    bool systeminit = 0;
    bool communication = 0;
    bool relayStart = 0;
    bool surge = 0;
    bool periodic = 0;
    bool manualTest = 0;
    bool lflFail = 0;
    bool lflOperate = 0;

    QString query = QString(
        "SELECT snmpserver, PlcDoError, PlcDiError, ModuleHispeedPhaseAError, ModuleHispeedPhaseBError, ModuleHispeedPhaseCError, modbusPhaseAError,"
        "modbusPhaseBError,modbusPhaseCError,GpsModuleFail,SystemInital, CommunicationError, RelayStartEvent, surgeStartEvent,PeriodicStartEvent"
        " FROM SettingNetwork ORDER BY id ASC LIMIT 8;");
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //        emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
    }

    else {
        while (qry.next()) {
            serverip = qry.value(0).toString();
            plcDO = qry.value(1).toInt();
            plcDI = qry.value(2).toInt();
            HiSpeedA = qry.value(3).toInt();
            HiSpeedB = qry.value(4).toInt();
            HiSpeedC = qry.value(5).toInt();
            modbusA = qry.value(6).toInt();
            modbusB = qry.value(7).toInt();
            modbusC = qry.value(8).toInt();
            Gps = qry.value(9).toInt();
            systeminit = qry.value(10).toInt();
            communication = qry.value(11).toInt();
            relayStart = qry.value(12).toInt();
            surge = qry.value(13).toInt();
            periodic = qry.value(14).toInt();
            manualTest = qry.value(15).toInt();
            lflFail = qry.value(16).toInt();
            lflOperate = qry.value(17).toInt();

            qDebug() << "newdataMysql" << serverip << "plcDO" << plcDO << "plcDI" << plcDI << "HiSpeedA" << HiSpeedA << "HiSpeedB" << HiSpeedB << "HiSpeedC" << HiSpeedC << "modbusA" << modbusA << "modbusB" << modbusB << "modbusC" << modbusC << "Gps" << Gps << "systeminit" << systeminit << "communication" << communication << "relayStart" << relayStart << "surge" << surge << "periodic" << periodic << "manualTest" << manualTest << "lflFail" << lflFail << "lflOperate" << lflOperate;
            emit newdataMysql(serverip, plcDO, plcDI, HiSpeedA, HiSpeedB, HiSpeedC, modbusA, modbusB, modbusC, Gps, systeminit, communication, relayStart, surge, periodic, manualTest, lflFail, lflOperate);
        }
    }
    db.close();
    return true;
}

// MONITOR

void Database::getEventandAlarm(QString msg) {
    //    qDebug() << "Database:" << msg;

    if (msg == "getEventandAlarm") {
        QSqlQuery checkTableQuery(db);
        QString checkTableSQL = QString(
                                    "SELECT COUNT(*) FROM information_schema.tables "
                                    "WHERE table_schema = '%1' AND table_name = 'eventandalarm'")
                                    .arg(db.databaseName());

        if (!checkTableQuery.exec(checkTableSQL)) {
            qDebug() << "Failed to check if table exists:" << checkTableQuery.lastError().text();
            return;
        }

        if (checkTableQuery.next() && checkTableQuery.value(0).toInt() > 0) {
            qDebug() << "Table `eventandalarm` exists. Fetching data...";

            QSqlQuery query(db);
            if (!query.exec("SELECT date, time, event_name, status FROM eventandalarm")) {
                qDebug() << "Query execution failed:" << query.lastError().text();
                return;
            }

            if (query.first()) {
                QString date = query.value("date").toString();
                QString time = query.value("time").toString();
                QString eventName = query.value("event_name").toString();
                QString status = query.value("status").toString();

                QString EventandAlarm = QString(
                                            "{\"objectName\"    :\"getEventandAlarm\","
                                            "\"Date\"           :\"%1\","
                                            "\"Time\"           :\"%2\","
                                            "\"EventName\"      :\"%3\","
                                            "\"Status\"         :\"%4\"")
                                            .arg(date)
                                            .arg(time)
                                            .arg(eventName)
                                            .arg(status);
                qDebug() << EventandAlarm;
                emit cmdmsg(EventandAlarm);
            } else {
                qDebug() << "No data found in `eventandalarm` table.";
            }
        } else {
            qDebug() << "Table `eventandalarm` does not exist.";
        }
    }
}

void Database::DistanceandDetailPhaseA(QString msg) {
    qDebug() << "DistanceandDetailPhaseA:" << msg;

    // ตรวจสอบว่า database เปิดอยู่หรือไม่
    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open.";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command["objectName"].toString();

    if (getCommand.contains("getDistanceDetailA")) {
        QString phase = command["PHASE"].toString();
        double distancecmd = command["Distance"].toDouble();
        QString detailcmd = command["Detail"].toString();

        qDebug() << "getDistanceDetailA:" << distancecmd << detailcmd << "Phase:" << phase;

        if (phase.isEmpty() || detailcmd.isEmpty()) {
            qDebug() << "Invalid data: Phase or Detail is empty!";
            return;
        }

        QSqlQuery query;

        // ดึงค่า temp_no สูงสุดสำหรับ phase ที่กำหนด
        query.prepare("SELECT MAX(temp_no) FROM DataTagging WHERE Phase = :phase");
        query.bindValue(":phase", phase);

        int newTempNo = 1;
        if (!query.exec()) {
            qDebug() << "Failed to execute query:" << query.lastQuery();
            qDebug() << "Error:" << query.lastError().text();
            return;
        }

        if (query.next()) {
            QVariant maxTempNo = query.value(0);
            if (maxTempNo.isValid() && !maxTempNo.isNull()) {
                newTempNo = maxTempNo.toInt() + 1;
            }
        } else {
            qDebug() << "No data found for phase:" << phase;
        }

        // INSERT ข้อมูลใหม่ (ไม่ระบุคอลัมน์ No เนื่องจากเป็น auto-increment)
        query.prepare(
            "INSERT INTO DataTagging (status, `Distance(Km)`, Detail, Phase, temp_no) "
            "VALUES (:status, :distance, :detail, :phase, :temp_no)");
        query.bindValue(":status", 0);
        query.bindValue(":distance", distancecmd);
        query.bindValue(":detail", detailcmd);
        query.bindValue(":phase", phase);
        query.bindValue(":temp_no", newTempNo);

        if (!query.exec()) {
            qDebug() << "Failed to insert data:" << query.lastError().text();
            db.close();
            return;
        }

        // ดึงค่า No ที่ auto-generated หลังการ INSERT
        QVariant noVal = query.lastInsertId();
        qDebug() << "Data inserted successfully with No:" << noVal.toInt() << "temp_no:" << newTempNo;

        QString getTaggingPhaseA = QString(
                                       "{"
                                       "\"objectName\":\"TaggingPhaseA\","
                                       "\"No\":%1,"
                                       "\"status\":%2,"
                                       "\"Distance\":%3,"
                                       "\"Detail\":\"%4\","
                                       "\"Phase\":\"%5\","
                                       "\"temp_no\":%6"
                                       "}")
                                       .arg(noVal.toInt())
                                       .arg(0)
                                       .arg(distancecmd)
                                       .arg(detailcmd)
                                       .arg(phase)
                                       .arg(newTempNo);

        qDebug() << "getTaggingPhaseA:" << getTaggingPhaseA;
        cmdmsg(getTaggingPhaseA);
    }

    db.close();
}

void Database::DistanceandDetailPhaseB(QString msg) {
    qDebug() << "DistanceandDetailPhaseB:" << msg;

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open.";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command["objectName"].toString();

    if (getCommand.contains("getDistanceDetailB")) {
        QString phase = command["PHASE"].toString();
        double distancecmd = command["Distance"].toDouble();
        QString detailcmd = command["Detail"].toString();

        qDebug() << "getDistanceDetailB:" << distancecmd << detailcmd << "Phase:" << phase;

        if (phase.isEmpty() || detailcmd.isEmpty()) {
            qDebug() << "Invalid data: Phase or Detail is empty!";
            return;
        }

        QSqlQuery query;

        query.prepare("SELECT MAX(temp_no) FROM DataTagging WHERE Phase = :phase");
        query.bindValue(":phase", phase);

        int newTempNo = 1;
        if (!query.exec()) {
            qDebug() << "Failed to execute query:" << query.lastQuery();
            qDebug() << "Error:" << query.lastError().text();
            return;
        }

        if (query.next()) {
            QVariant maxTempNo = query.value(0);
            if (maxTempNo.isValid() && !maxTempNo.isNull()) {
                newTempNo = maxTempNo.toInt() + 1;
            }
        } else {
            qDebug() << "No data found for phase:" << phase;
        }

        query.prepare(
            "INSERT INTO DataTagging (status, `Distance(Km)`, Detail, Phase, temp_no) "
            "VALUES (:status, :distance, :detail, :phase, :temp_no)");
        query.bindValue(":status", 0);
        query.bindValue(":distance", distancecmd);
        query.bindValue(":detail", detailcmd);
        query.bindValue(":phase", phase);
        query.bindValue(":temp_no", newTempNo);

        if (!query.exec()) {
            qDebug() << "Failed to insert data:" << query.lastError().text();
            db.close();
            return;
        }

        QVariant noVal = query.lastInsertId();
        qDebug() << "Data inserted successfully with No:" << noVal.toInt() << "temp_no:" << newTempNo;

        QString getTaggingPhaseB = QString(
                                       "{"
                                       "\"objectName\":\"TaggingPhaseB\","
                                       "\"No\":%1,"
                                       "\"status\":%2,"
                                       "\"Distance\":%3,"
                                       "\"Detail\":\"%4\","
                                       "\"Phase\":\"%5\","
                                       "\"temp_no\":%6"
                                       "}")
                                       .arg(noVal.toInt())
                                       .arg(0)
                                       .arg(distancecmd)
                                       .arg(detailcmd)
                                       .arg(phase)
                                       .arg(newTempNo);

        qDebug() << "getTaggingPhaseB:" << getTaggingPhaseB;
        cmdmsg(getTaggingPhaseB);
    }

    db.close();
}

void Database::DistanceandDetailPhaseC(QString msg) {
    qDebug() << "DistanceandDetailPhaseC:" << msg;

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open.";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command["objectName"].toString();

    if (getCommand.contains("getDistanceDetailC")) {
        QString phase = command["PHASE"].toString();
        double distancecmd = command["Distance"].toDouble();
        QString detailcmd = command["Detail"].toString();

        qDebug() << "getDistanceDetailC:" << distancecmd << detailcmd << "Phase:" << phase;

        if (phase.isEmpty() || detailcmd.isEmpty()) {
            qDebug() << "Invalid data: Phase or Detail is empty!";
            return;
        }

        QSqlQuery query;

        query.prepare("SELECT MAX(temp_no) FROM DataTagging WHERE Phase = :phase");
        query.bindValue(":phase", phase);

        int newTempNo = 1;
        if (!query.exec()) {
            qDebug() << "Failed to execute query:" << query.lastQuery();
            qDebug() << "Error:" << query.lastError().text();
            return;
        }

        if (query.next()) {
            QVariant maxTempNo = query.value(0);
            if (maxTempNo.isValid() && !maxTempNo.isNull()) {
                newTempNo = maxTempNo.toInt() + 1;
            }
        } else {
            qDebug() << "No data found for phase:" << phase;
        }

        query.prepare(
            "INSERT INTO DataTagging (status, `Distance(Km)`, Detail, Phase, temp_no) "
            "VALUES (:status, :distance, :detail, :phase, :temp_no)");
        query.bindValue(":status", 0);
        query.bindValue(":distance", distancecmd);
        query.bindValue(":detail", detailcmd);
        query.bindValue(":phase", phase);
        query.bindValue(":temp_no", newTempNo);

        if (!query.exec()) {
            qDebug() << "Failed to insert data:" << query.lastError().text();
            db.close();
            return;
        }

        QVariant noVal = query.lastInsertId();
        qDebug() << "Data inserted successfully with No:" << noVal.toInt() << "temp_no:" << newTempNo;

        QString getTaggingPhaseC = QString(
                                       "{"
                                       "\"objectName\":\"TaggingPhaseC\","
                                       "\"No\":%1,"
                                       "\"status\":%2,"
                                       "\"Distance\":%3,"
                                       "\"Detail\":\"%4\","
                                       "\"Phase\":\"%5\","
                                       "\"temp_no\":%6"
                                       "}")
                                       .arg(noVal.toInt())
                                       .arg(0)
                                       .arg(distancecmd)
                                       .arg(detailcmd)
                                       .arg(phase)
                                       .arg(newTempNo);

        qDebug() << "getTaggingPhaseC:" << getTaggingPhaseC;
        cmdmsg(getTaggingPhaseC);
    }

    db.close();
}

void Database::getMySqlPhaseA(QString msg) {
    qDebug() << "getMySqlPhase:" << msg;

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            db.close();
            return;
        }
    }

    QSqlQuery query;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();

    if (getCommand.contains("TaggingPhaseA")) {
        if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'A'")) {
            while (query.next()) {
                QVariant statusVar = query.value("status");
                QVariant numListVar = query.value("No");
                QVariant tempNoVar = query.value("temp_no");
                QVariant distanceVar = query.value("Distance(Km)");
                QVariant detailVar = query.value("Detail");
                QVariant phaseVar = query.value("Phase");

                bool status = statusVar.toInt() != 0;
                int num_list = numListVar.toInt();
                int temp_no = tempNoVar.toInt();
                double Distance = distanceVar.toDouble();
                QString Detail = detailVar.toString();
                QString Phase = phaseVar.toString();

                // Debug ค่าของตัวแปร
                qDebug() << "Debug Variables and Types:";
                qDebug() << "status (as bool):" << (status ? "true" : "false") << "type: bool";
                qDebug() << "num_list:" << num_list << "type:" << numListVar.typeName();
                qDebug() << "temp_no:" << temp_no << "type:" << tempNoVar.typeName();
                qDebug() << "Distance:" << Distance << "type:" << distanceVar.typeName();
                qDebug() << "Detail:" << Detail << "type:" << detailVar.typeName();
                qDebug() << "Phase:" << Phase << "type:" << phaseVar.typeName();

                if (Phase.isEmpty() || Detail.isEmpty()) {
                    qWarning() << "Warning: Empty Phase or Detail. Skipping this row.";
                    continue;
                }

                QString message = QString(
                                      "{\"objectName\"  :\"getMySqlPhaseA\", "
                                      "\"status\"       :%1, "
                                      "\"num_list\"     :%2, "
                                      "\"temp_no\"      :%3, "
                                      "\"Distance\"     :\"%4\", "
                                      "\"Detail\"       :\"%5\", "
                                      "\"Phase\"        :\"%6\"}")
                                      .arg(status ? "true" : "false")
                                      .arg(num_list)
                                      .arg(temp_no)
                                      .arg(Distance)
                                      .arg(Detail)
                                      .arg(Phase);

                qDebug() << "Sent message get PhaseA:" << message;
                emit cmdmsg(message);
            }
        } else {
            qDebug() << "Failed to execute query:" << query.lastError().text();
        }
        closeMySQL();
    }
}

void Database::getMySqlPhaseB(QString msg) {
    qDebug() << "getMySqlPhaseB:" << msg;

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            db.close();
            return;
        }
    }

    QSqlQuery query;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("TaggingPhaseB")) {
        if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'B'")) {
            while (query.next()) {
                QVariant statusVar = query.value("status");
                QVariant numListVar = query.value("No");
                QVariant tempNoVar = query.value("temp_no");
                QVariant distanceVar = query.value("Distance(Km)");
                QVariant detailVar = query.value("Detail");
                QVariant phaseVar = query.value("Phase");

                bool status = statusVar.toInt() != 0;
                int num_list = numListVar.toInt();
                int temp_no = tempNoVar.toInt();
                double Distance = distanceVar.toDouble();
                QString Detail = detailVar.toString();
                QString Phase = phaseVar.toString();

                // Debug ค่าของตัวแปร
                qDebug() << "Debug Variables and Types:";
                qDebug() << "status (as bool):" << (status ? "true" : "false") << "type: bool";
                qDebug() << "num_list:" << num_list << "type:" << numListVar.typeName();
                qDebug() << "temp_no:" << temp_no << "type:" << tempNoVar.typeName();
                qDebug() << "Distance:" << Distance << "type:" << distanceVar.typeName();
                qDebug() << "Detail:" << Detail << "type:" << detailVar.typeName();
                qDebug() << "Phase:" << Phase << "type:" << phaseVar.typeName();

                if (Phase.isEmpty() || Detail.isEmpty()) {
                    qWarning() << "Warning: Empty Phase or Detail. Skipping this row.";
                    continue;
                }

                QString message = QString(
                                      "{\"objectName\"  :\"getMySqlPhaseB\", "
                                      "\"status\"       :%1, "
                                      "\"num_list\"     :%2, "
                                      "\"temp_no\"      :%3, "
                                      "\"Distance\"     :\"%4\", "
                                      "\"Detail\"       :\"%5\", "
                                      "\"Phase\"        :\"%6\"}")
                                      .arg(status ? "true" : "false")
                                      .arg(num_list)
                                      .arg(temp_no)
                                      .arg(Distance)
                                      .arg(Detail)
                                      .arg(Phase);
                qDebug() << "Sent message get PhaseB:" << message;
                emit cmdmsg(message);
            }
        } else {
            qDebug() << "Failed to execute query:" << query.lastError().text();
        }
        closeMySQL();
    }
}

void Database::getMySqlPhaseC(QString msg) {
    qDebug() << "getMySqlPhaseC:" << msg;
    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            db.close();
            return;
        }
    }
    QSqlQuery query;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("TaggingPhaseC")) {
        if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'C'")) {
            while (query.next()) {
                bool status = query.value("status").toBool();
                int num_list = query.value("No").toInt();  // ดึงค่าของ No
                int temp_no = query.value("temp_no").toInt();
                double Distance = query.value("Distance(Km)").toDouble();
                QString Detail = query.value("Detail").toString();
                QString Phase = query.value("Phase").toString();

                QString message = QString(
                                      "{\"objectName\":\"getMySqlPhaseC\", "
                                      "\"status\":%1, "
                                      "\"num_list\":%2, "
                                      "\"temp_no\":%3, "
                                      "\"Distance\":\"%4\", "
                                      "\"Detail\":\"%5\", "
                                      "\"Phase\":\"%6\"}")
                                      .arg(status ? "true" : "false")
                                      .arg(num_list)  // ใส่ค่า num_list
                                      .arg(temp_no)
                                      .arg(Distance)
                                      .arg(Detail)
                                      .arg(Phase);

                qDebug() << "Sent message get PhaseB:" << message;
                emit cmdmsg(message);
            }
        } else {
            qDebug() << "Failed to execute query:" << query.lastError().text();
        }
        closeMySQL();
    }
}

void Database::deletedDataMySQLPhaseA(QString msg) {
    qDebug() << "deletedDataMySQLPhaseA:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString numListStr = command["num_listA"].toString();
    int list_deviceID = numListStr.toInt();
    bool checkedStates = (command["checkedStates"].toString() == "1" || command["checkedStates"].toBool());

    qDebug() << "Parsed JSON: list_deviceID =" << list_deviceID << ", checkedStates =" << checkedStates;

    if (checkedStates) {
        if (!db.isOpen() && !db.open()) {
            qWarning() << "Database is not open and failed to reopen!";
            emit databaseError();
            return;
        }

        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM DataTagging WHERE No = :No AND Phase = 'A'");
        checkQuery.bindValue(":No", list_deviceID);

        if (!checkQuery.exec()) {
            qWarning() << "Failed to execute checkQuery:" << checkQuery.lastError().text();
            return;
        }
        if (checkQuery.next()) {
            int recordCount = checkQuery.value(0).toInt();
            qDebug() << "checkQuery result: recordCount =" << recordCount;

            if (recordCount > 0) {
                QSqlQuery deleteQuery;
                deleteQuery.prepare("DELETE FROM DataTagging WHERE No = :No AND Phase = 'A'");
                deleteQuery.bindValue(":No", list_deviceID);
                if (deleteQuery.exec()) {
                    qDebug() << "Phase A: Record with No =" << list_deviceID << "deleted successfully.";
                } else {
                    qWarning() << "Phase A: ERROR! Failed to delete the record:" << deleteQuery.lastError().text();
                    emit databaseError();
                }
            } else {
                qDebug() << "Phase A: No record found with No =" << list_deviceID;
            }
            db.close();
        }
    } else {
        qDebug() << "Phase A: checkedStates is false. No deletion performed.";
    }

    updateTablePhaseA("updatedataTableA");
}

void Database::deletedDataMySQLPhaseB(QString msg) {
    qDebug() << "deletedDataMySQLPhaseB:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString numListStr = command["num_listB"].toString();
    int list_deviceID = numListStr.toInt();
    bool checkedStates = (command["checkedStates"].toString() == "1" || command["checkedStates"].toBool());

    if (checkedStates) {
        if (!db.isOpen() && !db.open()) {
            qWarning() << "Failed to open the database!";
            emit databaseError();
            return;
        }

        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM DataTagging WHERE No = :No AND Phase = 'B'");
        checkQuery.bindValue(":No", list_deviceID);

        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            QSqlQuery deleteQuery;
            deleteQuery.prepare("DELETE FROM DataTagging WHERE No = :No AND Phase = 'B'");
            deleteQuery.bindValue(":No", list_deviceID);

            if (deleteQuery.exec()) {
                qDebug() << "Phase B: Record with No" << list_deviceID << "deleted successfully.";
            } else {
                qWarning() << "Phase B: ERROR! Failed to delete the record:" << deleteQuery.lastError().text();
                emit databaseError();
            }
        } else {
            qDebug() << "Phase B: No record found with No =" << list_deviceID;
        }
    } else {
        qDebug() << "Phase B: checkedStates is false. No deletion performed.";
    }
}

void Database::deletedDataMySQLPhaseC(QString msg) {
    qDebug() << "deletedDataMySQLPhaseC:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString numListStr = command["num_listC"].toString();
    int list_deviceID = numListStr.toInt();
    bool checkedStates = (command["checkedStates"].toString() == "1" || command["checkedStates"].toBool());

    if (checkedStates) {
        if (!db.isOpen() && !db.open()) {
            qWarning() << "Failed to open the database!";
            emit databaseError();
            return;
        }

        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM DataTagging WHERE No = :No AND Phase = 'C'");
        checkQuery.bindValue(":No", list_deviceID);

        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            QSqlQuery deleteQuery;
            deleteQuery.prepare("DELETE FROM DataTagging WHERE No = :No AND Phase = 'C'");
            deleteQuery.bindValue(":No", list_deviceID);

            if (deleteQuery.exec()) {
                qDebug() << "Phase C: Record with No" << list_deviceID << "deleted successfully.";
            } else {
                qWarning() << "Phase C: ERROR! Failed to delete the record:" << deleteQuery.lastError().text();
                emit databaseError();
            }
        } else {
            qDebug() << "Phase C: No record found with No =" << list_deviceID;
        }
    } else {
        qDebug() << "Phase C: checkedStates is false. No deletion performed.";
    }
}

void Database::updateDataBaseDisplay(QString msg) {
    qDebug() << "getMySqlPhaseA:" << msg;
    QSqlQuery query;

    if (query.exec("SELECT * FROM DataTagging")) {
        QString clearMessage = "{\"objectName\":\"updateDataDisplay\"}";
        cmdmsg(clearMessage);

        while (query.next()) {
            bool status = query.value("status").toBool();
            int num_list = query.value("No").toInt();
            int temp_no = query.value("temp_no").toInt();
            double Distance = query.value("Distance(Km)").toDouble();
            QString Detail = query.value("Detail").toString();
            QString Phase = query.value("Phase").toString();

            QString message = QString(
                                  "{\"objectName\":\"updateDataDisplay\", "
                                  "\"status\":%1, "
                                  "\"num_list\":%2, "
                                  "\"temp_no\":%3, "
                                  "\"Distance\":\"%4\", "
                                  "\"Detail\":\"%5\", "
                                  "\"Phase\":\"%6\"}")
                                  .arg(status ? "true" : "false")
                                  .arg(num_list)
                                  .arg(temp_no)
                                  .arg(Distance)
                                  .arg(Detail)
                                  .arg(Phase);

            qDebug() << "Sent message:" << message;
            cmdmsg(message);
        }
    } else {
        qDebug() << "Failed to execute query:" << query.lastError().text();
    }
    db.close();
}

void Database::updateTablePhaseA(QString msg) {
    qDebug() << "updateTablePhaseA:" << msg;

    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query;
    if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'A'")) {
        while (query.next()) {
            QVariant statusVar = query.value("status");
            QVariant numListVar = query.value("No");
            QVariant tempNoVar = query.value("temp_no");
            QVariant distanceVar = query.value("Distance(Km)");
            QVariant detailVar = query.value("Detail");
            QVariant phaseVar = query.value("Phase");

            bool status = statusVar.toInt() != 0;
            int num_list = numListVar.toInt();
            int temp_no = tempNoVar.toInt();
            double distance = distanceVar.toDouble();
            QString detail = detailVar.toString();
            QString phase = phaseVar.toString();

            qDebug() << "Debug Variables and Types:";
            qDebug() << "status (as bool):" << (status ? "true" : "false") << "type: bool";
            qDebug() << "num_list:" << num_list << "type:" << numListVar.typeName();
            qDebug() << "temp_no:" << temp_no << "type:" << tempNoVar.typeName();
            qDebug() << "Distance:" << distance << "type:" << distanceVar.typeName();
            qDebug() << "Detail:" << detail << "type:" << detailVar.typeName();
            qDebug() << "Phase:" << phase << "type:" << phaseVar.typeName();

            QString newMessage = QString(
                                     "{\"objectName\":\"updatedataTableA\", "
                                     "\"status\":%1, "
                                     "\"num_list\":%2, "
                                     "\"temp_no\":%3, "
                                     "\"Distance\":%4, "
                                     "\"Detail\":\"%5\", "
                                     "\"Phase\":\"%6\"}")
                                     .arg(status ? "true" : "false")
                                     .arg(num_list)
                                     .arg(temp_no)
                                     .arg(distance)
                                     .arg(detail)
                                     .arg(phase);

            qDebug() << "newMessage:" << newMessage;
            emit cmdmsg(newMessage);
            db.close();
        }
    } else {
        qDebug() << "Failed to execute query for Phase A:" << query.lastError().text();
    }
}

void Database::updateTablePhaseB(QString msg) {
    qDebug() << "updateTablePhaseB:" << msg;

    static QSet<int> sentNumLists;

    QSqlQuery query;
    if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'B' ORDER BY temp_no DESC LIMIT 1")) {
        if (query.next()) {
            bool status = query.value("status").toBool();
            int num_list = query.value("No").toInt();
            int temp_no = query.value("temp_no").toInt();
            double distance = query.value("Distance(Km)").toDouble();
            QString detail = query.value("Detail").toString();
            QString phase = query.value("Phase").toString();

            QString newMessage = QString(
                                     "{\"objectName\":\"updatedataTableB\", "
                                     "\"statusB\":%1, "
                                     "\"num_listB\":%2, "
                                     "\"temp_noB\":%3, "
                                     "\"DistanceB\":\"%4\", "
                                     "\"DetailB\":\"%5\", "
                                     "\"PhaseB\":\"%6\"}")
                                     .arg(status ? "true" : "false")
                                     .arg(num_list)
                                     .arg(temp_no)
                                     .arg(distance)
                                     .arg(detail)
                                     .arg(phase);

            if (!sentNumLists.contains(num_list)) {
                emit cmdmsg(newMessage);

                sentNumLists.insert(num_list);
            }
        }
    } else {
        qDebug() << "Failed to execute query for Phase B:" << query.lastError().text();
    }
}

void Database::updateTablePhaseC(QString msg) {
    qDebug() << "updateTablePhaseC:" << msg;

    static QSet<int> sentNumLists;

    QSqlQuery query;
    if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'C' ORDER BY temp_no DESC LIMIT 1")) {
        if (query.next()) {
            bool status = query.value("status").toBool();
            int num_list = query.value("No").toInt();
            int temp_no = query.value("temp_no").toInt();
            double distance = query.value("Distance(Km)").toDouble();
            QString detail = query.value("Detail").toString();
            QString phase = query.value("Phase").toString();

            QString newMessage = QString(
                                     "{\"objectName\":\"updatedataTableC\", "
                                     "\"statusC\":%1, "
                                     "\"num_listC\":%2, "
                                     "\"temp_noC\":%3, "
                                     "\"DistanceC\":\"%4\", "
                                     "\"DetailC\":\"%5\", "
                                     "\"PhaseC\":\"%6\"}")
                                     .arg(status ? "true" : "false")
                                     .arg(num_list)
                                     .arg(temp_no)
                                     .arg(distance)
                                     .arg(detail)
                                     .arg(phase);

            if (!sentNumLists.contains(num_list)) {
                emit cmdmsg(newMessage);

                sentNumLists.insert(num_list);
            }
        }
    } else {
        qDebug() << "Failed to execute query for Phase C:" << query.lastError().text();
    }
}

void Database::edittingMysqlA(QString msg) {
    qDebug() << "edittingMysqlA received message:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("editDataPhaseA")) {
        int IndexNum = command["IndexNum"].toInt();
        QString phase = command["PHASE"].toString().trimmed();

        qDebug() << "edittingMysqlA:" << IndexNum << phase;

        if (!db.isOpen() && !db.open()) {
            qDebug() << "Database connection failed:" << db.lastError().text();
            return;
        }

        QSqlQuery query(db);
        query.prepare("SELECT status, No, `Distance(Km)`, Detail, Phase, temp_no FROM DataTagging WHERE temp_no = :temp_no AND Phase = :phase");
        query.bindValue(":temp_no", IndexNum);
        query.bindValue(":phase", phase);

        if (!query.exec()) {
            qDebug() << "Query execution failed:" << query.lastError().text();
            db.close();
            return;
        }

        while (query.next()) {
            bool statusOk, noOk, distanceOk, tempNoOk;
            int status = query.value("status").toInt(&statusOk);
            int num_list = query.value("No").toInt(&noOk);
            double distance = query.value("Distance(Km)").toDouble(&distanceOk);
            int temp_no = query.value("temp_no").toInt(&tempNoOk);
            QString detail = query.value("Detail").toString();
            QString phase = query.value("Phase").toString();

            if (!statusOk) status = 0;
            if (!noOk) num_list = 0;
            if (!distanceOk) distance = 0.0;
            if (!tempNoOk) temp_no = 0;

            detail = detail.replace("\"", "\\\"");

            qDebug() << "Debug Variables and Types:";
            qDebug() << "status:" << status << "num_list:" << num_list << "temp_no:" << temp_no;
            qDebug() << "Distance:" << distance << "Detail:" << detail << "Phase:" << phase;

            QString newMessage = QString(R"({"objectName":"editDataTaggingPhaseA",)"
                                         R"("status":%1, "num_list":%2, "temp_no":%3,)"
                                         R"("Distance":%4, "Detail":"%5", "Phase":"%6"})")
                                     .arg(status)
                                     .arg(num_list)
                                     .arg(temp_no)
                                     .arg(distance)
                                     .arg(detail)
                                     .arg(phase);

            qDebug() << "newMessage:" << newMessage;
            emit cmdmsg(newMessage);
        }
        db.close();
        qDebug() << "Database connection closed.";
    }
}

void Database::edittingMysqlB(QString msg) {
    qDebug() << "edittingMysqlB received message:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("edittingMysqlB")) {
        int IndexNum = command["IndexNum"].toInt();
        QString phase = command["PHASE"].toString().trimmed();

        qDebug() << "edittingMysqlB:" << IndexNum << phase;

        if (!db.isOpen() && !db.open()) {
            qDebug() << "Database connection failed:" << db.lastError().text();
            return;
        }

        QSqlQuery query(db);
        query.prepare("SELECT status, No, `Distance(Km)`, Detail, Phase, temp_no FROM DataTagging WHERE No = :IndexNum AND Phase = :phase");
        query.bindValue(":IndexNum", IndexNum);
        query.bindValue(":phase", phase);

        if (!query.exec()) {
            qDebug() << "Query execution failed:" << query.lastError().text();
            db.close();
            return;
        }

        while (query.next()) {
            bool statusOk, noOk, distanceOk, tempNoOk;
            int status = query.value("status").toInt(&statusOk);
            int num_list = query.value("No").toInt(&noOk);
            double distance = query.value("Distance(Km)").toDouble(&distanceOk);
            int temp_no = query.value("temp_no").toInt(&tempNoOk);
            QString detail = query.value("Detail").toString();
            QString phase = query.value("Phase").toString();

            if (!statusOk) status = 0;
            if (!noOk) num_list = 0;
            if (!distanceOk) distance = 0.0;
            if (!tempNoOk) temp_no = 0;

            detail = detail.replace("\"", "\\\"");

            qDebug() << "Debug Variables and Types:";
            qDebug() << "status:" << status << "num_list:" << num_list << "temp_no:" << temp_no;
            qDebug() << "Distance:" << distance << "Detail:" << detail << "Phase:" << phase;

            QString newMessage = QString(R"({"objectName":"editDataTaggingPhaseB",)"
                                         R"("status":%1, "num_list":%2, "temp_no":%3,)"
                                         R"("Distance":%4, "Detail":"%5", "Phase":"%6"})")
                                     .arg(status)
                                     .arg(num_list)
                                     .arg(temp_no)
                                     .arg(distance)
                                     .arg(detail)
                                     .arg(phase);

            qDebug() << "newMessage:" << newMessage;
            emit cmdmsg(newMessage);
        }
        db.close();
        qDebug() << "Database connection closed.";
    }
}
void Database::edittingMysqlC(QString msg) {
    qDebug() << "edittingMysqlC received message:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("edittingMysqlC")) {
        int IndexNum = command["IndexNum"].toInt();
        QString phase = command["PHASE"].toString().trimmed();

        qDebug() << "edittingMysqlC:" << IndexNum << phase;

        if (!db.isOpen() && !db.open()) {
            qDebug() << "Database connection failed:" << db.lastError().text();
            return;
        }

        QSqlQuery query(db);
        query.prepare("SELECT status, No, `Distance(Km)`, Detail, Phase, temp_no FROM DataTagging WHERE No = :IndexNum AND Phase = :phase");
        query.bindValue(":IndexNum", IndexNum);
        query.bindValue(":phase", phase);

        if (!query.exec()) {
            qDebug() << "Query execution failed:" << query.lastError().text();
            db.close();
            return;
        }

        while (query.next()) {
            bool statusOk, noOk, distanceOk, tempNoOk;
            int status = query.value("status").toInt(&statusOk);
            int num_list = query.value("No").toInt(&noOk);
            double distance = query.value("Distance(Km)").toDouble(&distanceOk);
            int temp_no = query.value("temp_no").toInt(&tempNoOk);
            QString detail = query.value("Detail").toString();
            QString phase = query.value("Phase").toString();

            if (!statusOk) status = 0;
            if (!noOk) num_list = 0;
            if (!distanceOk) distance = 0.0;
            if (!tempNoOk) temp_no = 0;

            detail = detail.replace("\"", "\\\"");

            qDebug() << "Debug Variables and Types:";
            qDebug() << "status:" << status << "num_list:" << num_list << "temp_no:" << temp_no;
            qDebug() << "Distance:" << distance << "Detail:" << detail << "Phase:" << phase;

            QString newMessage = QString(R"({"objectName":"editDataTaggingPhaseC",)"
                                         R"("status":%1, "num_list":%2, "temp_no":%3,)"
                                         R"("Distance":%4, "Detail":"%5", "Phase":"%6"})")
                                     .arg(status)
                                     .arg(num_list)
                                     .arg(temp_no)
                                     .arg(distance)
                                     .arg(detail)
                                     .arg(phase);

            qDebug() << "newMessage:" << newMessage;
            emit cmdmsg(newMessage);
        }
        db.close();
        qDebug() << "Database connection closed.";
    }
}

void Database::closeMySQL() {
    if (db.isOpen()) {
        qDebug() << "Database is open. Closing all connections...";
        db.close();
        qDebug() << "All database connections are closed.";
    } else {
        qDebug() << "Database is already closed.";
    }
}

void Database::configParemeterThreshold(QString msg) {
    qDebug() << "configParameterThreshold:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString Phase = QJsonValue(command["Phase"]).toString();

    if (!db.isOpen() && !db.open()) {
        qDebug() << "Failed to open the database:" << db.lastError().text();
        return;
    }

    QSqlQuery query;

    QStringList phases = {"A", "B", "C"};  // List of all phases to process
    for (const QString &phase : phases) {
        QString thresholdKey = "thresholdInit" + phase;  // e.g., "thresholdA", "thresholdB", etc.
        if (!command.contains(thresholdKey)) {
            qDebug() << "No threshold data for phase:" << phase << ", skipping...";
            continue;
        }

        double paraOfThreshold = command[thresholdKey].toDouble();
        qDebug() << "Processing phase:" << phase << "with threshold:" << paraOfThreshold;

        query.prepare("SELECT COUNT(*) FROM ParamThreshold WHERE Phase = :Phase");
        query.bindValue(":Phase", phase);

        if (!query.exec()) {
            qDebug() << "Failed to check existing data for phase" << phase << ":" << query.lastError().text();
            continue;
        }

        query.next();
        int count = query.value(0).toInt();

        if (count > 0) {
            query.prepare("UPDATE ParamThreshold SET ThresholdValue = :ThresholdValue WHERE Phase = :Phase");
            query.bindValue(":ThresholdValue", paraOfThreshold);
            query.bindValue(":Phase", phase);

            if (!query.exec()) {
                qDebug() << "Failed to update data for phase" << phase << ":" << query.lastError().text();
            } else {
                qDebug() << "Data updated successfully for phase:" << phase;
            }
        } else {
            qDebug() << "No existing data for phase:" << phase << ". Update skipped.";
        }
    }

    closeMySQL();
    getThreshold();
}

void Database::getThreshold() {
    qDebug() << "getThreshold!";

    if (!db.isOpen()) {
        qDebug() << "Database is not open, opening now...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery checkTableQuery(db);
    QString checkTableSQL = QString(
                                "SELECT COUNT(*) FROM information_schema.tables "
                                "WHERE table_schema = '%1' AND table_name = 'ParamThreshold'")
                                .arg(db.databaseName());

    if (!checkTableQuery.exec(checkTableSQL)) {
        qDebug() << "Failed to check if table exists:" << checkTableQuery.lastError().text();
        return;
    }

    if (checkTableQuery.next() && checkTableQuery.value(0).toInt() > 0) {
        qDebug() << "Table `ParamThreshold` exists. Fetching data...";

        QSqlQuery query(db);
        if (!query.exec("SELECT Phase, ThresholdValue FROM ParamThreshold WHERE Phase IN ('A', 'B', 'C')")) {
            qDebug() << "Query execution failed:" << query.lastError().text();
            return;
        }

        double thresholdA = 0.0, thresholdB = 0.0, thresholdC = 0.0;

        while (query.next()) {
            QString phase = query.value("Phase").toString();
            double thresholdValue = query.value("ThresholdValue").toDouble();

            if (phase == "A") {
                thresholdA = thresholdValue;
            } else if (phase == "B") {
                thresholdB = thresholdValue;
            } else if (phase == "C") {
                thresholdC = thresholdValue;
            }
        }

        QString getThresholds = QString(
                                    "{\"objectName\":\"getThreshold\", "
                                    "\"thresholdInitA\":%1, "
                                    "\"thresholdInitB\":%2, "
                                    "\"thresholdInitC\":%3}")
                                    .arg(thresholdA)
                                    .arg(thresholdB)
                                    .arg(thresholdC);

        qDebug() << "updateThreshold:" << getThresholds;
        emit cmdmsg(getThresholds);
        if (!onceTime)
            emit assignThreshold(thresholdA, thresholdB, thresholdC);
        else
            emit sendToCal(getThresholds);
        updateMargin();

    } else {
        qDebug() << "Table `ParamThreshold` does not exist.";
    }

    closeMySQL();
}

void Database::getSettingInfo() {
    qDebug() << "getSettingInfo!";

    if (!db.isOpen()) {
        qDebug() << "Database is not open, opening now...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
    }

    if (!onceTime) {
        qDebug() << "getSettingInfo! onceTime" << onceTime;
        QSqlQuery query(db);
        // Rename table from 'SettingGaneral' to 'SettingGeneral'
        if (!query.exec("RENAME TABLE SettingGaneral TO SettingGeneral;")) {
            qDebug() << "Error renaming table:" << query.lastError().text();
        } else {
            qDebug() << "Table renamed successfully from 'SettingGaneral' to 'SettingGeneral'.";
        }

        query.prepare("SELECT COLUMN_TYPE  FROM INFORMATION_SCHEMA.COLUMNS  WHERE TABLE_NAME = 'SettingGeneral'  AND COLUMN_NAME = 'line_no';");

        if (!query.exec()) {
            qDebug() << "Error checking column type:" << query.lastError().text();
            return;
        }

        if (query.next()) {
            QString columnType = query.value(0).toString();
            qDebug() << "Current column type:" << columnType;

            if (!columnType.startsWith("varchar", Qt::CaseInsensitive)) {
                // Modify column type to VARCHAR(255)
                QSqlQuery alterQuery(db);
                if (!alterQuery.exec("ALTER TABLE SettingGeneral MODIFY COLUMN line_no VARCHAR(255);")) {
                    qDebug() << "Error modifying column:" << alterQuery.lastError().text();
                } else {
                    qDebug() << "Column modified successfully to VARCHAR(255).";
                }
            } else {
                qDebug() << "Column is already VARCHAR, no modification needed.";
            }
        } else {
            qDebug() << "Column not found.";
        }
    } else {
        qDebug() << "getSettingInfo! onceTime" << onceTime;
        QSqlQuery checkTableQuery(db);
        QString checkTableSQL = QString(
                                    "SELECT COUNT(*) FROM information_schema.tables "
                                    "WHERE table_schema = '%1' AND table_name = 'SettingGeneral'")
                                    .arg(db.databaseName());

        if (!checkTableQuery.exec(checkTableSQL)) {
            qDebug() << "Failed to check if table exists:" << checkTableQuery.lastError().text();
            return;
        }

        if (checkTableQuery.next() && checkTableQuery.value(0).toInt() > 0) {
            qDebug() << "Table `SettingGaneral` exists. Fetching data...";

            QSqlQuery query(db);
            if (!query.exec("SELECT voltage, substation, direction, line_no FROM SettingGeneral")) {
                qDebug() << "Query execution failed:" << query.lastError().text();
                return;
            }

            double voltage = 0.0;
            QString substation;
            QString direction;
            QString lineNo = 0;

            while (query.next()) {
                voltage = query.value("voltage").toDouble();
                substation = query.value("substation").toString();
                direction = query.value("direction").toString();
                lineNo = query.value("line_no").toString();
            }

            QString getSettingInfo = QString(
                                         "{\"objectName\":\"getSettingInfo\", "
                                         "\"voltage\":%1, "
                                         "\"substation\":\"%2\", "
                                         "\"direction\":\"%3\", "
                                         "\"line_no\":\"%4\"}")
                                         .arg(voltage)
                                         .arg(substation)
                                         .arg(direction)
                                         .arg(lineNo);

            qDebug() << "Setting Info:" << getSettingInfo;
            emit assignDisplayGerneral(voltage,substation,direction,lineNo);
            emit cmdmsg(getSettingInfo);
            ToShowSettingInfo(getSettingInfo);

        } else {
            qDebug() << "Table `SettingGeneral` does not exist.";
        }
    }
    closeMySQL();
}
void Database::getpreiodicInfo() {
    qDebug() << "getPeriodicInfo!";
    if (!db.isOpen()) {
        qDebug() << "Database is not open, opening now...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery checkTableQuery(db);
    QString checkTableSQL = QString(
                                "SELECT COUNT(*) FROM information_schema.tables "
                                "WHERE table_schema = '%1' AND table_name = 'PeriodicInfo'")
                                .arg(db.databaseName());

    if (!checkTableQuery.exec(checkTableSQL)) {
        qDebug() << "Failed to check if table exists:" << checkTableQuery.lastError().text();
        return;
    }

    if (checkTableQuery.next() && checkTableQuery.value(0).toInt() > 0) {
        qDebug() << "Table `PeriodicInfo` exists. Fetching data...";

        QSqlQuery query(db);
        if (!query.exec("SELECT Time, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday FROM PeriodicInfo")) {
            qDebug() << "Query execution failed:" << query.lastError().text();
            return;
        }

        while (query.next()) {
            times = query.value("Time").toString();
            Monday = query.value("Monday").toBool();
            Tuesday = query.value("Tuesday").toBool();
            Wednesday = query.value("Wednesday").toBool();
            Thursday = query.value("Thursday").toBool();
            Friday = query.value("Friday").toBool();
            Saturday = query.value("Saturday").toBool();
            Sunday = query.value("Sunday").toBool();

            QString getPeriodicInfo = QString(
                                          "{\"objectName\":\"getPeriodicInfo\", "
                                          "\"Time\":\"%1\", "
                                          "\"Monday\":%2, "
                                          "\"Tuesday\":%3, "
                                          "\"Wednesday\":%4, "
                                          "\"Thursday\":%5, "
                                          "\"Friday\":%6, "
                                          "\"Saturday\":%7, "
                                          "\"Sunday\":%8}")
                                          .arg(times)
                                          .arg(Monday ? "true" : "false")
                                          .arg(Tuesday ? "true" : "false")
                                          .arg(Wednesday ? "true" : "false")
                                          .arg(Thursday ? "true" : "false")
                                          .arg(Friday ? "true" : "false")
                                          .arg(Saturday ? "true" : "false")
                                          .arg(Sunday ? "true" : "false");

            qDebug() << "Periodic Info: " << getPeriodicInfo;

            emit cmdmsg(getPeriodicInfo);
        }
    } else {
        qDebug() << "Table `PeriodicInfo` does not exist.";
    }

    db.close();
}

void Database::getUpdatePeriodic(QString msg) {
    qDebug() << "getUpdatePreiodic" << msg;
    db.close();
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString timer = command["Time"].toString();
    qDebug() << "getUpdatePreiodic - Extracted Time:" << timer;

    if (!db.isOpen()) {
        qDebug() << "Database is not open, opening now...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    query.prepare("UPDATE PeriodicInfo SET Time = :newTime");
    query.bindValue(":newTime", timer);

    if (!query.exec()) {
        qDebug() << "Failed to update Time in PeriodicInfo table:" << query.lastError().text();
    } else {
        qDebug() << "Time updated successfully in PeriodicInfo table to" << timer;
    }
    closeMySQL();
    emit updateTimePeriodic(timer);
}

void Database::getUpdateWeekly(QString msg) {
    qDebug() << "getUpdateWeekly:" << msg;
    db.close();
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    // Extracting the values as strings
    QString mondayStr = command["Monday"].toString();
    QString tuesdayStr = command["Tuesday"].toString();
    QString wednesdayStr = command["Wednesday"].toString();
    QString thursdayStr = command["Thursday"].toString();
    QString fridayStr = command["Friday"].toString();
    QString saturdayStr = command["Saturday"].toString();
    QString sundayStr = command["Sunday"].toString();

    // Converting strings to bool (1 -> true, else false)
    bool monday = (mondayStr == "1");
    bool tuesday = (tuesdayStr == "1");
    bool wednesday = (wednesdayStr == "1");
    bool thursday = (thursdayStr == "1");
    bool friday = (fridayStr == "1");
    bool saturday = (saturdayStr == "1");
    bool sunday = (sundayStr == "1");
    qDebug() << "monday:" << monday << " mondayStr:" << mondayStr << " obj['Monday'].toString()" << command["Monday"].toString();
    qDebug() << "tuesday:" << monday << " tuesdayStr:" << tuesdayStr << " obj['Tuesday'].toString()" << command["Tuesday"].toString();
    qDebug() << "wednesday:" << monday << " wednesdayStr:" << wednesdayStr << " obj['Wednesday'].toString()" << command["Wednesday"].toString();
    qDebug() << "mthursdayonday:" << monday << " thursdayStr:" << thursdayStr << " obj['Thursday'].toString()" << command["Thursday"].toString();
    qDebug() << "friday:" << monday << " fridayStr:" << fridayStr << " obj['Friday'].toString()" << command["Friday"].toString();
    qDebug() << "saturday:" << monday << " saturdayStr:" << saturdayStr << " obj['Saturday'].toString()" << command["Saturday"].toString();
    qDebug() << "sunday:" << monday << " sundayStr:" << sundayStr << " obj['Sunday'].toString()" << command["Sunday"].toString();

    bool mondayUpdate;
    bool tuesdayUpdate;
    bool wednesdayUpdate;
    bool thursdayUpdate;
    bool fridayUpdate;
    bool saturdayUpdate;
    bool sundayUpdate;
    // Open the database if it's not already open
    if (!db.isOpen()) {
        qDebug() << "Database is not open, opening now...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
    }

    // Checking each day individually and updating only the days that changed
    if (mondayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Monday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Monday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentMonday = query.value("Monday").toBool();
        qDebug() << "Current Monday value in database:" << currentMonday << monday;
        mondayUpdate = currentMonday;
        if (currentMonday != monday) {
            qDebug() << "Monday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Monday = :newMonday");
            query.bindValue(":newMonday", monday);
            mondayUpdate = monday;
            if (!query.exec()) {
                qDebug() << "Failed to update Monday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Monday updated successfully in PeriodicInfo table to" << monday;
            }
        }
    }

    if (tuesdayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Tuesday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Tuesday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentTuesday = query.value("Tuesday").toBool();
        qDebug() << "Current Tuesday value in database:" << currentTuesday << tuesday;
        tuesdayUpdate = currentTuesday;

        if (currentTuesday != tuesday) {
            qDebug() << "Tuesday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Tuesday = :newTuesday");
            query.bindValue(":newTuesday", tuesday);
            tuesdayUpdate = tuesday;
            if (!query.exec()) {
                qDebug() << "Failed to update Tuesday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Tuesday updated successfully in PeriodicInfo table to" << tuesday;
            }
        }
    }

    if (wednesdayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Wednesday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Wednesday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentWednesday = query.value("Wednesday").toBool();
        qDebug() << "Current Wednesday value in database:" << currentWednesday << wednesday;
        wednesdayUpdate = currentWednesday;

        if (currentWednesday != wednesday) {
            qDebug() << "Wednesday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Wednesday = :newWednesday");
            query.bindValue(":newWednesday", wednesday);
            wednesdayUpdate = wednesday;
            if (!query.exec()) {
                qDebug() << "Failed to update Wednesday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Wednesday updated successfully in PeriodicInfo table to" << wednesday;
            }
        }
    }

    if (thursdayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Thursday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Thursday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentThursday = query.value("Thursday").toBool();
        qDebug() << "Current Thursday value in database:" << currentThursday << thursday;
        thursdayUpdate = currentThursday;

        if (currentThursday != thursday) {
            qDebug() << "Thursday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Thursday = :newThursday");
            query.bindValue(":newThursday", thursday);
            thursdayUpdate = thursday;
            if (!query.exec()) {
                qDebug() << "Failed to update Thursday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Thursday updated successfully in PeriodicInfo table to" << thursday;
            }
        }
    }

    if (fridayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Friday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Friday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentFriday = query.value("Friday").toBool();
        qDebug() << "Current Friday value in database:" << currentFriday << friday;
        fridayUpdate = currentFriday;

        if (currentFriday != friday) {
            qDebug() << "Friday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Friday = :newFriday");
            query.bindValue(":newFriday", friday);
            fridayUpdate = friday;
            if (!query.exec()) {
                qDebug() << "Failed to update Friday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Friday updated successfully in PeriodicInfo table to" << friday;
            }
        }
    }

    if (saturdayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Saturday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Saturday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentSaturday = query.value("Saturday").toBool();
        qDebug() << "Current Saturday value in database:" << currentSaturday << saturday;
        saturdayUpdate = currentSaturday;

        if (currentSaturday != saturday) {
            qDebug() << "Saturday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Saturday = :newSaturday");
            query.bindValue(":newSaturday", saturday);
            saturdayUpdate = saturday;
            if (!query.exec()) {
                qDebug() << "Failed to update Saturday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Saturday updated successfully in PeriodicInfo table to" << saturday;
            }
        }
    }

    if (sundayStr != "") {
        QSqlQuery query(db);
        query.prepare("SELECT Sunday FROM PeriodicInfo LIMIT 1");
        if (!query.exec() || !query.next()) {
            qDebug() << "Failed to fetch current value for Sunday:" << query.lastError().text();
            closeMySQL();
            return;
        }

        bool currentSunday = query.value("Sunday").toBool();
        qDebug() << "Current Sunday value in database:" << currentSunday << sunday;
        sundayUpdate = currentSunday;

        if (currentSunday != sunday) {
            qDebug() << "Sunday value changed. Updating database...";
            query.prepare("UPDATE PeriodicInfo SET Sunday = :newSunday");
            query.bindValue(":newSunday", sunday);
            sundayUpdate = sunday;
            if (!query.exec()) {
                qDebug() << "Failed to update Sunday in PeriodicInfo table:" << query.lastError().text();
            } else {
                qDebug() << "Sunday updated successfully in PeriodicInfo table to" << sunday;
            }
        }
    }

    closeMySQL();
    emit updateDatePeriodic(mondayUpdate, tuesdayUpdate, wednesdayUpdate, thursdayUpdate, fridayUpdate, saturdayUpdate, sundayUpdate);
}

void Database::userMode(QString msg) {
    qDebug() << "userMode:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    if (!command.contains("userType")) {
        qDebug() << "Invalid message format: Missing 'userType'.";
        return;
    }

    QString userType = command["userType"].toString();                // Extract Master/Slave
    QString userStatusSlave = command["userStatusSlave"].toString();  // Extract status (if Slave)

    if (userType != "Master" && userType != "Slave") {
        qDebug() << "Invalid userType value:" << userType;
        return;
    }

    int userStatus = userStatusSlave.toInt();

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);

    query.prepare("UPDATE UserModeTable SET userMode = :userMode, userStatus = :userStatus WHERE Num = 1");
    query.bindValue(":userMode", userType);
    query.bindValue(":userStatus", userStatus);

    if (!query.exec()) {
        qDebug() << "Failed to update UserModeTable:" << query.lastError().text();
    } else {
        qDebug() << "UserModeTable updated successfully. userMode:" << userType << ", userStatus:" << userStatus;
    }

    closeMySQL();
    getUpdateUserMode();
}

void Database::getUpdateUserMode() {
    qDebug() << "Fetching current UserMode from UserModeTable...";

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);

    query.prepare("SELECT Num, userMode, userStatus FROM UserModeTable WHERE Num = 1");

    if (!query.exec()) {
        qDebug() << "Failed to fetch data from UserModeTable:" << query.lastError().text();
    } else if (query.next()) {
        int num = query.value("Num").toInt();
        QString userMode = query.value("userMode").toString();
        bool userStatus = query.value("userStatus").toInt() != 0;  // Convert int to bool

        qDebug() << "Current UserModeTable Data:";
        qDebug() << "Num:" << num << ", userMode:" << userMode << ", userStatus:" << userStatus;
    } else {
        qDebug() << "No data found in UserModeTable.";
    }

    closeMySQL();
}

void Database::ToShowSettingInfo(QString msg) {
    qDebug() << "ToShowSettingInfo Received:" << msg;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();
    double actualVoltage = QJsonValue(command["voltage"]).toDouble();
    QString actualSubstation = QJsonValue(command["substation"]).toString();
    QString actualDirection = QJsonValue(command["direction"]).toString();
    QString actualLineNo = QJsonValue(command["line_no"]).toString();
    QString formattedString = QString("%1 kV %2 - %3 #%4").arg(actualVoltage).arg(actualSubstation).arg(actualDirection).arg(actualLineNo);

    QJsonObject jsonOutput;
    jsonOutput["objectName"] = "detailInfoSetting";
    jsonOutput["data"] = formattedString;

    QJsonDocument doc(jsonOutput);
    QString combinedData = doc.toJson(QJsonDocument::Compact);

    qDebug() << "Final JSON Output:" << combinedData;
    emit cmdmsg(combinedData);
}

void Database::updateSettingInfo(QString msg) {
    qDebug() << "updateSettingInfo:" << msg;
    static int receivedVoltage = 0;
    static QString receivedSubstation = "";
    static QString receivedDirection = "";
    QString receivedLineNo = 0;

    //    static bool hasVoltage = false;
    //    static bool hasSubstation = false;
    //    static bool hasDirection = false;
    //    static bool hasLineNo = false;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    if (d.isNull() || !d.isObject()) {
        qDebug() << "Invalid JSON format!";
        return;
    }

    QJsonObject command = d.object();

    if (command["objectName"] == "InforSettingVoltage") {
        receivedVoltage = command["valueVoltage"].toInt();
        if (!db.isOpen()) {
            qDebug() << "Database is not open, attempting to open...";
            if (!db.open()) {
                qDebug() << "Failed to open the database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery query(db);
        query.prepare("UPDATE SettingGeneral SET voltage = :voltage WHERE id = 1");
        query.bindValue(":voltage", receivedVoltage);

        if (query.exec()) {
            qDebug() << "Database updated successfully.";
        } else {
            qDebug() << "Failed to update database:" << query.lastError().text();
        }

        db.close();
        //        hasVoltage = true;
        qDebug() << "Received Voltage:" << receivedVoltage;
    }

    if (command["objectName"] == "ValueSubstation") {
        receivedSubstation = command["Substation"].toString();
        if (!db.isOpen()) {
            qDebug() << "Database is not open, attempting to open...";
            if (!db.open()) {
                qDebug() << "Failed to open the database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery query(db);
        query.prepare("UPDATE SettingGeneral SET substation = :substation WHERE id = 1");
        query.bindValue(":substation", receivedSubstation);

        if (query.exec()) {
            qDebug() << "Database updated successfully.";
        } else {
            qDebug() << "Failed to update database:" << query.lastError().text();
        }

        db.close();
        //        hasSubstation = true;
        qDebug() << "Received Substation:" << receivedSubstation;
    }

    if (command["objectName"] == "valueDirection") {
        receivedDirection = command["Direction"].toString();
        if (!db.isOpen()) {
            qDebug() << "Database is not open, attempting to open...";
            if (!db.open()) {
                qDebug() << "Failed to open the database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery query(db);
        query.prepare("UPDATE SettingGeneral SET direction = :direction WHERE id = 1");
        query.bindValue(":direction", receivedDirection);

        if (query.exec()) {
            qDebug() << "Database updated successfully.";
        } else {
            qDebug() << "Failed to update database:" << query.lastError().text();
        }

        db.close();
        //        hasDirection = true;
        qDebug() << "Received Direction:" << receivedDirection;
    }

    if (command["objectName"] == "valueLineNo") {
        receivedLineNo = command["LineNo"].toString();
        if (!db.isOpen()) {
            qDebug() << "Database is not open, attempting to open...";
            if (!db.open()) {
                qDebug() << "Failed to open the database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery query(db);
        query.prepare("UPDATE SettingGeneral SET line_no = :line_no WHERE id = 1");
        query.bindValue(":line_no", receivedLineNo);

        if (query.exec()) {
            qDebug() << "Database updated successfully.";
        } else {
            qDebug() << "Failed to update database:" << query.lastError().text();
        }

        db.close();
        //        hasLineNo = true;
        qDebug() << "Received LineNo:" << receivedLineNo;
    }

    getSettingInfo();
}

void Database::updateSettingGeneralInfo(const QString &msg)
{
    qDebug() << "[updateSettingGeneralInfo] msg =" << msg;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[updateSettingGeneralInfo] Invalid JSON:"
                   << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    const QString objectName = obj.value("objectName").toString();

    if (objectName != "UpdateSettingGeneralInfo") {
        qWarning() << "[updateSettingGeneralInfo] Invalid objectName:" << objectName;
        return;
    }

    const int voltage = obj.value("valueVoltage").toInt();
    const QString substation = obj.value("Substation").toString();
    const QString direction = "";
    const QString lineNo = obj.value("LineNo").toString();

    qDebug() << "[updateSettingGeneralInfo]"
             << "voltage =" << voltage
             << "substation =" << substation
             << "direction =" << direction
             << "lineNo =" << lineNo;

    if (!db.isOpen()) {
        qDebug() << "[updateSettingGeneralInfo] Database is not open, opening...";

        if (!db.open()) {
            qWarning() << "[updateSettingGeneralInfo] Failed to open database:"
                       << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);

    query.prepare(
        "UPDATE SettingGeneral "
        "SET voltage = :voltage, "
        "    substation = :substation, "
        "    line_no = :line_no "
        "WHERE id = 1"
        );

    query.bindValue(":voltage", voltage);
    query.bindValue(":substation", substation);
    query.bindValue(":line_no", lineNo);

    if (!query.exec()) {
        qWarning() << "[updateSettingGeneralInfo] Failed to update SettingGeneral:"
                   << query.lastError().text();

        db.close();
        return;
    }

    qDebug() << "[updateSettingGeneralInfo] SettingGeneral updated successfully."
             << "rows affected =" << query.numRowsAffected();

    db.close();

    getSettingInfo();
}

void Database::storeStatusAux(QString msg) {
    qDebug() << "storeStatusAux:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();

    if (getCommand == "statusOperate") {
        bool statusOperate = command["LFLOPERATE"].toBool();
        //        int LFLStatus = statusOperate.toInt(); // แปลงค่าจาก QString เป็น int
        qDebug() << "statusOperate:" << statusOperate;

        //        if (statusOperate == "1") {
        if (!db.isOpen()) {
            qDebug() << "Database is not open. Attempting to open...";
            if (!db.open()) {
                qDebug() << "Failed to open database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery query(db);
        query.prepare("UPDATE OperateTable SET LFL_Operate = :activated WHERE id = 1");
        query.bindValue(":activated", statusOperate);  // เปลี่ยน activated เป็น "LFLOPERATE"

        if (!query.exec()) {
            qDebug() << "Failed to update OperateTable for statusOperate:" << query.lastError().text();
        } else {
            qDebug() << "OperateTable updated successfully. statusOperate:" << statusOperate;
        }

        closeMySQL();
        //        }
    } else if (getCommand == "statusFail") {
        bool statusFail = command["LFLFAIL"].toBool();
        //        int failStatus = statusFail.toInt(); // แปลงค่าจาก QString เป็น int
        qDebug() << "statusFail:" << statusFail;

        //        if (statusFail == "1") {
        if (!db.isOpen()) {
            qDebug() << "Database is not open. Attempting to open...";
            if (!db.open()) {
                qDebug() << "Failed to open database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery query(db);
        query.prepare("UPDATE OperateTable SET LFL_Fail = :activated WHERE id = 1");
        query.bindValue(":activated", statusFail);  // เปลี่ยน activated เป็น "LFLFAIL"

        if (!query.exec()) {
            qDebug() << "Failed to update OperateTable for statusFail:" << query.lastError().text();
        } else {
            qDebug() << "OperateTable updated successfully. statusFail:" << statusFail;
        }

        closeMySQL();
        //        }
    } else {
        qDebug() << "Invalid message format: Unknown 'objectName'.";
    }
}

void Database::UpdateMarginSettingParameter(int margin,
                                            double valueVoltage,
                                            int focusIndex,
                                            const QString &phase)
{
    qDebug() << "Updating MarginSettingParameter:"
             << "margin =" << margin
             << ", valueVoltage =" << valueVoltage
             << ", focusIndex =" << focusIndex
             << ", phase =" << phase;

    // ส่งค่ากลับไปหน้า QML ตาม phase
    QJsonObject param;

    if (phase == "A") {
        param.insert("objectName", "valueMarginA");
        param.insert("valueMarginA", margin);
    } else if (phase == "B") {
        param.insert("objectName", "valueMarginB");
        param.insert("valueMarginB", margin);
    } else if (phase == "C") {
        param.insert("objectName", "valueMarginC");
        param.insert("valueMarginC", margin);
    } else {
        qDebug() << "Unknown phase:" << phase;
    }

    if (!param.isEmpty()) {
        QString raw_data = QString::fromUtf8(
            QJsonDocument(param).toJson(QJsonDocument::Compact)
            );
        emit cmdmsg(raw_data);
    }

    if (!db.isOpen() && !db.open()) {
        qDebug() << "Error: Unable to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery query(db);

    query.prepare("SELECT id FROM MarginSettingParameter WHERE PHASE = :phase");
    query.bindValue(":phase", phase);

    if (!query.exec()) {
        qDebug() << "Error: Unable to query database:" << query.lastError().text();
        db.close();
        return;
    }

    if (query.next()) {
        int id = query.value(0).toInt();

        query.prepare(R"(
            UPDATE MarginSettingParameter
            SET margin = :margin,
                valueVoltage = :valueVoltage,
                focusIndex = :focusIndex
            WHERE id = :id
        )");
        query.bindValue(":margin", margin);
        query.bindValue(":valueVoltage", valueVoltage);
        query.bindValue(":focusIndex", focusIndex);
        query.bindValue(":id", id);

        if (!query.exec()) {
            qDebug() << "Error: Unable to update data:" << query.lastError().text();
        } else {
            qDebug() << "Updated MarginSettingParameter for PHASE:" << phase;
        }
    } else {
        query.prepare(R"(
            INSERT INTO MarginSettingParameter (PHASE, margin, valueVoltage, focusIndex)
            VALUES (:phase, :margin, :valueVoltage, :focusIndex)
        )");
        query.bindValue(":phase", phase);
        query.bindValue(":margin", margin);
        query.bindValue(":valueVoltage", valueVoltage);
        query.bindValue(":focusIndex", focusIndex);

        if (!query.exec()) {
            qDebug() << "Error: Unable to insert data:" << query.lastError().text();
        } else {
            qDebug() << "Inserted new MarginSettingParameter for PHASE:" << phase;
        }
    }

    db.close();
    updateMargin();
}

void Database::UpdateMarginSettingParameter(QString msg) {
    qDebug() << "Updating MarginSettingParameter with data:" << msg;

    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject obj = doc.object();
    int margin = obj["margin"].toInt();
    double valueVoltage = obj["valueVoltage"].toDouble();
    int focusIndex = obj["focusIndex"].toInt();
    QString phase = obj["PHASE"].toString();
    qDebug() << "UPhase update Margin:" << margin << valueVoltage << focusIndex << phase;

    QJsonDocument jsonDoc;
    QJsonObject Param;
    if (obj["objectName"].toString() == "combinedDataPhaseA") {
        Param.insert("objectName", "valueMarginA");
        Param.insert("valueMarginA", margin);
    } else if (obj["objectName"].toString() == "combinedDataPhaseB") {
        Param.insert("objectName", "valueMarginB");
        Param.insert("valueMarginB", margin);
    } else if (obj["objectName"].toString() == "combinedDataPhaseC") {
        Param.insert("objectName", "valueMarginC");
        Param.insert("valueMarginC", margin);
    }
    jsonDoc.setObject(Param);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    emit cmdmsg(raw_data);

    if (!db.open()) {
        qDebug() << "Error: Unable to open database" << db.lastError().text();
        return;
    }
    QSqlQuery query(db);
    QString selectSQL = "SELECT id FROM MarginSettingParameter WHERE PHASE = :phase";
    query.prepare(selectSQL);
    query.bindValue(":phase", phase);

    if (!query.exec()) {
        qDebug() << "Error: Unable to query database" << query.lastError().text();
        db.close();
        return;
    }

    if (query.next()) {
        int id = query.value(0).toInt();
        QString updateSQL = R"(
            UPDATE MarginSettingParameter
            SET margin = :margin, valueVoltage = :valueVoltage, focusIndex = :focusIndex
            WHERE id = :id
        )";
        query.prepare(updateSQL);
        query.bindValue(":margin", margin);
        query.bindValue(":valueVoltage", valueVoltage);
        query.bindValue(":focusIndex", focusIndex);
        query.bindValue(":id", id);

        if (!query.exec()) {
            qDebug() << "Error: Unable to update data" << query.lastError().text();
        } else {
            qDebug() << "Updated MarginSettingParameter for PHASE:" << phase;
        }
    } else {
        if (!query.exec()) {
            qDebug() << "Error: Unable to insert data" << query.lastError().text();
        } else {
            qDebug() << "Inserted new MarginSettingParameter for PHASE: " << phase;
        }
    }

    db.close();
    updateMargin();
}

void Database::updateMargin() {
    qDebug() << "Fetching margin data from database...";

    if (!db.isOpen()) {
        qDebug() << "Database updateMargin is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryString = "SELECT margin, valueVoltage, focusIndex, PHASE FROM MarginSettingParameter";

    if (!query.exec(queryString)) {
        qDebug() << "Failed to fetch data from MarginSettingParameter:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int margin = query.value("margin").toInt();
        double valueVoltage = query.value("valueVoltage").toDouble();
        int focusIndex = query.value("focusIndex").toInt();
        QString phase = query.value("PHASE").toString();

        QString updateMarginmessage = QString(
                                          "{"
                                          "\"objectName\": \"updateParameterMargin\","
                                          "\"margin\":%1,"
                                          "\"valueVoltage\":%2,"
                                          "\"focusIndex\":%3,"
                                          "\"PHASE\":\"%4\""
                                          "}")
                                          .arg(margin)
                                          .arg(valueVoltage)
                                          .arg(focusIndex)
                                          .arg(phase);

        qDebug() << "Generated JSON for update Margin:" << updateMarginmessage;
        emit cmdmsg(updateMarginmessage);
        if (phase == "A") {
            QString updateMarginmessageA = QString(
                                               "{"
                                               "\"objectName\":\"marginCountA\","
                                               "\"marginA\":%1,"
                                               "\"valueVoltage\":%2,"
                                               "\"focusIndex\":%3,"
                                               "\"PHASE\":\"%4\""
                                               "}")
                                               .arg(margin)
                                               .arg(valueVoltage)
                                               .arg(focusIndex)
                                               .arg(phase);

            configParemeterMarginA(updateMarginmessageA);
        }
        if (phase == "B") {
            QString updateMarginmessageB = QString(
                                               "{"
                                               "\"objectName\":\"marginCountB\","
                                               "\"marginB\":%1,"
                                               "\"valueVoltage\":%2,"
                                               "\"focusIndex\":%3,"
                                               "\"PHASE\":\"%4\""
                                               "}")
                                               .arg(margin)
                                               .arg(valueVoltage)
                                               .arg(focusIndex)
                                               .arg(phase);

            configParemeterMarginB(updateMarginmessageB);
        }
        if (phase == "C") {
            QString updateMarginmessageC = QString(
                                               "{"
                                               "\"objectName\":\"marginCountC\","
                                               "\"marginC\":%1,"
                                               "\"valueVoltage\":%2,"
                                               "\"focusIndex\":%3,"
                                               "\"PHASE\":\"%4\""
                                               "}")
                                               .arg(margin)
                                               .arg(valueVoltage)
                                               .arg(focusIndex)
                                               .arg(phase);

            configParemeterMarginC(updateMarginmessageC);
        }
    }
    db.close();
}

void Database::configParemeterMarginA(QString msg) {
    qDebug() << "configParemeterMarginA:" << msg;

    if (!db.isOpen() && !db.open()) {
        qDebug() << "Failed to open the database:" << db.lastError().text();
        return;
    }

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command["objectName"].toString();
    int numOfMarginA = command["marginA"].toInt();
    QString phase = command["PHASE"].toString();
    int focusIndex = command.contains("focusIndex") ? command["focusIndex"].toInt() : -1;
    MaxMarginA = numOfMarginA;
    if (phase == "A" && command.contains("marginA") && command.contains("PHASE")) {
        qDebug() << "Updating MarginSettingParameter with marginA: " << numOfMarginA;

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE MarginSettingParameter SET margin = :numOfMarginA WHERE PHASE = 'A'");
        updateQuery.bindValue(":numOfMarginA", numOfMarginA);

        if (!updateQuery.exec()) {
            qDebug() << "Error: Unable to update MarginSettingParameter" << updateQuery.lastError().text();
            db.close();
            return;
        }
        qDebug() << "Updated MarginSettingParameter for Phase A.";
    }

    if (phase == "A" && command.contains("focusIndex")) {
        qDebug() << "Updating focusIndex for MarginSettingParameter...";

        QSqlQuery updateFocusQuery(db);
        updateFocusQuery.prepare("UPDATE MarginSettingParameter SET focusIndex = :focusIndex WHERE PHASE = 'A'");
        updateFocusQuery.bindValue(":focusIndex", focusIndex);

        if (!updateFocusQuery.exec()) {
            qDebug() << "Error: Unable to update focusIndex in MarginSettingParameter" << updateFocusQuery.lastError().text();
            db.close();
            return;
        }
        qDebug() << "Updated focusIndex for Phase A to:" << focusIndex;
    }

    // if (getCommand.contains("marginCountA") && phase == "A") {
        qDebug() << "Fetching MarginA Data: " << numOfMarginA;

        QSqlQuery fetchQuery(db);
        fetchQuery.prepare("SELECT * FROM MarginTableA WHERE No <= :numOfMarginA ORDER BY No ASC");
        fetchQuery.bindValue(":numOfMarginA", numOfMarginA);

        if (!fetchQuery.exec()) {
            qDebug() << "Query execution failed:" << fetchQuery.lastError().text();
            db.close();
            return;
        }

        while (fetchQuery.next()) {
            int no = fetchQuery.value("No").toInt();
            QString marginNo = fetchQuery.value("Margin(No.)").toString();
            int valueOfMargin = fetchQuery.value("value of margin").toInt();
            QString unit = fetchQuery.value("unit").toString();

            QString singleMarginData = QString(
                                           "{\"objectName\":\"marginlistCountA\", "
                                           "\"no\":%1, "
                                           "\"marginNo\":\"%2\", "
                                           "\"valueOfMargin\":%3, "
                                           "\"maxmargin\":%4, "
                                           "\"unit\":\"%5\"}")
                                           .arg(no)
                                           .arg(marginNo)
                                           .arg(valueOfMargin)
                                           .arg(numOfMarginA)
                                           .arg(unit);

            qDebug() << "Sending single margin data:" << singleMarginData;
            emit cmdmsg(singleMarginData);
        }
    // }
    db.close();
    qDebug() << "Database closed successfully.";
}

void Database::configParemeterMarginB(QString msg) {
    qDebug() << "configParemeterMarginB:" << msg;

    if (!db.isOpen() && !db.open()) {
        qDebug() << "Failed to open the database:" << db.lastError().text();
        return;
    }

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command["objectName"].toString();
    int numOfMarginB = command["marginB"].toInt();
    QString phase = command["PHASE"].toString();
    int focusIndex = command.contains("focusIndex") ? command["focusIndex"].toInt() : -1;
    MaxMarginB = numOfMarginB;
    if (phase == "B" && command.contains("marginB") && command.contains("PHASE")) {
        qDebug() << "Updating MarginSettingParameter with marginB: " << numOfMarginB;

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE MarginSettingParameter SET margin = :numOfMarginB WHERE PHASE = 'B'");
        updateQuery.bindValue(":numOfMarginB", numOfMarginB);

        if (!updateQuery.exec()) {
            qDebug() << "Error: Unable to update MarginSettingParameter" << updateQuery.lastError().text();
            db.close();
            return;
        }
        qDebug() << "Updated MarginSettingParameter for Phase B.";
    }

    if (phase == "B" && command.contains("focusIndex")) {
        qDebug() << "Updating focusIndex for MarginSettingParameter...";

        QSqlQuery updateFocusQuery(db);
        updateFocusQuery.prepare("UPDATE MarginSettingParameter SET focusIndex = :focusIndex WHERE PHASE = 'B'");
        updateFocusQuery.bindValue(":focusIndex", focusIndex);

        if (!updateFocusQuery.exec()) {
            qDebug() << "Error: Unable to update focusIndex in MarginSettingParameter" << updateFocusQuery.lastError().text();
            db.close();
            return;
        }
        qDebug() << "Updated focusIndex for Phase B to:" << focusIndex;
    }

    // if (getCommand.contains("marginCountB") && phase == "B") {
        qDebug() << "Fetching MarginB Data: " << numOfMarginB;

        QSqlQuery fetchQuery(db);
        fetchQuery.prepare("SELECT * FROM MarginTableB WHERE No <= :numOfMarginB ORDER BY No ASC");
        fetchQuery.bindValue(":numOfMarginB", numOfMarginB);

        if (!fetchQuery.exec()) {
            qDebug() << "Query execution failed:" << fetchQuery.lastError().text();
            db.close();
            return;
        }

        while (fetchQuery.next()) {
            int no = fetchQuery.value("No").toInt();
            QString marginNo = fetchQuery.value("Margin(No.)").toString();
            int valueOfMargin = fetchQuery.value("value of margin").toInt();
            QString unit = fetchQuery.value("unit").toString();

            QString singleMarginData = QString(
                                           "{\"objectName\":\"marginlistCountB\", "
                                           "\"no\":%1, "
                                           "\"marginNo\":\"%2\", "
                                           "\"valueOfMargin\":%3, "
                                           "\"maxmargin\":%4, "
                                           "\"unit\":\"%5\"}")
                                           .arg(no)
                                           .arg(marginNo)
                                           .arg(valueOfMargin)
                                           .arg(numOfMarginB)
                                           .arg(unit);

            qDebug() << "Sending single margin data:" << singleMarginData;
            emit cmdmsg(singleMarginData);
        }
    // }
    db.close();
    qDebug() << "Database closed successfully.";
}

void Database::configParemeterMarginC(QString msg) {
    qDebug() << "configParemeterMarginC:" << msg;

    if (!db.isOpen() && !db.open()) {
        qDebug() << "Failed to open the database:" << db.lastError().text();
        return;
    }

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = command["objectName"].toString();
    int numOfMarginC = command["marginC"].toInt();
    QString phase = command["PHASE"].toString();
    int focusIndex = command.contains("focusIndex") ? command["focusIndex"].toInt() : -1;
    MaxMarginC = numOfMarginC;
    if (phase == "C" && command.contains("marginC") && command.contains("PHASE")) {
        qDebug() << "Updating MarginSettingParameter with marginC: " << numOfMarginC;

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE MarginSettingParameter SET margin = :numOfMarginC WHERE PHASE = 'C'");
        updateQuery.bindValue(":numOfMarginC", numOfMarginC);

        if (!updateQuery.exec()) {
            qDebug() << "Error: Unable to update MarginSettingParameter" << updateQuery.lastError().text();
            db.close();
            return;
        }
        qDebug() << "Updated MarginSettingParameter for Phase C.";
    }

    if (phase == "B" && command.contains("focusIndex")) {
        qDebug() << "Updating focusIndex for MarginSettingParameter...";

        QSqlQuery updateFocusQuery(db);
        updateFocusQuery.prepare("UPDATE MarginSettingParameter SET focusIndex = :focusIndex WHERE PHASE = 'C'");
        updateFocusQuery.bindValue(":focusIndex", focusIndex);

        if (!updateFocusQuery.exec()) {
            qDebug() << "Error: Unable to update focusIndex in MarginSettingParameter" << updateFocusQuery.lastError().text();
            db.close();
            return;
        }
        qDebug() << "Updated focusIndex for Phase C to:" << focusIndex;
    }

    // if (getCommand.contains("marginCountC") && phase == "C") {
        qDebug() << "Fetching MarginC Data: " << numOfMarginC;

        QSqlQuery fetchQuery(db);
        fetchQuery.prepare("SELECT * FROM MarginTableC WHERE No <= :numOfMarginC ORDER BY No ASC");
        fetchQuery.bindValue(":numOfMarginC", numOfMarginC);

        if (!fetchQuery.exec()) {
            qDebug() << "Query execution failed:" << fetchQuery.lastError().text();
            db.close();
            return;
        }

        while (fetchQuery.next()) {
            int no = fetchQuery.value("No").toInt();
            QString marginNo = fetchQuery.value("Margin(No.)").toString();
            int valueOfMargin = fetchQuery.value("value of margin").toInt();
            QString unit = fetchQuery.value("unit").toString();

            QString singleMarginData = QString(
                                           "{\"objectName\":\"marginlistCountC\", "
                                           "\"no\":%1, "
                                           "\"marginNo\":\"%2\", "
                                           "\"valueOfMargin\":%3, "
                                           "\"maxmargin\":%4, "
                                           "\"unit\":\"%5\"}")
                                           .arg(no)
                                           .arg(marginNo)
                                           .arg(valueOfMargin)
                                           .arg(numOfMarginC)
                                           .arg(unit);

            qDebug() << "Sending single margin data:" << singleMarginData;
            emit cmdmsg(singleMarginData);
        }
    // }
    db.close();
    qDebug() << "Database closed successfully.";
}
void Database::updataListOfMarginA(QString msg) {
//    qDebug() << "updataListOfMarginA:" << msg;

   if (!db.isOpen() && !db.open()) {
       qDebug() << "Database is not open. Attempting to open...";
       return;
   }

   QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
   QJsonObject obj = doc.object();

   int marginA = obj.contains("margin") ? obj["margin"].toInt() : obj["rangeofmargin"].toInt();
   int valueVoltageA = obj.contains("valueVoltage") ? obj["valueVoltage"].toInt() : obj["autoValueVoltage"].toInt();
   int focusIndex = obj.contains("focusIndex") ? obj["focusIndex"].toInt() : -1;

   qDebug() << "Processing MarginA:" << marginA << " ValueVoltageA:" << valueVoltageA << " FocusIndex:" << focusIndex;

   if (focusIndex > 0) {
       // ✅ **Update a specific row**
       int targetNo = focusIndex + 1;
       QSqlQuery updateQuery(db);
       updateQuery.prepare("UPDATE MarginTableA SET `value of margin` = :valueVoltageA WHERE No = :targetNo");
       updateQuery.bindValue(":valueVoltageA", valueVoltageA);
       updateQuery.bindValue(":targetNo", targetNo);

       if (!updateQuery.exec()) {
           qDebug() << "Error: Unable to update MarginTableA at No:" << targetNo << updateQuery.lastError().text();
           db.close();
           return;
       }
       qDebug() << "Updated MarginTableA at No:" << targetNo << " with ValueVoltageA:" << valueVoltageA;

   } else if (focusIndex == 0) {
       // ✅ **Update only first row (No = 1)**
       QSqlQuery updateFirstQuery(db);
       updateFirstQuery.prepare("UPDATE MarginTableA SET `value of margin` = :valueVoltageA WHERE No = 1");
       updateFirstQuery.bindValue(":valueVoltageA", valueVoltageA);

       if (!updateFirstQuery.exec()) {
           qDebug() << "Error: Unable to update MarginTableA at No 1" << updateFirstQuery.lastError().text();
           db.close();
           return;
       }
       qDebug() << "Updated **only** first row (No = 1) with ValueVoltageA:" << valueVoltageA;

   } else if (focusIndex == -1) {
       // ✅ Set all rows in MarginTableA
       QSqlQuery updateAllQuery(db);
       updateAllQuery.prepare("UPDATE MarginTableA SET `value of margin` = :valueVoltageA");
       updateAllQuery.bindValue(":valueVoltageA", valueVoltageA);

       if (!updateAllQuery.exec()) {
           qDebug() << "Error: Unable to update all MarginTableA rows"
                    << updateAllQuery.lastError().text();
           db.close();
           return;
       }

       qDebug() << "Updated all rows in MarginTableA with ValueVoltageA:" << valueVoltageA;
   }

   // ✅ **Retrieve updated rows**
   QSqlQuery selectQuery(db);
   selectQuery.prepare("SELECT No, `Margin(No.)`, `value of margin`, unit FROM MarginTableA WHERE No <= :marginA");
   selectQuery.bindValue(":marginA", marginA);

   if (!selectQuery.exec()) {
       qDebug() << "Error: Unable to retrieve MarginTableA data" << selectQuery.lastError().text();
       db.close();
       return;
   }

   // ✅ **Pack results into JSON**
   QJsonArray marginList;
   while (selectQuery.next()) {
       QJsonObject marginData;
       marginData["No"] = selectQuery.value(0).toInt();
       marginData["MarginName"] = selectQuery.value(1).toString();
       marginData["Value"] = selectQuery.value(2).toInt();
       marginData["Unit"] = selectQuery.value(3).toString();
       marginList.append(marginData);
       qDebug() << "marginList::" << marginData;
   }

   QJsonObject output;
   output["objectName"] = "MarginTableUpdated";
   output["Margins"] = marginList;

   QJsonDocument jsonDoc(output);
   QString jsonData = jsonDoc.toJson(QJsonDocument::Compact);

   qDebug() << "Formatted JSON sendUpdatedMarginList:" << jsonData;
   emit sendUpdatedMarginList(jsonData);

   // ✅ Close the database
   db.close();
   qDebug() << "Database closed successfully.";
}

void Database::updataListOfMarginB(QString msg) {
//    qDebug() << "updataListOfMarginB:" << msg;

   if (!db.isOpen() && !db.open()) {
       qDebug() << "Database is not open. Attempting to open...";
       return;
   }

   QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
   QJsonObject obj = doc.object();

   int marginB = obj.contains("margin") ? obj["margin"].toInt() : obj["rangeofmargin"].toInt();
   int valueVoltageB = obj.contains("valueVoltage") ? obj["valueVoltage"].toInt() : obj["autoValueVoltage"].toInt();
   int focusIndex = obj.contains("focusIndex") ? obj["focusIndex"].toInt() : -1;

   qDebug() << "Processing marginB:" << marginB << " ValueVoltageB:" << valueVoltageB << " FocusIndex:" << focusIndex;

   if (focusIndex > 0) {
       // ✅ **Update a specific row**
       int targetNo = focusIndex + 1;
       QSqlQuery updateQuery(db);
       updateQuery.prepare("UPDATE MarginTableB SET `value of margin` = :valueVoltageB WHERE No = :targetNo");
       updateQuery.bindValue(":valueVoltageB", valueVoltageB);
       updateQuery.bindValue(":targetNo", targetNo);

       if (!updateQuery.exec()) {
           qDebug() << "Error: Unable to update MarginTableB at No:" << targetNo << updateQuery.lastError().text();
           db.close();
           return;
       }
       qDebug() << "Updated MarginTableB at No:" << targetNo << " with ValueVoltageA:" << valueVoltageB;

   } else if (focusIndex == 0) {
       // ✅ **Update only first row (No = 1)**
       QSqlQuery updateFirstQuery(db);
       updateFirstQuery.prepare("UPDATE MarginTableB SET `value of margin` = :valueVoltageB WHERE No = 1");
       updateFirstQuery.bindValue(":valueVoltageB", valueVoltageB);

       if (!updateFirstQuery.exec()) {
           qDebug() << "Error: Unable to update MarginTableB at No 1" << updateFirstQuery.lastError().text();
           db.close();
           return;
       }
       qDebug() << "Updated **only** first row (No = 1) with valueVoltageB:" << valueVoltageB;

   } else if (focusIndex == -1) {
       // ✅ Set all rows in MarginTableB
       QSqlQuery updateAllQuery(db);
       updateAllQuery.prepare("UPDATE MarginTableB SET `value of margin` = :valueVoltageB");
       updateAllQuery.bindValue(":valueVoltageB", valueVoltageB);

       if (!updateAllQuery.exec()) {
           qDebug() << "Error: Unable to update all MarginTableB rows"
                    << updateAllQuery.lastError().text();
           db.close();
           return;
       }

       qDebug() << "Updated all rows in MarginTableB with ValueVoltageB:" << valueVoltageB;
   }

   // ✅ **Retrieve updated rows**
   QSqlQuery selectQuery(db);
   selectQuery.prepare("SELECT No, `Margin(No.)`, `value of margin`, unit FROM MarginTableB WHERE No <= :marginB");
   selectQuery.bindValue(":marginB", marginB);

   if (!selectQuery.exec()) {
       qDebug() << "Error: Unable to retrieve MarginTableB data" << selectQuery.lastError().text();
       db.close();
       return;
   }

   QJsonArray marginList;
   while (selectQuery.next()) {
       QJsonObject marginData;
       marginData["No"] = selectQuery.value(0).toInt();
       marginData["MarginName"] = selectQuery.value(1).toString();
       marginData["Value"] = selectQuery.value(2).toInt();
       marginData["Unit"] = selectQuery.value(3).toString();
       marginList.append(marginData);
   }

   QJsonObject output;
   output["objectName"] = "MarginTableUpdated";
   output["Margins"] = marginList;

   QJsonDocument jsonDoc(output);
   QString jsonData = jsonDoc.toJson(QJsonDocument::Compact);

   qDebug() << "Formatted JSON sendUpdatedMarginList:" << jsonData;
   emit sendUpdatedMarginList(jsonData);

   // ✅ Close the database
   db.close();
   qDebug() << "Database closed successfully.";
}

void Database::updataListOfMarginC(QString msg) {
//    qDebug() << "updataListOfMarginC:" << msg;

   if (!db.isOpen() && !db.open()) {
       qDebug() << "Database is not open. Attempting to open...";
       return;
   }

   QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
   QJsonObject obj = doc.object();

   int marginC = obj.contains("margin") ? obj["margin"].toInt() : obj["rangeofmargin"].toInt();
   int valueVoltageC = obj.contains("valueVoltage") ? obj["valueVoltage"].toInt() : obj["autoValueVoltage"].toInt();
   int focusIndex = obj.contains("focusIndex") ? obj["focusIndex"].toInt() : -1;

   qDebug() << "Processing marginC:" << marginC << " valueVoltageC:" << valueVoltageC << " FocusIndex:" << focusIndex;

   if (focusIndex > 0) {
       // ✅ **Update a specific row**
       int targetNo = focusIndex + 1;
       QSqlQuery updateQuery(db);
       updateQuery.prepare("UPDATE MarginTableC SET `value of margin` = :valueVoltageC WHERE No = :targetNo");
       updateQuery.bindValue(":valueVoltageC", valueVoltageC);
       updateQuery.bindValue(":targetNo", targetNo);

       if (!updateQuery.exec()) {
           qDebug() << "Error: Unable to update MarginTableC at No:" << targetNo << updateQuery.lastError().text();
           db.close();
           return;
       }
       qDebug() << "Updated MarginTableC at No:" << targetNo << " with ValueVoltageA:" << valueVoltageC;

   } else if (focusIndex == 0) {
       // ✅ **Update only first row (No = 1)**
       QSqlQuery updateFirstQuery(db);
       updateFirstQuery.prepare("UPDATE MarginTableC SET `value of margin` = :valueVoltageC WHERE No = 1");
       updateFirstQuery.bindValue(":valueVoltageC", valueVoltageC);

       if (!updateFirstQuery.exec()) {
           qDebug() << "Error: Unable to update MarginTableC at No 1" << updateFirstQuery.lastError().text();
           db.close();
           return;
       }
       qDebug() << "Updated **only** first row (No = 1) with valueVoltageC:" << valueVoltageC;

   } else if (focusIndex == -1) {
       // ✅ Set all rows in MarginTableC
       QSqlQuery updateAllQuery(db);
       updateAllQuery.prepare("UPDATE MarginTableC SET `value of margin` = :valueVoltageC");
       updateAllQuery.bindValue(":valueVoltageC", valueVoltageC);

       if (!updateAllQuery.exec()) {
           qDebug() << "Error: Unable to update all MarginTableC rows"
                    << updateAllQuery.lastError().text();
           db.close();
           return;
       }

       qDebug() << "Updated all rows in MarginTableA with ValueVoltageC:" << valueVoltageC;
   }

   QSqlQuery selectQuery(db);
   selectQuery.prepare("SELECT No, `Margin(No.)`, `value of margin`, unit FROM MarginTableC WHERE No <= :marginC");
   selectQuery.bindValue(":marginC", marginC);

   if (!selectQuery.exec()) {
       qDebug() << "Error: Unable to retrieve MarginTableC data" << selectQuery.lastError().text();
       db.close();
       return;
   }

   QJsonArray marginList;
   while (selectQuery.next()) {
       QJsonObject marginData;
       marginData["No"] = selectQuery.value(0).toInt();
       marginData["MarginName"] = selectQuery.value(1).toString();
       marginData["Value"] = selectQuery.value(2).toInt();
       marginData["Unit"] = selectQuery.value(3).toString();
       marginList.append(marginData);
   }

   QJsonObject output;
   output["objectName"] = "MarginTableUpdated";
   output["Margins"] = marginList;

   QJsonDocument jsonDoc(output);
   QString jsonData = jsonDoc.toJson(QJsonDocument::Compact);

   qDebug() << "Formatted JSON sendUpdatedMarginList:" << jsonData;
   emit sendUpdatedMarginList(jsonData);

   // ✅ Close the database
   db.close();
   qDebug() << "Database closed successfully.";
}

void Database::getMyTaggingPhaseA() {
    qDebug() << "getMyTaggingPhase:";
    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            db.close();
            return;
        }
    }

    QSqlQuery query;

    if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'A'")) {
        while (query.next()) {
            QVariant statusVar = query.value("status");
            QVariant numListVar = query.value("No");
            QVariant tempNoVar = query.value("temp_no");
            QVariant distanceVar = query.value("Distance(Km)");
            QVariant detailVar = query.value("Detail");
            QVariant phaseVar = query.value("Phase");

            bool status = statusVar.toInt() != 0;
            int num_list = numListVar.toInt();
            int temp_no = tempNoVar.toInt();
            double Distance = distanceVar.toDouble();
            QString Detail = detailVar.toString();
            QString Phase = phaseVar.toString();

            // Debug ค่าของตัวแปร
            qDebug() << "Debug Variables and Types:";
            qDebug() << "status (as bool):" << (status ? "true" : "false") << "type: bool";
            qDebug() << "num_list:" << num_list << "type:" << numListVar.typeName();
            qDebug() << "temp_no:" << temp_no << "type:" << tempNoVar.typeName();
            qDebug() << "Distance:" << Distance << "type:" << distanceVar.typeName();
            qDebug() << "Detail:" << Detail << "type:" << detailVar.typeName();
            qDebug() << "Phase:" << Phase << "type:" << phaseVar.typeName();

            if (Phase.isEmpty() || Detail.isEmpty()) {
                qWarning() << "Warning: Empty Phase or Detail. Skipping this row.";
                continue;
            }

            QString message = QString(
                                  "{\"objectName\"  :\"getDataTaggingPhaseA\", "
                                  "\"status\"       :%1, "
                                  "\"num_list\"     :%2, "
                                  "\"temp_no\"      :%3, "
                                  "\"Distance\"     :\"%4\", "
                                  "\"Detail\"       :\"%5\", "
                                  "\"Phase\"        :\"%6\"}")
                                  .arg(status ? "true" : "false")
                                  .arg(num_list)
                                  .arg(temp_no)
                                  .arg(Distance)
                                  .arg(Detail)
                                  .arg(Phase);

            qDebug() << "Sent message get PhaseA:" << message;
            emit cmdmsg(message);
        }
    } else {
        qDebug() << "Failed to execute query:" << query.lastError().text();
    }
    closeMySQL();
}

void Database::getMyTaggingPhaseB() {
    qDebug() << "getMyTaggingPhaseB:";
    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            db.close();
            return;
        }
    }

    QSqlQuery query;

    if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'B'")) {
        while (query.next()) {
            QVariant statusVar = query.value("status");
            QVariant numListVar = query.value("No");
            QVariant tempNoVar = query.value("temp_no");
            QVariant distanceVar = query.value("Distance(Km)");
            QVariant detailVar = query.value("Detail");
            QVariant phaseVar = query.value("Phase");

            bool status = statusVar.toInt() != 0;
            int num_list = numListVar.toInt();
            int temp_no = tempNoVar.toInt();
            double Distance = distanceVar.toDouble();
            QString Detail = detailVar.toString();
            QString Phase = phaseVar.toString();

            // Debug ค่าของตัวแปร
            qDebug() << "Debug Variables and Types:";
            qDebug() << "status (as bool):" << (status ? "true" : "false") << "type: bool";
            qDebug() << "num_list:" << num_list << "type:" << numListVar.typeName();
            qDebug() << "temp_no:" << temp_no << "type:" << tempNoVar.typeName();
            qDebug() << "Distance:" << Distance << "type:" << distanceVar.typeName();
            qDebug() << "Detail:" << Detail << "type:" << detailVar.typeName();
            qDebug() << "Phase:" << Phase << "type:" << phaseVar.typeName();

            if (Phase.isEmpty() || Detail.isEmpty()) {
                qWarning() << "Warning: Empty Phase or Detail. Skipping this row.";
                continue;
            }

            QString message = QString(
                                  "{\"objectName\"  :\"getDataTaggingPhaseB\", "
                                  "\"status\"       :%1, "
                                  "\"num_list\"     :%2, "
                                  "\"temp_no\"      :%3, "
                                  "\"Distance\"     :\"%4\", "
                                  "\"Detail\"       :\"%5\", "
                                  "\"Phase\"        :\"%6\"}")
                                  .arg(status ? "true" : "false")
                                  .arg(num_list)
                                  .arg(temp_no)
                                  .arg(Distance)
                                  .arg(Detail)
                                  .arg(Phase);
            qDebug() << "Sent message get PhaseB:" << message;
            emit cmdmsg(message);
        }
    } else {
        qDebug() << "Failed to execute query:" << query.lastError().text();
    }
    closeMySQL();
}

void Database::getMyTaggingPhaseC() {
    qDebug() << "getMyTaggingPhase:";
    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        db.close();
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            db.close();
            return;
        }
    }

    QSqlQuery query;

    if (query.exec("SELECT * FROM DataTagging WHERE Phase = 'C'")) {
        while (query.next()) {
            QVariant statusVar = query.value("status");
            QVariant numListVar = query.value("No");
            QVariant tempNoVar = query.value("temp_no");
            QVariant distanceVar = query.value("Distance(Km)");
            QVariant detailVar = query.value("Detail");
            QVariant phaseVar = query.value("Phase");

            bool status = statusVar.toInt() != 0;
            int num_list = numListVar.toInt();
            int temp_no = tempNoVar.toInt();
            double Distance = distanceVar.toDouble();
            QString Detail = detailVar.toString();
            QString Phase = phaseVar.toString();

            // Debug ค่าของตัวแปร
            qDebug() << "Debug Variables and Types:";
            qDebug() << "status (as bool):" << (status ? "true" : "false") << "type: bool";
            qDebug() << "num_list:" << num_list << "type:" << numListVar.typeName();
            qDebug() << "temp_no:" << temp_no << "type:" << tempNoVar.typeName();
            qDebug() << "Distance:" << Distance << "type:" << distanceVar.typeName();
            qDebug() << "Detail:" << Detail << "type:" << detailVar.typeName();
            qDebug() << "Phase:" << Phase << "type:" << phaseVar.typeName();

            if (Phase.isEmpty() || Detail.isEmpty()) {
                qWarning() << "Warning: Empty Phase or Detail. Skipping this row.";
                continue;
            }

            QString message = QString(
                                  "{\"objectName\"  :\"getDataTaggingPhaseC\", "
                                  "\"status\"       :%1, "
                                  "\"num_list\"     :%2, "
                                  "\"temp_no\"      :%3, "
                                  "\"Distance\"     :\"%4\", "
                                  "\"Detail\"       :\"%5\", "
                                  "\"Phase\"        :\"%6\"}")
                                  .arg(status ? "true" : "false")
                                  .arg(num_list)
                                  .arg(temp_no)
                                  .arg(Distance)
                                  .arg(Detail)
                                  .arg(Phase);
            qDebug() << "Sent message get PhaseB:" << message;
            emit cmdmsg(message);
        }
    } else {
        qDebug() << "Failed to execute query:" << query.lastError().text();
    }
    closeMySQL();
}

void Database::updateTaggingPoint()
{
    qDebug() << "[updateTaggingPoint] Reset all DataTagging status to 0";

    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "[updateTaggingPoint] Database open error:"
                     << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);

    query.prepare("UPDATE DataTagging SET status = 0");

    if (!query.exec()) {
        qDebug() << "[updateTaggingPoint] Update query failed:"
                 << query.lastError().text();
    } else {
        qDebug() << "[updateTaggingPoint] Rows affected:"
                 << query.numRowsAffected();
    }

    db.close();
    qDebug() << "[updateTaggingPoint] Database closed after reset.";

    fetchTaggingData();
}

void Database::updateTaggingPoint(QString msg) {
    qDebug() << "updateTaggingPoint_message:" << msg;

    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    if (!doc.isObject()) {
        qDebug() << "Invalid JSON message:" << msg;
        return;
    }

    QJsonObject jsonObj = doc.object();

    if (jsonObj.value("objectName").toString() != "selectTaggingPoint") {
        qDebug() << "Unexpected objectName:" << jsonObj.value("objectName").toString();
        return;
    }

    bool selectFlag = jsonObj.value("select").toBool();
    int taggingPointID = jsonObj.value("taggingPointID").toInt();
    QString phase = jsonObj.value("Phase").toString();

    qDebug() << "Updating DataTagging -> selectFlag:" << selectFlag << ", taggingPointID:" << taggingPointID << ", Phase:" << phase;

    if (!db.open()) {
        qDebug() << "Database open error:" << db.lastError().text();
        return;
    }

    int statusValue = selectFlag ? 1 : 0;
    QSqlQuery query(db);
    query.prepare("UPDATE DataTagging SET status = :status WHERE Phase = :phase AND temp_no = :temp_no");
    query.bindValue(":status", statusValue);
    query.bindValue(":phase", phase);
    query.bindValue(":temp_no", taggingPointID);

    if (!query.exec()) {
        qDebug() << "Update query failed:" << query.lastError().text();
    } else {
        qDebug() << "Rows affected by UPDATE:" << query.numRowsAffected();
        if (query.numRowsAffected() == 0) {
            qDebug() << "Warning: No rows were updated. Check Phase and temp_no values!";
        }
    }

    db.close();
    qDebug() << "Database closed after updating.";
    fetchTaggingData();
}

void Database::fetchTaggingData() {
    qDebug() << "Fetching Tagging Data...";

    if (!db.isOpen()) {
        qDebug() << "Database is not open, opening now...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery checkTableQuery(db);
    QString checkTableSQL = QString(
                                "SELECT COUNT(*) FROM information_schema.tables "
                                "WHERE table_schema = '%1' AND table_name = 'DataTagging'")
                                .arg(db.databaseName());

    if (!checkTableQuery.exec(checkTableSQL)) {
        qDebug() << "Failed to check if table exists:" << checkTableQuery.lastError().text();
        return;
    }

    if (checkTableQuery.next() && checkTableQuery.value(0).toInt() > 0) {
        qDebug() << "Table `DataTagging` exists. Fetching data...";

        QSqlQuery query(db);
        if (!query.exec("SELECT status, No, `Distance(Km)`, Detail, Phase, temp_no FROM DataTagging")) {
            qDebug() << "Query execution failed:" << query.lastError().text();
            return;
        }

        QJsonArray rows;
        while (query.next()) {
            QJsonObject rowObj;

            QString statusStr = query.value("status").toString();
            QString noStr = query.value("No").toString();
            QString distanceStr = query.value("Distance(Km)").toString();
            QString detailStr = query.value("Detail").toString();
            QString phaseStr = query.value("Phase").toString();
            QString tempNoStr = query.value("temp_no").toString();

            bool statusOk, noOk, distanceOk, tempNoOk;
            int status = statusStr.toInt(&statusOk);
            int no = noStr.toInt(&noOk);
            double distance = distanceStr.toDouble(&distanceOk);
            int tempNo = tempNoStr.toInt(&tempNoOk);

            if (!statusOk) status = 0;
            if (!noOk) no = 0;
            if (!distanceOk) distance = 0.0;
            if (!tempNoOk) tempNo = 0;

            rowObj["status"] = status;
            rowObj["No"] = no;
            rowObj["Distance"] = distance;
            rowObj["Detail"] = detailStr;
            rowObj["Phase"] = phaseStr;
            rowObj["temp_no"] = tempNo;

            qDebug() << "Fetched row:" << status << no << distance << detailStr << phaseStr << tempNo;

            rows.append(rowObj);
        }

        closeMySQL();
        qDebug() << "Database closed after fetching.";

        QJsonObject finalObj;
        finalObj["objectName"] = "updateTaggingData";
        finalObj["data"] = rows;

        QString pointData = QJsonDocument(finalObj).toJson(QJsonDocument::Compact);
        qDebug() << "Fetched Tagging Data:" << pointData;
        cmdmsg(pointData);

    } else {
        qDebug() << "Table `DataTagging` does not exist.";
    }

    closeMySQL();
}

void Database::getLFL() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }
    // เตรียมคำสั่ง SQL สำหรับดึงข้อมูล
    QSqlQuery query;
    QString sql = "SELECT * FROM OperateTable";
    QString getLFL_Fail;
    QString getLFL_Operate;
    if (!query.exec(sql)) {
        qDebug() << "Failed to execute query:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        LFL_Fail = query.value("LFL_Fail").toBool();
        LFL_Operate = query.value("LFL_Operate").toBool();
        //        getLFL_Fail = QString("{"
        //                          "\"objectName\"  :\"statusFail\","
        //                          "\"LFLFAIL\"  :\"%1\""
        //                          "}").arg(LFL_Fail);
        //        getLFL_Operate = QString("{"
        //                          "\"objectName\"  :\"statusOperate\","
        //                          "\"LFLOPERATE\"  :\"%1\""
        //                          "}").arg(LFL_Operate);
        qDebug() << "sDEBUG GetSettingDisplay" << "LFL_Fail:" << LFL_Fail << "LFL_Operate:" << LFL_Operate;
    }
    //    qDebug() << getLFL_Fail;
    //    qDebug() << getLFL_Operate;
    //    emit updateThresholdA(getLFL_Fail);
    //    emit updateThresholdA(getLFL_Operate);
    closeMySQL();
}

void Database::getPositionDistance(QString msg) {
    qDebug() << "getPositionDistance:" << msg;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("distance")) {
        QString distance = QJsonValue(command["distance"]).toString();
        QString cutsor = QString(
                             "{\"objectName\"          :\"positonCursor\","
                             "\"distance\"        :\"%1\""
                             "}")
                             .arg(distance);
        qDebug() << "cursorPosition:" << cutsor;
        cmdmsg(cutsor);
    }
}

void Database::controlCursor(QString msg) {
    qDebug() << "controlCursor:" << msg;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();
    if (getCommand.contains("decreaseValue")) {
        double leftmove = QJsonValue(command["decreaseValue"]).toDouble();
        qDebug() << "cursorPosition:" << leftmove;
        QString cutsorleftMove = QString(
                                     "{\"objectName\"          :\"decreaseValue\","
                                     "\"decreaseValue\"        :%1"
                                     "}")
                                     .arg(leftmove);
        qDebug() << "cutsorleftMove:" << cutsorleftMove;
        cmdmsg(cutsorleftMove);
    } else if (getCommand.contains("increaseValue")) {
        double rigth = QJsonValue(command["increaseValue"]).toDouble();
        qDebug() << "cursorPosition:" << rigth;
        QString cutsorrightMove = QString(
                                      "{\"objectName\"          :\"increaseValue\","
                                      "\"increaseValue\"        :%1"
                                      "}")
                                      .arg(rigth);
        qDebug() << "cutsorrigthMove:" << cutsorrightMove;
        cmdmsg(cutsorrightMove);
    }
}

void Database::getChangeDistance(QString msg) {
    qDebug() << "getChangeDistance:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();

    QString getCommand = QJsonValue(command["objectName"]).toString();
    double rangeDistance = QJsonValue(command["rangedistance"]).toDouble();

    if (getCommand.contains("rangedistance")) {
        qDebug() << "Received rangedistance value:" << rangeDistance;

        if (!db.isOpen()) {
            qDebug() << "Database not open, attempting to open...";
            if (!db.open()) {
                qWarning() << "Failed to open database:" << db.lastError().text();
                return;
            }
            qDebug() << "Database connection opened successfully.";
        }

        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT MAX(AutoNumber) FROM DistanceData");

        if (!checkQuery.exec()) {
            qWarning() << "Failed to execute checkQuery:" << checkQuery.lastError().text();
            return;
        }

        if (checkQuery.next()) {
            QVariant maxAutoNumber = checkQuery.value(0);

            if (!maxAutoNumber.isNull()) {
                int recordAutoNumber = maxAutoNumber.toInt();
                qDebug() << "Existing record found with AutoNumber:" << recordAutoNumber;

                QSqlQuery updateQuery;
                updateQuery.prepare(
                    "UPDATE DistanceData "
                    "SET DistanceKm = :distance "
                    "WHERE AutoNumber = :autoNumber");
                updateQuery.bindValue(":distance", rangeDistance);
                updateQuery.bindValue(":autoNumber", recordAutoNumber);

                if (updateQuery.exec()) {
                    qDebug() << "Record with AutoNumber =" << recordAutoNumber << "updated successfully with new DistanceKm =" << rangeDistance;
                } else {
                    qWarning() << "Failed to update the record:" << updateQuery.lastError().text();
                }
            } else {
                qDebug() << "No records found in DistanceData table.";
            }
        } else {
            qWarning() << "Failed to retrieve records from DistanceData table.";
        }
    } else {
        qDebug() << "Invalid command received in message.";
    }
    if (db.isOpen()) {
        db.close();
        qDebug() << "Database connection closed.";
    }
    emit updateDistance(rangeDistance);
}

void Database::updateDistance(double updatedistance) {
    qDebug() << "Taking data from table..." << updatedistance;

    QString newCursor = QString(
                            "{\"objectName\"          :\"updateCursor\","
                            "\"distance\"        :\"%1\""
                            "}")
                            .arg(updatedistance);
    qDebug() << "updateDistance:" << newCursor;
    emit cmdmsg(newCursor);
}

void Database::taggingpoint(QString msg) {
    qDebug() << "taggingpoint:" << msg;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();
    int taggingPoint = QJsonValue(command["checklist"]).toInt();
    bool statuslist = QJsonValue(command["statuslist"]).toBool();
    qDebug() << "tagging data:" << taggingPoint << statuslist;

    updataStatusTagging(taggingPoint, statuslist);

    if (getCommand.contains("taggingdata")) {
        qDebug() << "Processing taggingdata command with taggingpoint:" << taggingPoint;

        if (!db.isOpen()) {
            qDebug() << "Database not open, attempting to open...";
            if (!db.open()) {
                qWarning() << "Failed to open database:" << db.lastError().text();
                return;
            }
            qDebug() << "Database connection opened successfully.";
        }

        // ดึงข้อมูลที่อัปเดตแล้ว
        QString checkQueryStr = QString("SELECT * FROM DataTagging WHERE No = '%1'").arg(taggingPoint);
        QSqlQuery query;

        if (query.exec(checkQueryStr)) {
            if (query.next()) {
                QVariant statusVar = query.value("status");
                QVariant numListVar = query.value("No");
                QVariant tempNoVar = query.value("temp_no");
                QVariant distanceVar = query.value("Distance(Km)");
                QVariant detailVar = query.value("Detail");
                QVariant phaseVar = query.value("Phase");

                bool status = statusVar.toInt() != 0;
                int num_list = numListVar.toInt();
                int temp_no = tempNoVar.toInt();
                double Distance = distanceVar.toDouble();
                QString Detail = detailVar.toString();
                QString Phase = phaseVar.toString();

                QString message = QString(
                                      "{\"objectName\"  :\"taggingdata\", "
                                      "\"status\"       :%1, "
                                      "\"num_list\"     :%2, "
                                      "\"temp_no\"      :%3, "
                                      "\"Distance\"     :\"%4\", "
                                      "\"Detail\"       :\"%5\", "
                                      "\"Phase\"        :\"%6\"}")
                                      .arg(status ? "true" : "false")
                                      .arg(num_list)
                                      .arg(temp_no)
                                      .arg(Distance, 0, 'f', 2)  // format Distance to 2 decimal places
                                      .arg(Detail)
                                      .arg(Phase);
                qDebug() << "Sent message get PhaseA:" << message;

                emit cmdmsg(message);
            } else {
                qDebug() << "No record found with No =" << taggingPoint;
            }
        } else {
            qWarning() << "Failed to execute query:" << query.lastError().text();
        }
    } else {
        qDebug() << "Unknown command received.";
    }

    if (db.isOpen()) {
        db.close();
        qDebug() << "Database is open. Closing all connections...";
    }
}

void Database::updataStatusTagging(int taggingPoint, bool status) {
    qDebug() << "updataStatusTagging: taggingPoint =" << taggingPoint << ", status =" << status;

    if (!db.isOpen()) {
        qDebug() << "Database not open, attempting to open...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
        qDebug() << "Database connection opened successfully.";
    }

    QSqlQuery query;
    QString updateQueryStr = "UPDATE DataTagging SET status = :status WHERE No = :No";
    query.prepare(updateQueryStr);
    query.bindValue(":status", status ? 1 : 0);  // Convert bool to int (true = 1, false = 0)
    query.bindValue(":No", taggingPoint);

    if (!query.exec()) {
        qWarning() << "Failed to update status:" << query.lastError().text();
    } else {
        qDebug() << "Successfully updated status for No =" << taggingPoint;
    }
}

void Database::cleanDataInGraph(QString msg) {
    qDebug() << "cleanDataInGraph:" << msg;
    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();

    if (getCommand.contains("clearpatternPhaseA")) {
        qDebug() << "Clearing data for Phase A";
        QVector<double> voltageDataA;
        QString rawDataString = "[]";

        QString rawdataArray = QString(
                                   "{\"objectName\"  :\"clearpatternPhaseA\","
                                   "\"dataA\"         :%1}")
                                   .arg(rawDataString);

        emit cmdmsg(rawdataArray);
    } else if (getCommand.contains("clearpatternPhaseB")) {
        qDebug() << "Clearing data for Phase B";
        QVector<double> voltageDataB;
        QString rawDataString = "[]";

        QString rawdataArray = QString(
                                   "{\"objectName\"  :\"clearpatternPhaseB\","
                                   "\"dataB\"         :%1}")
                                   .arg(rawDataString);

        emit cmdmsg(rawdataArray);
    } else if (getCommand.contains("clearpatternPhaseC")) {
        qDebug() << "Clearing data for Phase C";
        QVector<double> voltageDataC;
        QString rawDataString = "[]";

        QString rawdataArray = QString(
                                   "{\"objectName\"  :\"clearpatternPhaseC\","
                                   "\"dataC\"         :%1}")
                                   .arg(rawDataString);

        emit cmdmsg(rawdataArray);
    } else if (getCommand.contains("clearDatabuttonPhaseA")) {
        qDebug() << "Clearing data for Phase A";
        QVector<double> voltageDataA;
        QString rawDataString = "[]";

        QString rawdataArray = QString(
                                   "{\"objectName\"  :\"clearGraphDataPhaseA\","
                                   "\"cleardataA\"         :%1}")
                                   .arg(rawDataString);

        qDebug() << "Clearing Pattern for Phase A" << rawdataArray;
        emit cmdmsg(rawdataArray);
    } else if (getCommand.contains("clearDatabuttonPhaseB")) {
        qDebug() << "Clearing data for Phase B";
        QVector<double> voltageDataB;
        QString rawDataString = "[]";

        QString rawdataArray = QString(
                                   "{\"objectName\"  :\"clearGraphDataPhaseB\","
                                   "\"cleardataB\"         :%1}")
                                   .arg(rawDataString);

        qDebug() << "Clearing Pattern for Phase B" << rawdataArray;
        emit cmdmsg(rawdataArray);
    } else if (getCommand.contains("clearDatabuttonPhaseC")) {
        qDebug() << "Clearing data for Phase C";
        QVector<double> voltageDataC;
        QString rawDataString = "[]";

        QString rawdataArray = QString(
                                   "{\"objectName\"  :\"clearGraphDataPhaseC\","
                                   "\"cleardataC\"         :%1}")
                                   .arg(rawDataString);

        qDebug() << "Clearing Pattern for Phase C" << rawdataArray;
        emit cmdmsg(rawdataArray);
    } else {
        qWarning() << "Unknown phase: " << msg;
    }
}

void Database::SettingDisplay(QString msg) {
    qDebug() << "SettingDisplay:" << msg;

    QJsonDocument d = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject command = d.object();
    QString getCommand = QJsonValue(command["objectName"]).toString();

    if (getCommand != "displaySetting") {
        qDebug() << "Invalid command. Expected 'displaySetting'.";
        return;
    }

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }
    qDebug() << "Database connection successful";

    QSqlQuery query(db);

    if (command.contains("sagFactor")) {
        QJsonValue sagValue = command["sagFactor"];

        if (!sagValue.isNull() && !sagValue.isUndefined()) {
            double sagFactor = sagValue.toDouble();  // แปลงเป็น double
            qDebug() << "Received sagFactor:" << sagFactor;

            if (sagFactor != 0) {
                qDebug() << "Updating database with sagFactor:" << sagFactor;
                query.prepare("UPDATE DisplaySetting SET SAG = :sagFactor WHERE number = 1");
                query.bindValue(":sagFactor", sagFactor);
                if (!query.exec()) {
                    qDebug() << "Failed to update SAG in DisplaySetting table:" << query.lastError().text();
                } else {
                    qDebug() << "SAG updated successfully to" << sagFactor;
                }
            } else {
                qDebug() << "Received sagFactor is 0. Updating database anyway.";
                query.prepare("UPDATE DisplaySetting SET SAG = 0 WHERE number = 1");
                if (!query.exec()) {
                    qDebug() << "Failed to update SAG in DisplaySetting table:" << query.lastError().text();
                } else {
                    qDebug() << "SAG updated successfully to 0.";
                }
            }
        } else {
            qDebug() << "sagFactor is null or undefined. Skipping update.";
        }
    }

    if (command.contains("samplingRate")) {
        QJsonValue samplingRate = command["samplingRate"];

        if (!samplingRate.isNull() && !samplingRate.isUndefined()) {
            double sampling = samplingRate.toDouble();  // แปลงเป็น double
            qDebug() << "Received samplingRate:" << sampling;

            if (sampling != 0) {
                qDebug() << "Updating database with sagFactor:" << sampling;
                query.prepare("UPDATE DisplaySetting SET Sampling_rate = :samplingRate WHERE number = 1");
                query.bindValue(":samplingRate", sampling);
                if (!query.exec()) {
                    qDebug() << "Failed to update samplingRate in DisplaySetting table:" << query.lastError().text();
                } else {
                    qDebug() << "samplingRate updated successfully to" << sampling;
                }
            } else {
                qDebug() << "Received samplingRate is 0. Updating database anyway.";
                query.prepare("UPDATE DisplaySetting SET Sampling_rate = 0 WHERE number = 1");
                if (!query.exec()) {
                    qDebug() << "Failed to update Sampling_rate in DisplaySetting table:" << query.lastError().text();
                } else {
                    qDebug() << "Sampling_rate updated successfully to 0.";
                }
            }
        } else {
            qDebug() << "sagFactor is null or undefined. Skipping update.";
        }
    }

    if (command.contains("distancetostartText")) {
        QJsonValue distanceStartValue = command["distancetostartText"];

        if (!distanceStartValue.isNull() && !distanceStartValue.isUndefined()) {
            int distanceStart = distanceStartValue.toInt();
            qDebug() << "Updating DistanceStart to:" << distanceStart;

            query.prepare("UPDATE DisplaySetting SET DistanceStart = :distanceStart WHERE number = 1");
            query.bindValue(":distanceStart", distanceStart);

            if (!query.exec()) {
                qDebug() << "Failed to update DistanceStart in DisplaySetting table:" << query.lastError().text();
            } else {
                qDebug() << "DistanceStart updated successfully to" << distanceStart;
            }
        } else {
            qDebug() << "distancetostartText is null or undefined. Skipping update.";
        }
    }

    if (command.contains("distancetoshowText")) {
        QJsonValue distanceStopValue = command["distancetoshowText"];

        if (!distanceStopValue.isNull() && !distanceStopValue.isUndefined()) {
            double distanceStop = distanceStopValue.toDouble();
            qDebug() << "Updating DistanceStop to:" << distanceStop;

            query.prepare("UPDATE DisplaySetting SET DistanceStop = :distanceStop WHERE number = 1");
            query.bindValue(":distanceStop", distanceStop);

            if (!query.exec()) {
                qDebug() << "Failed to update DistanceStop in DisplaySetting table:" << query.lastError().text();
            } else {
                qDebug() << "DistanceStop updated successfully to" << distanceStop;
            }
        } else {
            qDebug() << "distancetoshowText is null or undefined. Skipping update.";
        }
    }

    if (command.contains("fulldistanceText")) {
        QJsonValue fullDistanceValue = command["fulldistanceText"];

        if (!fullDistanceValue.isNull() && !fullDistanceValue.isUndefined()) {
            double fullDistance = fullDistanceValue.toDouble();
            qDebug() << "Updating FullDistance to:" << fullDistance;

            query.prepare("UPDATE DisplaySetting SET FullDistance = :fullDistance WHERE number = 1");
            query.bindValue(":fullDistance", fullDistance);

            if (!query.exec()) {
                qDebug() << "Failed to update FullDistance in DisplaySetting table:" << query.lastError().text();
            } else {
                qDebug() << "FullDistance updated successfully to" << fullDistance;
            }
            QString sql = R"(
                UPDATE TransmissionData
                SET FullDistance = :fullDistance
            )";

            QSqlQuery query(db);
            query.prepare(sql);
            query.bindValue(":fullDistance", fullDistance);

            if (!query.exec()) {
                qDebug() << "Failed to update FullDistance for all rows:" << query.lastError().text();
                return;
            }

        } else {
            qDebug() << "fulldistanceText is null or undefined. Skipping update.";
        }



    }

    closeMySQL();  // Close the database connection if necessary
    GetSettingDisplay();
}

void Database::GetSag() {
    qDebug() << "GetSag:";

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
        qDebug() << "Database opened successfully.";
    }

    // เตรียมคำสั่ง SQL สำหรับดึงข้อมูล
    QSqlQuery query;
    QString sql = "SELECT SAG, Sampling_rate, DistanceStart, DistanceStop, FullDistance FROM DisplaySetting";
    QString getPeriodicInfo;
    if (!query.exec(sql)) {
        qDebug() << "Failed to execute query:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        double SAG = query.value("SAG").toDouble();
        int Sampling_rate = query.value("Sampling_rate").toInt();
        double DistanceStart = query.value("DistanceStart").toDouble();
        double DistanceStop = query.value("DistanceStop").toDouble();
        double FullDistance = query.value("FullDistance").toDouble();

        qDebug() << "DEBUG GetSettingDisplay" << "SAG:" << SAG << "Sampling_rate:" << Sampling_rate << "DistanceStart:" << DistanceStart << "DistanceStop:" << DistanceStop << "FullDistance:" << FullDistance;

        getPeriodicInfo = QString(
                              "{\"objectName\":\"GetSettingDisplay\", "
                              "\"sagFactorInit\":%1, "
                              "\"samplingRateInit\":%2, "
                              "\"distanceToStartInit\":%3, "
                              "\"distanceToShowInit\":%4, "
                              "\"fulldistancesInit\":%5}")
                              .arg(SAG)
                              .arg(Sampling_rate)
                              .arg(DistanceStart)
                              .arg(DistanceStop)
                              .arg(FullDistance);

        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("objectName", "velocity_factor");
        Param.insert("value", SAG);
        jsonDoc.setObject(Param);
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToFPGA(raw_data);

        QJsonObject().swap(Param);
        Param.insert("objectName", "cableLength");
        Param.insert("value", FullDistance);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToFPGA(raw_data);
    }

    closeMySQL();
}
// void Database::GetSettingDisplay() {
//     qDebug() << "GetSettingDisplay:";

//     if (!db.isOpen()) {
//         qDebug() << "Database is not open. Attempting to open...";
//         if (!db.open()) {
//             qDebug() << "Failed to open the database:" << db.lastError().text();
//             return;
//         }
//         qDebug() << "Database opened successfully.";
//     }

//     QSqlQuery query;
//     QString sql = "SELECT SAG, Sampling_rate, DistanceStart, DistanceStop, FullDistance FROM DisplaySetting";
//     QString getPeriodicInfo;
//     if (!query.exec(sql)) {
//         qDebug() << "Failed to execute query:" << query.lastError().text();
//         return;
//     }

//     while (query.next()) {
//         double SAG = query.value("SAG").toDouble();
//         double Sampling_rate = query.value("Sampling_rate").toDouble();
//         double DistanceStart = query.value("DistanceStart").toDouble();
//         double DistanceStop = query.value("DistanceStop").toDouble();
//         double FullDistance = query.value("FullDistance").toDouble();

//         qDebug() << "DEBUG GetSettingDisplay"
//                  << "SAG:" << SAG
//                  << "Sampling_rate:" << Sampling_rate
//                  << "DistanceStart:" << DistanceStart
//                  << "DistanceStop:" << DistanceStop
//                  << "FullDistance:" << FullDistance;

//         // เพิ่มส่วนนี้เพื่ออัพเดตค่า FullDistance ในตาราง TransmissionData
//         QSqlQuery updateQuery;
//         updateQuery.prepare("UPDATE TransmissionData SET FullDistance = :fullDistance");
//         updateQuery.bindValue(":fullDistance", FullDistance);
//         if (!updateQuery.exec()) {
//             qDebug() << "Failed to update FullDistance in TransmissionData:" << updateQuery.lastError().text();
//         } else {
//             qDebug() << "Successfully updated FullDistance in TransmissionData.";
//         }

//         getPeriodicInfo = QString(
//                               "{\"objectName\":\"GetSettingDisplay\", "
//                               "\"sagFactorInit\":%1, "
//                               "\"samplingRateInit\":%2, "
//                               "\"distanceToStartInit\":%3, "
//                               "\"distanceToShowInit\":%4, "
//                               "\"fulldistancesInit\":%5}")
//                               .arg(SAG)
//                               .arg(Sampling_rate)
//                               .arg(DistanceStart)
//                               .arg(DistanceStop)
//                               .arg(FullDistance);

//         emit assignGetSettingDisplay(SAG, Sampling_rate, DistanceStart, DistanceStop, FullDistance);
//     }

//     emit updateThresholdA(getPeriodicInfo);
//     if (!onceTime) {
//         // nothing
//     } else {
//         emit sendToCal(getPeriodicInfo);
//     }

//     GetSag();
//     closeMySQL();
// }

void Database::GetSettingDisplay() {
    qDebug() << "GetSettingDisplay:";

    if (!db.isOpen()) {
        qDebug() << "Database is not open. Attempting to open...";
        if (!db.open()) {
            qDebug() << "Failed to open the database:" << db.lastError().text();
            return;
        }
        qDebug() << "Database opened successfully.";
    }

    // เตรียมคำสั่ง SQL สำหรับดึงข้อมูล
    QSqlQuery query;
    QString sql = "SELECT SAG, Sampling_rate, DistanceStart, DistanceStop, FullDistance FROM DisplaySetting";
    QString getPeriodicInfo;
    if (!query.exec(sql)) {
        qDebug() << "Failed to execute query:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        double SAG = query.value("SAG").toDouble();
        double Sampling_rate = query.value("Sampling_rate").toDouble();
        double DistanceStart = query.value("DistanceStart").toDouble();
        double DistanceStop = query.value("DistanceStop").toDouble();
        double FullDistance = query.value("FullDistance").toDouble();

        qDebug() << "DEBUG GetSettingDisplay" << "SAG:" << SAG << "Sampling_rate:" << Sampling_rate << "DistanceStart:" << DistanceStart << "DistanceStop:" << DistanceStop << "FullDistance:" << FullDistance;

        getPeriodicInfo = QString(
                              "{\"objectName\":\"GetSettingDisplay\", "
                              "\"sagFactorInit\":%1, "
                              "\"samplingRateInit\":%2, "
                              "\"distanceToStartInit\":%3, "
                              "\"distanceToShowInit\":%4, "
                              "\"fulldistancesInit\":%5}")
                              .arg(SAG)
                              .arg(Sampling_rate)
                              .arg(DistanceStart)
                              .arg(DistanceStop)
                              .arg(FullDistance);
        //        if(!onceTime)
        emit assignGetSettingDisplay(SAG, Sampling_rate, DistanceStart, DistanceStop, FullDistance);
    }
    emit cmdmsg(getPeriodicInfo);
    if (!onceTime)
        ;
    else
        emit sendToCal(getPeriodicInfo);

    GetSag();
    closeMySQL();
}
///----------------------recipientEmail------------------------//
void Database::getrecipientEmail() {
    qDebug() << "Fetching all event records from database...";

    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    QSqlQuery query("SELECT id, recipientEmail FROM recipientEmail", db);
    if (!query.exec()) {
        qDebug() << "Failed to execute query:" << query.lastError().text();
        return;
    }
    qDebug() << "Searching for filename:";

    QStringList emailList;

    emailList.clear();

    while (query.next()) {
        QString email = query.value("recipientEmail").toString();
        emailList.append(email);
    }

    QString emailString = emailList.join(",");

    emit updateNewRecipientgmail(emailString);

    qDebug() << "Emails:" << emailString;
    db.close();
}

void Database::RemoveRecipientgmail(QString gmail) {
    qDebug() << "RemoveRecipientgmail" << gmail;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("DELETE FROM recipientEmail WHERE recipientEmail = '%1'").arg(gmail);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    db.close();
    emit getrecipientEmail();
}

void Database::UpdateRecipientgmail(QString gmail, int ID) {
    qDebug() << "UpdateRecipientgmail" << gmail;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("UPDATE recipientEmail SET recipientEmail = '%1' WHERE id = %2").arg(gmail).arg(ID);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    db.close();
    getrecipientEmail();
}

void Database::NewRecipientgmail(QString gmail) {
    qDebug() << "NewRecipientgmail" << gmail;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("INSERT INTO recipientEmail (recipientEmail) VALUES ('%1');").arg(gmail);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    db.close();
    getrecipientEmail();
}

void Database::UpdateuserNameandPassword(QString UserName, QString Password, int UserLevel, int id) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    QSqlQuery query(db);
    QString queryStr = QString("UPDATE users SET username = '%1', password = '%2', userlevel = %3 WHERE id = %4").arg(UserName).arg(Password).arg(UserLevel).arg(id);
    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    qDebug() << "User updated successfully!";
    db.close();
}

void Database::RemoveuserNameandPassword(QString UserName, QString Password, int UserLevel) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("DELETE FROM users WHERE username = '%1' AND password = '%2' AND userlevel = %3").arg(UserName).arg(Password).arg(UserLevel);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    qDebug() << "User removed successfully!";
    db.close();
}

void Database::NewuserNameandPassword(QString UserName, QString Password, int UserLevel) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("INSERT INTO users (username, password, userlevel) VALUES ( '%1', '%2', %3)").arg(UserName).arg(Password).arg(UserLevel);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    db.close();
    qDebug() << "New user added successfully!";
}

void Database::getuserlogin(QString username, QString password, QWebSocket *wClient) {
    int userLevel = getUserLevel(username, password);
    QJsonObject jsonOutput;
    jsonOutput["objectName"] = "userlevel";
    jsonOutput["level"] = userLevel;

    QJsonDocument doc(jsonOutput);
    QString loginuser = doc.toJson(QJsonDocument::Compact);

    qDebug() << "Final JSON Output:" << loginuser;

    emit sendMessage(loginuser, wClient);
    //    emit cmdmsg(loginuser);
}
int Database::getUserLevel(QString username, QString password) {
    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "Database error: Unable to open database!";
            return 3;
        }
    }

    QSqlQuery query;
    query.prepare("SELECT userlevel FROM users WHERE username = :username AND password = :password");
    query.bindValue(":username", username);
    query.bindValue(":password", password);

    if (!query.exec()) {
        qDebug() << "Database error:" << query.lastError().text();
        return 3;
    }

    if (query.next()) {
        int userLevel = query.value(0).toInt();
        qDebug() << "Login successful for user:" << username << "User Level:" << userLevel;
        return userLevel;
    } else {
        qDebug() << "Invalid username or password! Assigning User Level: 3";
        return 3;
    }
    db.close();
}

bool Database::ensureEventAlarmHistoryTimeColumn()
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[ensureEventAlarmHistoryTimeColumn] open db failed:"
                       << db.lastError().text();
            return false;
        }
    }

    QSqlQuery query(db);

    query.prepare(R"(
        SELECT DATA_TYPE,
               DATETIME_PRECISION,
               COLUMN_TYPE,
               IS_NULLABLE
        FROM INFORMATION_SCHEMA.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = 'eventandalarmhistory'
          AND COLUMN_NAME = 'time'
        LIMIT 1
    )");

    if (!query.exec()) {
        qWarning() << "[ensureEventAlarmHistoryTimeColumn] check column failed:"
                   << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qWarning() << "[ensureEventAlarmHistoryTimeColumn] column not found:"
                   << "eventandalarmhistory.time";
        return false;
    }

    const QString dataType = query.value("DATA_TYPE").toString().trimmed().toLower();
    const int precision = query.value("DATETIME_PRECISION").toInt();
    const QString columnType = query.value("COLUMN_TYPE").toString().trimmed().toLower();
    const QString nullable = query.value("IS_NULLABLE").toString().trimmed().toUpper();

    qDebug() << "[ensureEventAlarmHistoryTimeColumn] current:"
             << "DATA_TYPE =" << dataType
             << "DATETIME_PRECISION =" << precision
             << "COLUMN_TYPE =" << columnType
             << "IS_NULLABLE =" << nullable;

    /*
     * MariaDB TIME รองรับ fractional seconds สูงสุด TIME(6)
     * ไม่มี TIME(9)
     */
    if (dataType == "time" && precision >= 6 && nullable == "NO") {
        qDebug() << "[ensureEventAlarmHistoryTimeColumn] already TIME(6) NOT NULL";
        return true;
    }

    QSqlQuery alterQuery(db);

    const QString alterSql =
        "ALTER TABLE eventandalarmhistory "
        "MODIFY `time` TIME(6) NOT NULL";

    if (!alterQuery.exec(alterSql)) {
        qWarning() << "[ensureEventAlarmHistoryTimeColumn] alter failed:"
                   << alterQuery.lastError().text();
        return false;
    }

    qDebug() << "[ensureEventAlarmHistoryTimeColumn] changed to TIME(6) NOT NULL";
    db.close();
    return true;
}

QString Database::normalizeTimeForMariaDbTime6(const QString &input)
{
    QString value = input.trimmed();

    QString mainTime = value;
    QString fraction;

    const int dotIndex = value.indexOf('.');
    if (dotIndex >= 0) {
        mainTime = value.left(dotIndex);
        fraction = value.mid(dotIndex + 1);
    }

    const QStringList parts = mainTime.split(':');
    if (parts.size() != 3) {
        return QString();
    }

    bool okH = false;
    bool okM = false;
    bool okS = false;

    const int h = parts.at(0).toInt(&okH);
    const int m = parts.at(1).toInt(&okM);
    const int s = parts.at(2).toInt(&okS);

    if (!okH || !okM || !okS) {
        return QString();
    }

    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        return QString();
    }

    QString result = QString("%1:%2:%3")
                         .arg(h, 2, 10, QChar('0'))
                         .arg(m, 2, 10, QChar('0'))
                         .arg(s, 2, 10, QChar('0'));

    if (!fraction.isEmpty()) {
        QString onlyDigits;

        for (int i = 0; i < fraction.size(); ++i) {
            if (fraction.at(i).isDigit()) {
                onlyDigits.append(fraction.at(i));
            }
        }

        onlyDigits = onlyDigits.left(6);

        while (onlyDigits.size() < 6) {
            onlyDigits.append('0');
        }

        result += "." + onlyDigits;
    } else {
        result += ".000000";
    }

    return result;
}

void Database::insertEventAlarmHistory(QStringList d,
                                       QStringList t,
                                       QStringList event_name,
                                       QStringList status)
{
    qDebug() << "[insertEventAlarmHistory] called"
             << "d =" << d.size()
             << "t =" << t.size()
             << "event_name =" << event_name.size()
             << "status =" << status.size();

    if (d.isEmpty() || t.isEmpty() || event_name.isEmpty() || status.isEmpty()) {
        // qWarning() << "[insertEventAlarmHistory] skip: one or more lists are empty";
        return;
    }

    if (d.size() != t.size() ||
        d.size() != event_name.size() ||
        d.size() != status.size()) {
        // qWarning() << "[insertEventAlarmHistory] skip: list size mismatch"
        //            << "d =" << d.size()
        //            << "t =" << t.size()
        //            << "event_name =" << event_name.size()
        //            << "status =" << status.size();
        return;
    }

    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[insertEventAlarmHistory] Database open failed:"
                       << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);

    const QString sql =
        "INSERT INTO eventandalarmhistory "
        "(`date`, `time`, `event_name`, `status`) "
        "VALUES (:date, :time, :event_name, :status)";

    for (int i = 0; i < d.size(); ++i) {

        QString dateValue = d.at(i).trimmed();
        QString timeValue = t.at(i).trimmed();

        QDate date = QDate::fromString(dateValue, "dd/MM/yyyy");
        if (!date.isValid()) {
            date = QDate::fromString(dateValue, "yyyy-MM-dd");
        }

        if (!date.isValid()) {
            qWarning() << "[insertEventAlarmHistory] invalid date at index" << i
                       << "raw date =" << dateValue;
            continue;
        }

        QString timeForDb = normalizeTimeForMariaDbTime6(timeValue);

        if (timeForDb.isEmpty()) {
            qWarning() << "[insertEventAlarmHistory] invalid time at index" << i
                       << "raw time =" << timeValue;
            continue;
        }

        query.prepare(sql);
        query.bindValue(":date", date.toString("yyyy-MM-dd"));
        query.bindValue(":time", timeForDb);
        query.bindValue(":event_name", event_name.at(i).trimmed());
        query.bindValue(":status", status.at(i).trimmed());

        if (!query.exec()) {
            qWarning() << "[insertEventAlarmHistory] insert failed index" << i
                       << "error =" << query.lastError().text()
                       << "query =" << query.lastQuery();
        } else {
            qDebug() << "[insertEventAlarmHistory] inserted index" << i
                     << "lastInsertId =" << query.lastInsertId()
                     << "date =" << date.toString("yyyy-MM-dd")
                     << "time =" << timeForDb
                     << "event =" << event_name.at(i).trimmed()
                     << "status =" << status.at(i).trimmed();
        }
    }

    db.close();
}

void Database::getEventAlarmHistory()
{
    if (!db.isOpen()) {
        // qWarning() << "[getEventAlarmHistory] Database is not open! Attempting to reconnect...";

        if (!db.open()) {
            qWarning() << "[getEventAlarmHistory] Failed to reconnect database:"
                       << db.lastError().text();
            return;
        }
    }

    /*
     * ส่ง clear ก่อนทุกครั้ง
     * เพื่อให้หน้า Monitor/Web ล้างรายการเก่า ไม่ค้างข้อมูลเดิม
     */
    {
        QJsonObject clearObj;
        clearObj.insert("objectName", "EventAlarmHistoryClear");

        const QString clearData = QString::fromUtf8(
            QJsonDocument(clearObj).toJson(QJsonDocument::Compact)
            );

        emit cmdmsg(clearData);
    }

    QSqlQuery query(db);

    /*
     * ห้าม SELECT `time` ตรง ๆ กับ TIME(6)
     * เพราะ QMYSQL บาง version คืนค่าเป็น binary/control chars
     * ให้ MariaDB format เป็น string ก่อน
     */
    const QString sql = R"(
        SELECT *
        FROM (
            SELECT
                id,
                DATE_FORMAT(`date`, '%d/%m/%Y') AS date_text,
                TIME_FORMAT(`time`, '%H:%i:%s.%f') AS time_text,
                event_name,
                status
            FROM eventandalarmhistory
            ORDER BY id DESC
            LIMIT 100
        ) AS latest_records
        ORDER BY id ASC
    )";

    if (!query.exec(sql)) {
        qWarning() << "[getEventAlarmHistory] Query execution failed:"
                   << query.lastError().text();
        db.close();
        return;
    }

    while (query.next()) {
        const int id = query.value("id").toInt();

        QString date = query.value("date_text").toString().trimmed();
        QString time = query.value("time_text").toString().trimmed();
        QString eventName = query.value("event_name").toString().trimmed();
        QString eventStatus = query.value("status").toString().trimmed();

        /*
         * ถ้า TIME(6) เป็น .000000 แล้วไม่อยากแสดง microsecond ให้เปิดใช้
         * แต่ถ้าต้องการแสดงครบ ให้ comment ไว้แบบนี้ถูกแล้ว
         */
        // if (time.endsWith(".000000")) {
        //     time.chop(7);
        // }

        QString mainTime = time.section('.', 0, 0);
        QString frac = time.section('.', 1, 1);

        while (frac.size() < 9) {
            frac.append('0');
        }

        if (frac.size() > 9) {
            frac = frac.left(9);
        }

        time = mainTime + "." + frac;

        /*
         * Normalize status เผื่อใน DB เคยเก็บเป็น true/false/1/0
         */
        QString statusText = eventStatus.toUpper();
        QString fullDateTime = date + " " + time;
        bool status = false;
        if (statusText == "ACTIVE") {
            status = true;
        } else if (statusText == "DEACTIVE") {
            status = false;
        }

        QJsonObject param;
        param.insert("TrapsAlert", eventName);
        param.insert("state", status);
        param.insert("chanel", "OLD");
        param.insert("time", fullDateTime);
        const QString rawData = QString::fromUtf8(
            QJsonDocument(param).toJson(QJsonDocument::Compact)
            );

        // qWarning() << "FUNCTION getEventAlarmHistory -->" << rawData;

        emit cmdmsg(rawData);
    }
    db.close();
}

void Database::updateTableDataTagging(int No, double Distance, QString Detail) {
    //    qDebug() << "updateTableDataTagging______"<<No<<Distance<<Detail;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("UPDATE DataTagging   SET `Distance(Km)` = %1 , `Detail` = '%2'   WHERE `No` = %3").arg(Distance).arg(Detail).arg(No);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    db.close();
    qDebug() << "User removed successfully!";
    getMyTaggingPhaseA();
    getMyTaggingPhaseB();
    getMyTaggingPhaseC();
}

void Database::deleteTableDataTagging(int No) {
    qDebug() << "deleteTableDataTagging" << No;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("DELETE FROM DataTagging WHERE `No` = %1").arg(No);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    db.close();
    qDebug() << "User removed successfully!";
    getMyTaggingPhaseA();
    getMyTaggingPhaseB();
    getMyTaggingPhaseC();
}

void Database::updateSetupEquipment(SetupParameterEquipment *e) {
    *SetupEquipment = *e;

    qDebug() << "updateSetupEquipment towerAndDistance->direction" << towerAndDistance->direction
             << " towerAndDistance->linenumber" << towerAndDistance->linenumber
             << " towerAndDistance->substation" << towerAndDistance->substation
             << " e->TransmissionLineName" << e->TransmissionLineName
             << " e->LFLSerialNo" << e->LFLSerialNo
             << " e->SubstationName" << e->SubstationName;

    QString query = QString(
                        "UPDATE towerAndDistance SET voltage=%1,substation='%2',direction='%3',linenumber=%4")
                        .arg(SetupEquipment->Voltage)
                        .arg(SetupEquipment->SubstationName)
                        .arg(SetupEquipment->TransmissionLineName)
                        .arg(SetupEquipment->LFLSerialNo);
    //        qDebug() << query;
    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        emit mysqlError();
        return;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
        printf("***********************SQL Error*****************\n%s\n", query.toStdString().c_str());
    }

    towerAndDistance->direction = e->TransmissionLineName;
    towerAndDistance->linenumber = e->LFLSerialNo;
    towerAndDistance->substation = e->SubstationName;

    db.close();


}

void Database::setNTPServer(QString ntp){
    if (!db.open())
    {
        qWarning() << "c++: ERROR! " << "database error! database can not open.";
        emit mysqlError();
        return;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE SettingNetwork SET sychonizationserver = :ntp");
    updateQuery.bindValue(":ntp", ntp);
    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
        db.close();
        return;
    }
    qDebug() << "Updated setNTPServer to" << ntp;


    db.close();
}

void Database::updataListOfMarginANotObject(int no, int valueOfMargin)
{
    if (!db.open())
    {
        qWarning() << "c++: ERROR!" << "database error! database can not open.";
        emit mysqlError();
        return;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE `MarginTableA` "
                        "SET `value of margin` = :valueOfMargin "
                        "WHERE `No` = :no");

    updateQuery.bindValue(":valueOfMargin", valueOfMargin);
    updateQuery.bindValue(":no", no);

    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
        db.close();
        return;
    }

    qDebug() << "Updated value of margin to" << valueOfMargin
             << "for No =" << no;

    db.close();
}

void Database::updataListOfMarginBNotObject(int no, int valueOfMargin)
{
    if (!db.open())
    {
        qWarning() << "c++: ERROR!" << "database error! database can not open.";
        emit mysqlError();
        return;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE `MarginTableB` "
                        "SET `value of margin` = :valueOfMargin "
                        "WHERE `No` = :no");

    updateQuery.bindValue(":valueOfMargin", valueOfMargin);
    updateQuery.bindValue(":no", no);

    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
        db.close();
        return;
    }

    qDebug() << "Updated value of margin to" << valueOfMargin
             << "for No =" << no;

    db.close();
}

void Database::updataListOfMarginCNotObject(int no, int valueOfMargin)
{
    if (!db.open())
    {
        qWarning() << "c++: ERROR!" << "database error! database can not open.";
        emit mysqlError();
        return;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE `MarginTableC` "
                        "SET `value of margin` = :valueOfMargin "
                        "WHERE `No` = :no");

    updateQuery.bindValue(":valueOfMargin", valueOfMargin);
    updateQuery.bindValue(":no", no);

    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
        db.close();
        return;
    }

    qDebug() << "Updated value of margin to" << valueOfMargin
             << "for No =" << no;

    db.close();
}

void Database::resetAllMarginTablesValueToZero()
{
    if (!db.open())
    {
        qWarning() << "c++: ERROR!" << "database error! database can not open.";
        emit mysqlError();
        return;
    }

    QStringList tableList;
    tableList << "MarginTableA" << "MarginTableB" << "MarginTableC";

    for (const QString &tableName : tableList)
    {
        QSqlQuery updateQuery(db);
        QString queryString = QString("UPDATE `%1` "
                                      "SET `value of margin` = 0").arg(tableName);

        updateQuery.prepare(queryString);

        if (!updateQuery.exec()) {
            qWarning() << "Update failed in" << tableName << ":" << updateQuery.lastError().text();
            db.close();
            return;
        }

        qDebug() << "Reset `value of margin` = 0 in table" << tableName;
    }

    db.close();
}

void Database::updateMarginSettingParameter(int margin, int valueVoltage, int focusIndex, QString phase)
{
    if (!db.open())
    {
        qWarning() << "c++: ERROR!" << "database error! database can not open.";
        emit mysqlError();
        return;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE `MarginSettingParameter` "
                        "SET `margin` = :margin, "
                        "    `valueVoltage` = :valueVoltage, "
                        "    `focusIndex` = :focusIndex "
                        "WHERE `PHASE` = :phase");

    updateQuery.bindValue(":margin", margin);
    updateQuery.bindValue(":valueVoltage", valueVoltage);
    updateQuery.bindValue(":focusIndex", focusIndex);
    updateQuery.bindValue(":phase", phase.trimmed().toUpper());

    if (!updateQuery.exec()) {
        qWarning() << "Update MarginSettingParameter failed:" << updateQuery.lastError().text();
        db.close();
        return;
    }

    qDebug() << "Updated MarginSettingParameter:"
             << "PHASE =" << phase
             << "margin =" << margin
             << "valueVoltage =" << valueVoltage
             << "focusIndex =" << focusIndex;

    db.close();
}
